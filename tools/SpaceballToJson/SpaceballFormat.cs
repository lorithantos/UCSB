using System.Buffers.Binary;
using System.Text;

namespace Spaceball;

/// <summary>
/// On-disk data types. The stored value is the ORDINAL, so this enum is a permanent
/// contract with every archived file. Append only; never reorder.
/// Mirrors enum DataType in Include/UCSB-Datastream.h.
/// Suffix _I = Intel (little-endian), _M = Motorola (big-endian).
/// </summary>
public enum DataType
{
    TEXT_CHAR = 0,
    UCHAR     = 1,
    SCHAR     = 2,
    USHORT_I  = 3,
    USHORT_M  = 4,
    SSHORT_I  = 5,
    SSHORT_M  = 6,
    ULONG_I   = 7,
    ULONG_M   = 8,
    SLONG_I   = 9,
    SLONG_M   = 10,
    SPFLOAT_I = 11,
    SPFLOAT_M = 12,
    DPFLOAT_I = 13,
    DPFLOAT_M = 14,
    UINT64_I  = 15,
}

/// <summary>Message identifiers. Mirrors enum MESSAGE_TYPE.</summary>
public static class MessageType
{
    public const byte TelemetryEol      = 0x0F;
    public const byte FirstDeviceType   = 0x10;
    public const byte LastDeviceType    = 0xEF;
    public const byte DeviceDefinition  = 0xF0;
    public const byte ChannelDefinition = 0xF1;
    public const byte BlockDefinition    = 0xF2;  // declared but never implemented by the writer
}

public static class Format
{
    /// <summary>Unframed 8-byte file magic. Not an LN-250 message; only meaningful at offset 0.</summary>
    public static ReadOnlySpan<byte> Magic => "SPACBALL"u8;

    public const byte SyncByte = 0xCA;
    public const int HeaderSize = 4;            // SOH, MessageID, ByteCount, Checksum
    public const int FixedStringLength = 30;    // char[30], not necessarily NUL-terminated
    public const int GuidLength = 16;

    /// <summary>DeviceDefinition: GUID + char[30] + deviceNumber + channelCount, pack(1).</summary>
    public const int DeviceDefinitionSize = GuidLength + FixedStringLength + 1 + 1;

    /// <summary>ChannelDefinition: GUID + char[30] + channelIndex + dataType, pack(1).</summary>
    public const int ChannelDefinitionSize = GuidLength + FixedStringLength + 1 + 1;

    public static int SizeOf(DataType t) => t switch
    {
        DataType.TEXT_CHAR or DataType.UCHAR or DataType.SCHAR => 1,
        DataType.USHORT_I or DataType.USHORT_M
            or DataType.SSHORT_I or DataType.SSHORT_M => 2,
        DataType.ULONG_I or DataType.ULONG_M
            or DataType.SLONG_I or DataType.SLONG_M
            or DataType.SPFLOAT_I or DataType.SPFLOAT_M => 4,
        DataType.DPFLOAT_I or DataType.DPFLOAT_M or DataType.UINT64_I => 8,
        _ => 0,
    };

    public static bool IsKnown(DataType t) => t >= DataType.TEXT_CHAR && t <= DataType.UINT64_I;

    /// <summary>
    /// Fixed-width char[30] field. The writer pads with NULs but does not guarantee
    /// termination, so read the whole field and cut at the first NUL.
    /// </summary>
    public static string ReadFixedString(ReadOnlySpan<byte> field)
    {
        int end = field.IndexOf((byte)0);
        if (end < 0) end = field.Length;
        return Encoding.ASCII.GetString(field[..end]).TrimEnd();
    }

    /// <summary>
    /// Windows GUID layout: {DWORD, WORD, WORD, BYTE[8]} with the first three fields
    /// little-endian. This is exactly what Guid(ReadOnlySpan&lt;byte&gt;) expects, so it
    /// is NOT a flat big-endian UUID.
    /// </summary>
    public static Guid ReadGuid(ReadOnlySpan<byte> bytes) => new(bytes[..GuidLength]);

    /// <summary>
    /// Decode one channel value. Returns a boxed CLR value suitable for JSON.
    /// _M types are byte-reversed relative to the host.
    /// </summary>
    public static object? ReadValue(DataType t, ReadOnlySpan<byte> b) => t switch
    {
        DataType.TEXT_CHAR => ((char)b[0]).ToString(),
        DataType.UCHAR     => b[0],
        DataType.SCHAR     => (sbyte)b[0],

        DataType.USHORT_I  => BinaryPrimitives.ReadUInt16LittleEndian(b),
        DataType.USHORT_M  => BinaryPrimitives.ReadUInt16BigEndian(b),
        DataType.SSHORT_I  => BinaryPrimitives.ReadInt16LittleEndian(b),
        DataType.SSHORT_M  => BinaryPrimitives.ReadInt16BigEndian(b),

        DataType.ULONG_I   => BinaryPrimitives.ReadUInt32LittleEndian(b),
        DataType.ULONG_M   => BinaryPrimitives.ReadUInt32BigEndian(b),
        DataType.SLONG_I   => BinaryPrimitives.ReadInt32LittleEndian(b),
        DataType.SLONG_M   => BinaryPrimitives.ReadInt32BigEndian(b),

        DataType.SPFLOAT_I => BinaryPrimitives.ReadSingleLittleEndian(b),
        DataType.SPFLOAT_M => BinaryPrimitives.ReadSingleBigEndian(b),
        DataType.DPFLOAT_I => BinaryPrimitives.ReadDoubleLittleEndian(b),
        DataType.DPFLOAT_M => BinaryPrimitives.ReadDoubleBigEndian(b),

        DataType.UINT64_I  => BinaryPrimitives.ReadUInt64LittleEndian(b),
        _ => null,
    };
}

public sealed class ChannelDef
{
    public required byte Index { get; init; }
    public required string Name { get; init; }
    public required DataType Type { get; init; }
    public int Bytes => Format.SizeOf(Type);
}

public sealed class DeviceDef
{
    public required byte DeviceNumber { get; init; }
    public required Guid DeviceId { get; init; }
    public required string Name { get; init; }
    public required byte DeclaredChannelCount { get; init; }
    public List<ChannelDef> Channels { get; } = [];

    /// <summary>
    /// Expected payload size: the sum of every channel's width. Channels 0 and 1 are
    /// normally computerClock (UINT64_I) and index (UCHAR) — the writer wraps each device
    /// in timedT&lt;T&gt;, which prepends them to the declared channel list, so they are
    /// ordinary channels here and need no special handling.
    /// </summary>
    public int DataSize => Channels.Sum(c => c.Bytes);
}
