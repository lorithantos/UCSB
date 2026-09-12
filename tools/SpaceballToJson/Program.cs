using System.Text.Json;
using System.Text.Json.Serialization;
using Spaceball;

const string Usage = """
spaceball2json — convert a .spaceball recording to JSON

  spaceball2json <input.spaceball> [options]

Options
  -o, --out <file>     write JSON to a file (default: stdout)
  -n, --max <count>    emit at most <count> data records (default: all)
      --no-records     dictionary and diagnostics only
      --compact        single-line JSON (default: indented)
  -h, --help           this text

The dictionary is read from the stream itself, so no schema is needed. Channels 0
and 1 of every device are computerClock (the CPU cycle counter stamped at the tick)
and index (which instance of that device produced the sample).
""";

string? input = null, output = null;
int maxRecords = 0;
bool noRecords = false, indent = true;

for (int i = 0; i < args.Length; i++)
{
    switch (args[i])
    {
        case "-h" or "--help":
            Console.WriteLine(Usage);
            return 0;
        case "-o" or "--out" when i + 1 < args.Length:
            output = args[++i];
            break;
        case "-n" or "--max" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out maxRecords))
            {
                Console.Error.WriteLine($"Not a number: {args[i]}");
                return 2;
            }
            break;
        case "--no-records":
            noRecords = true;
            break;
        case "--compact":
            indent = false;
            break;
        default:
            if (args[i].StartsWith('-'))
            {
                Console.Error.WriteLine($"Unknown option: {args[i]}");
                return 2;
            }
            input ??= args[i];
            break;
    }
}

if (input is null)
{
    Console.Error.WriteLine(Usage);
    return 2;
}

if (!File.Exists(input))
{
    Console.Error.WriteLine($"No such file: {input}");
    return 1;
}

byte[] bytes = File.ReadAllBytes(input);
var reader = new SpaceballReader();

// The reader streams, but records are materialised here so the dictionary is complete
// before serialisation — definitions can legitimately arrive after data on a downlink.
// With --no-records we still walk the whole stream, so the diagnostics stay honest.
List<Record> records = [];
long decoded = 0;

foreach (Record r in reader.Read(bytes, noRecords ? 0 : maxRecords))
{
    decoded++;
    if (!noRecords) records.Add(r);
}

var result = new
{
    file = Path.GetFileName(input),
    path = Path.GetFullPath(input),
    magic = reader.Diag.MagicPresent ? "SPACBALL" : null,
    dictionary = reader.Dictionary.Select(d => new
    {
        deviceNumber = d.DeviceNumber,
        messageId = $"0x{d.DeviceNumber:X2}",
        name = d.Name,
        guid = d.DeviceId.ToString("D").ToUpperInvariant(),
        declaredChannelCount = d.DeclaredChannelCount,
        recordBytes = d.DataSize,
        channels = d.Channels.OrderBy(c => c.Index).Select(c => new
        {
            index = c.Index,
            name = c.Name,
            type = c.Type.ToString(),
            bytes = c.Bytes,
        }),
    }),
    decodedRecords = decoded,
    emittedRecords = records.Count,
    records = noRecords ? null : records.Select(r => new
    {
        device = r.Device,
        deviceNumber = r.DeviceNumber,
        values = r.Values,
    }),
    diagnostics = new
    {
        fileBytes = reader.Diag.FileBytes,
        magicPresent = reader.Diag.MagicPresent,
        dictionaryMessages = reader.Diag.DictionaryMessages,
        dataMessages = reader.Diag.DataMessages,
        controlMessages = reader.Diag.ControlMessages,
        controlIds = reader.Diag.ControlIds,
        deadBytes = reader.Diag.DeadBytes,
        badHeaderChecksum = reader.Diag.BadHeaderChecksum,
        badPayloadChecksum = reader.Diag.BadPayloadChecksum,
        truncatedTail = reader.Diag.TruncatedTail,
        unknownDevice = reader.Diag.UnknownDevice,
        sizeMismatch = reader.Diag.SizeMismatch,
    },
};

var options = new JsonSerializerOptions
{
    WriteIndented = indent,
    DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
};

string json = JsonSerializer.Serialize(result, options);

if (output is null) Console.WriteLine(json);
else
{
    File.WriteAllText(output, json);
    Console.Error.WriteLine(
        $"{Path.GetFileName(input)}: {decoded:N0} records decoded, " +
        $"{reader.Dictionary.Count()} devices -> {output}");
}

return 0;
