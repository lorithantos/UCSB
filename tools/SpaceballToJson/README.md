# spaceball2json

Converts a `.spaceball` recording to JSON. Written against
[`FILE-FORMAT.md`](../../FILE-FORMAT.md) as an independent check that the format
documentation is correct — it reads nothing but the bytes, and builds its own dictionary
from the stream.

```
dotnet run -- <input.spaceball> [options]

  -o, --out <file>     write JSON to a file (default: stdout)
  -n, --max <count>    emit at most <count> data records
      --no-records     dictionary and diagnostics only
      --compact        single-line JSON
```

## What it validates

Every parse reports diagnostics, and all of them should be zero on an intact file:

| Counter | Meaning |
|---|---|
| `deadBytes` | bytes discarded while hunting the `0xCA` sync byte |
| `badHeaderChecksum` | a `0xCA` that was payload data, not a header |
| `badPayloadChecksum` | a message whose body failed its checksum |
| `truncatedTail` | trailing bytes too short to form a message |
| `unknownDevice` | data for a device number with no dictionary entry |
| `sizeMismatch` | payload length disagreeing with the declared channel widths |

Result against the seven recordings in `FlightCode-01\20121220\` (13,000+ records):
**every counter zero.**

## Notes on the implementation

- **Resynchronisation follows the original.** On a failed payload checksum it advances a
  single byte rather than skipping `ByteCount`, because a dropped byte makes the length
  field itself untrustworthy. This mirrors `ChunkInputData` in `Include\LN250.h`.
- **The dictionary comes from the stream.** Device and channel definitions are re-emitted
  at the head of every file, so no schema is needed and a repeated definition replaces
  rather than duplicates.
- **Channels 0 and 1 need no special case.** The writer wraps each device in
  `timedT<T>`, which prepends `computerClock` (the CPU cycle counter stamped at the tick)
  and `index` (which instance of the device produced the sample) to the declared channel
  list. They arrive as ordinary channels.
- **GUIDs use the Windows layout** — first three fields little-endian — which is exactly
  what `System.Guid(ReadOnlySpan<byte>)` expects. They are not flat big-endian UUIDs.
- **`_M` types are byte-reversed.** Both orderings appear in the type enum; the LN-250's
  channels are all Motorola order.

## Worked example

`ClockSync` publishes wall clock while every record header carries the CPU cycle counter,
so one `ClockSync` record is a matched calibration pair. From `171849.spaceball`:

```
computerClock  283640224046200      filetime  130005263303716576
computerClock  283645810373100      filetime  130005263323717720
```

`filetime` is a Windows `FILETIME`; the first decodes to **2012-12-20 17:18:50.371 local**,
one second after the file's own name (`171849`). Dividing the cycle delta by the wall-clock
delta gives **2.793 GHz** — the recording machine's CPU frequency, recovered from the data.
