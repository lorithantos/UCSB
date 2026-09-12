namespace Spaceball;

public sealed class Diagnostics
{
    public long FileBytes { get; set; }
    public bool MagicPresent { get; set; }
    public long DeadBytes { get; set; }
    public long BadHeaderChecksum { get; set; }
    public long BadPayloadChecksum { get; set; }
    public long TruncatedTail { get; set; }
    public long ControlMessages { get; set; }
    public long DictionaryMessages { get; set; }
    public long DataMessages { get; set; }
    public long UnknownDevice { get; set; }
    public long SizeMismatch { get; set; }
    public SortedDictionary<string, long> ControlIds { get; } = [];
}

public sealed class Record
{
    public required byte DeviceNumber { get; init; }
    public required string Device { get; init; }
    public required Dictionary<string, object?> Values { get; init; }
}

/// <summary>
/// Reads a Spaceball stream. Framing is the LN-250 high-speed port protocol; the
/// resynchronisation discipline below mirrors ChunkInputData in Include/LN250.h,
/// including its refusal to trust ByteCount after a failed payload checksum.
/// </summary>
public sealed class SpaceballReader
{
    private readonly Dictionary<byte, DeviceDef> _byNumber = [];
    private readonly Dictionary<Guid, DeviceDef> _byGuid = [];

    public Diagnostics Diag { get; } = new();
    public IEnumerable<DeviceDef> Dictionary => _byNumber.Values.OrderBy(d => d.DeviceNumber);

    /// <summary>Sum of a span, modulo 256. A valid block sums to zero.</summary>
    private static byte Sum(ReadOnlySpan<byte> b)
    {
        byte s = 0;
        foreach (byte x in b) s += x;
        return s;
    }

    public IEnumerable<Record> Read(byte[] data, int maxRecords)
    {
        Diag.FileBytes = data.Length;

        int i = 0;
        if (data.Length >= Format.Magic.Length &&
            data.AsSpan(0, Format.Magic.Length).SequenceEqual(Format.Magic))
        {
            // The magic is raw bytes, not a framed message. Skipping it is optional —
            // the scan below would discard it as dead bytes anyway.
            Diag.MagicPresent = true;
            i = Format.Magic.Length;
        }

        int emitted = 0;

        while (i < data.Length)
        {
            if (data[i] != Format.SyncByte)
            {
                Diag.DeadBytes++;
                i++;
                continue;
            }

            // Need a header plus at least one payload/checksum byte to proceed.
            if (data.Length - i < Format.HeaderSize + 1)
            {
                Diag.TruncatedTail += data.Length - i;
                break;
            }

            // A valid header is four bytes summing to zero. If it does not, this 0xCA
            // was payload data — advance ONE byte, never four.
            if (Sum(data.AsSpan(i, Format.HeaderSize)) != 0)
            {
                Diag.BadHeaderChecksum++;
                Diag.DeadBytes++;
                i++;
                continue;
            }

            byte messageId = data[i + 1];
            int byteCount = data[i + 2];

            int payloadStart = i + Format.HeaderSize;
            if (data.Length - payloadStart < byteCount + 1)
            {
                Diag.TruncatedTail += data.Length - i;
                break;
            }

            // Payload plus its trailing checksum byte must also sum to zero. On failure we
            // deliberately do NOT skip byteCount bytes: if a byte was dropped, byteCount is
            // itself suspect and skipping would compound the desynchronisation.
            if (Sum(data.AsSpan(payloadStart, byteCount + 1)) != 0)
            {
                Diag.BadPayloadChecksum++;
                Diag.DeadBytes++;
                i++;
                continue;
            }

            ReadOnlySpan<byte> payload = data.AsSpan(payloadStart, byteCount);

            Record? record = Dispatch(messageId, payload);
            if (record is not null)
            {
                emitted++;
                if (maxRecords <= 0 || emitted <= maxRecords)
                    yield return record;
            }

            i = payloadStart + byteCount + 1;
        }
    }

    private Record? Dispatch(byte messageId, ReadOnlySpan<byte> payload)
    {
        switch (messageId)
        {
            case MessageType.DeviceDefinition:
                Diag.DictionaryMessages++;
                AddDevice(payload);
                return null;

            case MessageType.ChannelDefinition:
                Diag.DictionaryMessages++;
                AddChannel(payload);
                return null;
        }

        if (messageId < MessageType.FirstDeviceType)
        {
            Diag.ControlMessages++;
            string key = $"0x{messageId:X2}";
            Diag.ControlIds[key] = Diag.ControlIds.GetValueOrDefault(key) + 1;
            return null;
        }

        if (messageId > MessageType.LastDeviceType)
        {
            // 0xF2 BLOCK_DEFINITION and anything else above the device range.
            Diag.DictionaryMessages++;
            return null;
        }

        return ReadData(messageId, payload);
    }

    private void AddDevice(ReadOnlySpan<byte> p)
    {
        if (p.Length != Format.DeviceDefinitionSize) return;

        var device = new DeviceDef
        {
            DeviceId = Format.ReadGuid(p),
            Name = Format.ReadFixedString(p.Slice(Format.GuidLength, Format.FixedStringLength)),
            DeviceNumber = p[Format.GuidLength + Format.FixedStringLength],
            DeclaredChannelCount = p[Format.GuidLength + Format.FixedStringLength + 1],
        };

        // A repeated definition is normal: the dictionary is re-emitted at the head of
        // every file and once a minute on the downlink. Replace rather than duplicate.
        _byNumber[device.DeviceNumber] = device;
        _byGuid[device.DeviceId] = device;
    }

    private void AddChannel(ReadOnlySpan<byte> p)
    {
        if (p.Length != Format.ChannelDefinitionSize) return;

        Guid owner = Format.ReadGuid(p);
        if (!_byGuid.TryGetValue(owner, out DeviceDef? device)) return;

        var channel = new ChannelDef
        {
            Name = Format.ReadFixedString(p.Slice(Format.GuidLength, Format.FixedStringLength)),
            Index = p[Format.GuidLength + Format.FixedStringLength],
            Type = (DataType)p[Format.GuidLength + Format.FixedStringLength + 1],
        };

        // Channels are keyed on the parent device's GUID, not its number, so a channel
        // definition is identical however devices were ordered. On index collision the
        // most recent wins, per the format's own rule.
        int existing = device.Channels.FindIndex(c => c.Index == channel.Index);
        if (existing >= 0) device.Channels[existing] = channel;
        else device.Channels.Add(channel);
    }

    private Record? ReadData(byte messageId, ReadOnlySpan<byte> payload)
    {
        Diag.DataMessages++;

        if (!_byNumber.TryGetValue(messageId, out DeviceDef? device) || device.Channels.Count == 0)
        {
            // Expected before the first dictionary arrives, e.g. joining a downlink
            // mid-stream. Not an error until a dictionary has been seen.
            Diag.UnknownDevice++;
            return null;
        }

        if (device.DataSize != payload.Length)
        {
            Diag.SizeMismatch++;
            return null;
        }

        var values = new Dictionary<string, object?>(device.Channels.Count);
        int offset = 0;
        foreach (ChannelDef c in device.Channels.OrderBy(c => c.Index))
        {
            int width = c.Bytes;
            if (width == 0 || offset + width > payload.Length) break;
            values[c.Name] = Format.ReadValue(c.Type, payload.Slice(offset, width));
            offset += width;
        }

        return new Record
        {
            DeviceNumber = device.DeviceNumber,
            Device = device.Name,
            Values = values,
        };
    }
}
