# The Spaceball stream format

**A self-describing, self-synchronizing binary stream: a reader can start at any byte offset,
recover message framing within a few bytes, and become fully able to interpret the data within
one dictionary-repeat interval — at most 60 seconds on the telemetry link.**

That property is the whole design. It is what makes the format survive a ground-station restart,
a dropped radio link, a truncated file, or a recording that begins mid-flight. Everything below
follows from it.

Companion to [`ARCHITECTURE.md`](ARCHITECTURE.md), which covers how the program that writes this
stream is assembled, [`CONFIGURATION.md`](CONFIGURATION.md) for what was recorded, and
[`DEVICES.md`](DEVICES.md) for what each channel means.

> Describes the **December 2012** working copy. Seven real recordings made with that build are in
> `FlightCode-01\20121220\` — use them to check any claim here against actual bytes.

---

## The idea in one paragraph

There is no "header" you must have read to understand the rest. The stream is a flat sequence of
small, checksummed messages. Some of those messages *describe instruments* (the dictionary);
the rest *carry samples* and reference a description by a one-byte number. A reader that joins
late does not know what device `0x14` is, so it skips those messages and says so. The writer
re-sends the full dictionary periodically. Once that repeat arrives, every subsequent `0x14`
becomes meaningful, and stays meaningful. Recovery is therefore bounded by the repeat interval,
not by how badly the reader was interrupted.

---

## Layer 1 — Message framing

Framing is borrowed wholesale from the **LN-250 inertial navigation unit's** high-speed port
protocol (`Include\LN250.h`). Every block in the stream — dictionary entries and science data
alike — is an LN-250 message. If you are writing a parser, `LN250.h` is the format spec.

### Why it is borrowed

This is the first thing that looks strange and the thing most worth understanding, because the
reasoning is sound and is not visible from any single file.

**The flight computer had to speak LN-250 framing regardless.** One of the instruments *is* an
LN-250, and reading it means implementing its protocol. That code was going to exist, be
debugged, and be trusted no matter what format the recorder used. Inventing a second framing
layer would have meant building, debugging and maintaining a *second* resynchronizer alongside
it — to obtain properties the first one already had.

So one implementation was pointed at everything. `LN250::ChunkInputData` / `InputBuffer` is the
sole framing reader for all four transports in this system:

| Transport | Where |
|---|---|
| The LN-250 instrument's own serial port | `LN250.cpp:33` — `InputBuffer<LN250Reader&>` |
| The recorded Spaceball archive | `ConversionTool.cpp:448`, `SpaceballUtils\Functionality.cpp:231`, `TestICD.cpp:258` |
| The telemetry downlink, transmit side | `Telemetry.cpp:301` |
| The telemetry downlink, ground side | `TelemetryReceiver.h:15` |
| The live console display | `DisplaySource.cpp:142` |

The seam is clean: `InputBuffer<Handler>` is parameterized by what the messages *mean*.
`LN250Reader` feeds itself as the handler to interpret instrument messages;
everything else supplies `DeviceHolder::FileParser<X>` to interpret Spaceball messages. **Framing
is shared, interpretation is not.** One resync path, debugged once, serving a serial bus, a disk
archive, a radio link in both directions, and a display.

And the borrowed design is genuinely good for the job — see
[resynchronization](#how-resynchronization-actually-works). A sync byte, a self-checking header,
an explicit length, and a payload checksum are exactly the properties a lossy radio link needs.
Re-deriving them from scratch would have been work spent arriving back where the instrument's
designers already were.

**On provenance.** `LN250.h` carries no copyright, licence, or attribution — only a comment
saying it "contains the definitions used to talk to the RS-442 bus on the Inertial Guidance."
The header appears to have been transcribed by hand from the instrument's interface
documentation; the small errors are the tell — "LM-250" for LN-250 in a comment on line 23, and
"RS-442" for what is presumably RS-422. The only files in this tree carrying any licence text at
all are a third-party DAQ vendor header (`Include\cbw.h`) and a .NET assembly-info boilerplate.
Whether adopting the protocol as the project's own container format was ever formally sanctioned
is not recorded anywhere in the source, and the people who built it doubt that it was. Treat the
format as adopted rather than licensed if it ever leaves the archive.

### Message layout

```
┌──────┬───────────┬───────────┬──────────┬─────────────────┬──────────┐
│ 0xCA │ MessageID │ ByteCount │ Checksum │ payload         │ Checksum │
│  1   │     1     │     1     │    1     │   ByteCount     │    1     │
└──────┴───────────┴───────────┴──────────┴─────────────────┴──────────┘
   └──────────── header, 4 bytes ────────────┘
```

`HighSpeedPortMessageHeader` — `LN250.h:43`

| Field | Meaning |
|---|---|
| `StartOfHeader` | Always `0xCA` (`StartOfHeaderConst`, `LN250.h:24`). The sync byte. |
| `MessageID` | What this message is. See [the ID space](#layer-2--the-message-id-space). |
| `ByteCount` | Payload length. **Excludes** the 4-byte header and the trailing checksum byte. |
| `Checksum` | Two's complement over the header, such that the four header bytes sum to zero. |

Total message length is `ByteCount + 5`. Everything is `#pragma pack(1)`; there is no padding
anywhere in this format.

**Two independent checksums.** The header carries its own, and the payload is followed by one
more byte chosen so that `payload + checksum` sums to zero. This is what makes a false `0xCA`
inside payload data cheap to reject.

### How resynchronization actually works

Implemented in `ChunkInputData` (`LN250.h:378`), and worth reading because the error handling is
the interesting part:

1. Scan forward byte by byte for `0xCA`. Anything skipped is counted as `numDeadBytes`.
2. Require at least 5 bytes remaining, else **return** and wait for more input.
3. Verify the 4-byte header checksum. If it fails, this was not a header — **advance one byte**
   and keep scanning.
4. Skip the header. If `ByteCount + 1` bytes are not yet available, **return** and wait.
5. Verify the payload checksum. If it fails, **advance one byte and rescan** — do *not* skip
   `ByteCount` bytes.
6. Emit `(MessageID, ByteCount, payload)` to the handler and continue past the message.

Step 5 carries the comment that explains the whole approach:

> *"Bad packet - if it is because of dropped bytes then we don't want to skip because we might
> miss the start of the next packet"*

A corrupt message means `ByteCount` itself may be garbage, so trusting it to find the next
message would compound the error. Falling back to byte-by-byte scanning costs a few wasted bytes
and guarantees the next valid message is found. **Worst-case resync is bounded by the largest
message, not by the size of the damage.**

The "return and wait" behaviour at steps 2 and 4 is what makes this restartable across buffer
boundaries. `InputBuffer` (`LN250.h:441`) wraps it: feed it arbitrary chunks as they arrive off
a serial port or a file read, it tracks a `highWaterMark` of consumed bytes and re-runs the
scan as more arrives.

### One wrinkle worth knowing

Files begin with the 8 raw bytes `SPACBALL` (`UCSB-DataStream.cpp:15`) — **not** wrapped in a
message. A parser that starts at byte 0 can `memcmp` it as a format check, which is what the
conversion tools do (`ConversionTool.cpp:421`). A resynchronizing parser simply discards those
8 bytes as dead bytes while hunting for `0xCA`. Both behaviours are correct; do not expect the
magic to be findable mid-stream.

---

## Layer 2 — The message ID space

`enum MESSAGE_TYPE` — `UCSB-Datastream.h:136`

| Range | Meaning |
|---|---|
| `0x00`–`0x0E` | Control messages. A reader hands these to `ControlData(messageID)`. |
| `0x0F` | `TELEMETRY_EOL` — end-of-transmission-group marker on the downlink. |
| `0x10`–`0xEF` | **Device data.** The `MessageID` *is* the device number. |
| `0xF0` | `DEVICE_DEFINITION` |
| `0xF1` | `CHANNEL_DEFINITION` |
| `0xF2` | `BLOCK_DEFINITION` — **declared but never implemented.** See [below](#0xf2-the-third-record-type-that-was-never-built). |

The identity `MessageID == deviceNumber` for `0x10`–`0xEF` is the pivot of the whole design.
Device numbers are handed out from `FIRST_DEVICE_TYPE` upward (`DeviceHolder`'s
`m_NextDeviceID` starts at `0x10`, `UCSB-Datastream.h:546`), giving 224 possible devices.

Dictionary messages sit at the *top* of the space, deliberately out of the way of the data
numbering.

---

## Layer 3 — The dictionary

Two record types describe the instruments. Both are `#pragma pack(1)`.

### `DeviceDefinition` — `UCSB-Datastream.h:149`, sent as `0xF0`

| Field | Size | Notes |
|---|---|---|
| `deviceID` | 16 | `GUID`. The device's permanent identity. |
| `displayName` | 30 | `char[30]`, human-readable. Not used for anything else. |
| `deviceNumber` | 1 | The `MessageID` this device's data will arrive under. |
| `channelCount` | 1 | How many `CHANNEL_DEFINITION` messages follow for it. |

The source comment on `deviceNumber` is a warning to take seriously:

> *"This is the biggest potential source of corruption. Since this number is meaningless without
> the dictionary."*

That is exactly right, and it is the cost of the design: a one-byte reference is cheap on a
bandwidth-limited downlink, but it carries no self-description at all. A reader that mis-binds
device numbers produces plausible, wrong data rather than an error. This is why a reader must
track whether it has a dictionary at all, and why the strict handlers throw.

### `ChannelDefinition` — `UCSB-Datastream.h:166`, sent as `0xF1`

| Field | Size | Notes |
|---|---|---|
| `deviceGUID` | 16 | **The parent device's GUID — not its number.** |
| `displayName` | 30 | Human-readable channel name. |
| `channelIndex` | 1 | Sender's responsibility to avoid collisions; on collision the most recent wins. |
| `dataType` | 1 | Index into the `DataType` enumeration. |

**Why channels key off GUID rather than device number**, in the source's own words: *"any channel
definition will be the same, no matter how the devices are ordered or configured."* A channel
definition is thus invariant across runs and across renumbering — which is what allows the
telemetry link to carry a *different* device numbering than the recorder while still matching
channels up. See [The telemetry dictionary](#the-telemetry-dictionary-a-second-numbering).

### `dataType` is a permanent on-disk contract

`enum DataType` — `UCSB-Datastream.h:112`. **The stored value is the ordinal**, so this table is
the on-disk contract. Sizes and the config-file spelling of each type come from the parallel
`mapping[]` table at `UCSB-DataStream.cpp:18`.

| Value | Name | Bytes | Meaning |
|---:|---|---:|---|
| `-1` | `INVALID_DATA_TYPE_VALUE` | — | Sentinel. Never written. |
| `0` | `TEXT_CHAR` | 1 | Used as a text character |
| `1` | `UCHAR` | 1 | unsigned `[0…255]` |
| `2` | `SCHAR` | 1 | signed `[-128…127]` |
| `3` | `USHORT_I` | 2 | unsigned `[0…65535]`, **Intel** (little-endian) |
| `4` | `USHORT_M` | 2 | unsigned `[0…65535]`, **Motorola** (big-endian) |
| `5` | `SSHORT_I` | 2 | signed `[-32768…32767]`, Intel |
| `6` | `SSHORT_M` | 2 | signed `[-32768…32767]`, Motorola |
| `7` | `ULONG_I` | 4 | unsigned `[0…4294967295]`, Intel |
| `8` | `ULONG_M` | 4 | unsigned `[0…4294967295]`, Motorola |
| `9` | `SLONG_I` | 4 | signed `[-2147483648…2147483647]`, Intel |
| `10` | `SLONG_M` | 4 | signed `[-2147483648…2147483647]`, Motorola |
| `11` | `SPFLOAT_I` | 4 | IEEE 754 single precision, Intel |
| `12` | `SPFLOAT_M` | 4 | IEEE 754 single precision, Motorola |
| `13` | `DPFLOAT_I` | 8 | IEEE 754 double precision, Intel |
| `14` | `DPFLOAT_M` | 8 | IEEE 754 double precision, Motorola |
| `15` | `UINT64_I` | 8 | unsigned 64-bit, Intel |
| `16` | `DATATYPE_COUNT` | — | Count, not a type. |

The `name` column is not decoration: `ConvertStringToDataType` (`UCSB-Datastream.h:1537`) does a
case-insensitive `_stricmp` against exactly these strings, so this is also the vocabulary that
appears in device configuration text.

**Three asymmetries to notice before you write a decoder:**

- **`UINT64_I` has no `UINT64_M`.** Every other multi-byte type comes as an Intel/Motorola pair;
  the 64-bit unsigned does not. A big-endian 64-bit quantity is unrepresentable.
- **There is no signed 64-bit type at all.**
- **`GetDataSize` returns `0` for anything out of range** (`UCSB-Datastream.h:1149`), including
  `INVALID_DATA_TYPE_VALUE`. It is not an error signal you can distinguish from a genuine
  zero-size answer — check the range yourself.

Its comment shouts, and should be obeyed:

> *"Changes to that enumeration invalidate ALL data formats that depend on it. DON'T DO THAT"*

Values are positional. Inserting a type in the middle silently reinterprets every archived file.
Treat the enum as append-only and frozen.

The warning does carry *partial* compiler enforcement, which is worth knowing about because it
covers less than it appears to. `GetDataSize` asserts `TEXT_CHAR == 0` and
`DATATYPE_COUNT == _countof(mapping)` at compile time, so the enum and the size table cannot
drift apart in *length* or *origin*. Nothing checks that entry `n` of the table still describes
enum value `n` — a reordering that preserves the count compiles clean and silently changes every
size lookup.

### `0xF2`: the third record type that was never built

`BLOCK_DEFINITION` is declared in `MESSAGE_TYPE` beside its two working siblings, which makes it
look like part of the format. It is not. **No writer ever emits it and no reader ever accepts
it**, and the evidence is sitting in the dispatch switch (`UCSB-DataStream.cpp:273`):

```cpp
bool DeviceHolder::DictionaryBuilderImpl(BYTE messageID, BYTE byteCount, BYTE const* pPayload)
{
    switch(messageID)
    {
    case DEVICE_DEFINITION :
        AddDevice(pPayload, byteCount);
        return true;

    case CHANNEL_DEFINITION :
        AddChannel(pPayload, byteCount);
        return true;

        //case BLOCK_DEFINITION :
        //    AddBlock(pPayload, byteCount);
        //    break;
    }

    return false;
}
```

The case is commented out, and `AddBlock` **does not exist anywhere in the tree** — the only
occurrence of that name is inside the comment. So this was sketched as a third dictionary record
type, alongside device and channel, and abandoned before it was written. It was planned and
dropped, not built and retired.

**Where the name comes from.** "Block" is LN-250 vocabulary, not UCSB's: the framing layer calls
a message ID a *block type* — `SendLNMessage(HANDLE hFile, T const& t, BYTE blockType)`
(`LN250.h:492`). So `BLOCK_DEFINITION` reads as "a definition of a block type" in the borrowed
protocol's own terms. The `0xF2` slot itself is UCSB's, though: the LN-250's own message IDs are
low (`0x1E`, `0x2F`, `0x32`), and it has no definitional message of this kind.

**For a parser author**: treat `0xF2` as reserved-and-unused. If you ever encounter one in real
data, it did not come from this writer.

### A reading trap: two message-ID spaces

`LN250.h` declares message IDs `0x1E`, `0x2F` and `0x32` for the instrument's own protocol, and
those land squarely inside the Spaceball device-data range (`0x10`–`0xEF`). They do not collide,
because they belong to *different streams*: the LN-250's IDs appear on the wire between the
instrument and the flight computer, while Spaceball IDs are assigned by `DeviceHolder` for the
recorded stream. Only the framing is shared. Do not carry an ID from one document to the other.

### How the dictionary is emitted

`DeviceHolder::CreateFileHeader` (`UCSB-DataStream.cpp:170`) builds the whole block:

```
"SPACBALL"                       (8 raw bytes, unframed)
  ├─ 0xF0 DeviceDefinition      device A
  │   ├─ 0xF1 ChannelDefinition   A.0
  │   └─ 0xF1 ChannelDefinition   A.1
  ├─ 0xF0 DeviceDefinition      device B
  │   └─ 0xF1 ChannelDefinition   B.0
  └─ ...
```

It iterates slots `FIRST_DEVICE_TYPE`..`LAST_DEVICE_TYPE`, skipping empty ones, and emits each
populated device followed immediately by its channels.

The result is **cached in `m_headerData`** and rebuilt only when `m_writeDictionary` is set —
which happens when a device type is added (`UCSB-Datastream.h:813`). That caching is what makes
frequent re-emission affordable: repeating the dictionary is a `WriteFile` of a prepared buffer,
not a rebuild.

Note the name is misleading. `CreateFileHeader` takes a `HANDLE`, and a serial port is a
`HANDLE` — the identical bytes go to disk and to the radio.

---

## Layer 4 — Data records

Every sample is a `timedT<T>` (`UCSB-Datastream.h:197`), which prepends `BaseData` to the
payload type:

```cpp
struct BaseData {
    UINT64  bd_timer;   // computer clock
    BYTE    bd_index;   // device index
};
```

So a data message's payload is `[UINT64 timestamp][BYTE index][device-specific payload]`, and
the enclosing message's `MessageID` says which device it belongs to.

`timedT<T>::GetClassDescription()` builds a field-name/type list by prepending
`("UINT64_I", "computerClock")` and `("UCHAR", "index")` to the payload type's own description.
That is how the stream stays self-describing without a schema file living alongside it.

### `bd_timer` is a CPU cycle counter, not a wall clock

This is the most important practical fact about the recorded data, and nothing in the record says
it. The value is declared `UINT64_I`, named `computerClock`, and carried in `internalTime` — a
type that wraps a Windows `FILETIME` (`Include\internaltime.h`). All of that suggests absolute
time. **It is not.**

The tick timestamp is produced in the waitable-timer callback, `MakePeriodic::Complete`
(`Include\Periodic.h:107`):

```cpp
//FILETIME ft = {dwTimerLowValue, dwTimerHighValue};
internalTime ft(UCSBUtility::ReadTime());

pThis->m_core.Tick(ft);
```

The commented-out line is the original design: take the timer's own `FILETIME`, which is wall
clock but only as fine as the timer's granularity. The live line replaces it with
`UCSBUtility::ReadTime()` — `rdtsc` (`Include\utility.h:48`), the CPU cycle counter, read by
inline assembly and returned through `EDX:EAX`, which is why it has no `return` statement and is
x86-only.

That value reaches disk unchanged:

```
rdtsc  →  internalTime  →  Tick(now)  →  TickImpl(now)
       →  WriteDataWithCache(now, …)          UCSB-Datastream.h:621
       →  timedT<T>(timer, index, data)       UCSB-Datastream.h:633
       →  bd_timer
```

So **every record is stamped with a raw TSC count**. The trade was deliberate and right for the
job: cycle resolution instead of timer-granularity resolution, which is what lets samples from
independently-ticking device threads be ordered finely against each other. What it costs is
everything absolute — a TSC count has no epoch, its rate is the CPU's clock frequency, and on a
multi-core machine without invariant TSC it is not guaranteed consistent between cores.

### `ClockSync` supplies the calibration

Converting TSC to absolute time needs pairs of `(tsc, wall clock)` samples; two or more let you
fit rate and offset. **That is exactly what the `ClockSync` device provides**, and the way it
does it is worth understanding, because it looks like it is missing half the pair.

`ClockSync` publishes **one** channel — `filetime`, a `UINT64_I` — and acquires it in `TickImpl`:

```cpp
m_Data.filetime = InternalTime::internalTime::Now();   // ClockSync.h
```

`internalTime::Now()` is `GetSystemTimeAsFileTime` (`Include\internaltime.h:46`), so this is
genuine wall clock in 100-nanosecond units.

**The TSC half comes free from the record header.** Every record already carries `bd_timer`, which
is the `rdtsc` value for the tick that produced it. So a single `ClockSync` record *is* a
`(tsc, filetime)` pair: the counter in the header, the wall clock in the payload, both captured on
the same tick. A separate `cpu` channel would be storing the same number twice.

That redundancy is visible in the history. An earlier version of `ClockSync` declared two channels,
`cpu` and `filetime`; the `cpu` field and its channel definition were both removed, leaving the
single-channel form. The pairing did not get weaker — it got deduplicated.

`ClockSync` is enabled in the shipped `devices.cfg` at **1 Hz**, and it is one of only two devices
in the telemetry selection, so the calibration pair reaches both the archive and the downlink:

```
[ClockSync] [E247A5C7-4CCA-403C-BA60-089334450943] [3]  [computerClock] [index] [filetime]
```

Note the downlinked channel list names `computerClock` explicitly alongside `filetime` — the pair,
made visible in the telemetry dictionary.

**So archived data recorded with `ClockSync` enabled is self-calibrating**: fit `bd_timer` against
`filetime` across the `ClockSync` records in a file and you recover both the counter's rate and
its offset, to the precision of the wall clock rather than of a filename.

`TestComputerClock` is the standalone spike on the same problem — it brackets a known interval with
`ReadTime()` calls to characterise the counter — and is not used by the flight program.

### The filename carries wall clock too

No *data* record carries absolute time in its header — but the **file path does**, independently of
`ClockSync`. This is the fallback when a recording was made without `ClockSync`, and a useful
cross-check when it was not.

`NowToSubDirectoryAndFilename` (`Include\utility.h:398`) builds every output path from
`GetLocalTime`:

```cpp
swprintf_s(buffer, L"\\%04d%02d%02d\\", st.wYear, st.wMonth, st.wDay);      // subdirectory
swprintf_s(buffer, L"%02d%02d%02d.%ls", st.wHour, st.wMinute, st.wSecond,   // filename
           extention.c_str());
```

giving `.\YYYYMMDD\HHMMSS.spaceball`. Files roll every `g_secondsBoundary` seconds — **600**, ten
minutes (`FlightCode-01.cpp:57`) — on boundaries computed by `GetTimeBoundary`
(`internaltime.h:119`) and advanced by `FindNextTimeInTheFuture`, so the names land on regular
wall-clock marks. `UpdateSharedFile` creates the file on the first main-loop pass after the
boundary passes, and that loop runs every ~10 ms, so the name is within milliseconds of the true
boundary.

**What this gives you:**

| | |
|---|---|
| Absolute anchor per file | file path, **local time**, 1 s resolution |
| Fine relative time within a file | `bd_timer` deltas, in CPU cycles |
| TSC rate | derivable — divide the TSC delta across a file by the wall-clock interval between consecutive filenames |

That last row matters only when `ClockSync` is absent. Because files roll on a known 600-second
cadence, consecutive filenames give a wall-clock interval that can be divided into the
corresponding `bd_timer` span to **estimate the effective TSC frequency** — a calibration fitted
over ten-minute spans rather than sampled once a second.

**Caveats on that fallback method**: timestamps are **local time**, not UTC, so a recording
spanning a DST transition is ambiguous; the TSC rate is assumed stable across the interval, which
frequency scaling or a non-invariant TSC would violate; and it assumes the file was created at its
boundary rather than after a stall. `ClockSync` has none of these problems, which is why it is the
preferred source.

**For anyone using archived data**, in order of preference:

1. **`ClockSync` records** — fit `bd_timer` against `filetime` to calibrate the counter's rate and
   origin. Present whenever `ClockSync` was configured.
2. **`GPSTime`** from the LN-250 or Ashtech, when those instruments were running.
3. **The file path**, as an anchor and a cross-check, always available.
4. **`bd_timer` deltas** for fine ordering *within* a file, once calibrated.
5. **`ticks`** from the Digital Counter to confirm no frames went missing — independent of every
   clock above.

### Where absolute time comes from

Four sources, in descending order of precision. **The recorder itself supplies the first**, via
`ClockSync`, so a recording is normally self-sufficient:

| Source | Resolution | Available when |
|---|---|---|
| `ClockSync` `filetime` paired with the record's own `bd_timer` | 100 ns units, wall clock | `ClockSync` configured (it is, at 1 Hz) |
| `GPSTime` (LN-250, `DPFLOAT_M` seconds) | instrument rate | LN-250 configured |
| `GPSTime` (Ashtech, `SPFLOAT_I`) | 4 Hz as flown | Ashtech configured |
| File path `YYYYMMDD\HHMMSS` | 1 s, local time | always |

The first row is the one to reach for: it needs no instrument, it is sampled continuously, and it
calibrates the counter that timestamps *every other record*. The filename remains a useful
cross-check and the only fallback for a recording made without `ClockSync`.

**The control-message space `0x00`–`0x0E` is still entirely unused**, which is worth knowing for a
different reason now. Fifteen message IDs sit at the bottom of the space; every parser routes them
to `ControlData(messageID)`, and the default handler ignores an unrecognised one silently
(`void ControlData(BYTE) const {};`, `UCSB-Datastream.h:910`), so a new control message would be
backward compatible with every existing reader. The only control message written anywhere in this
tree is `TELEMETRY_EOL` (`0x0F`), emitted once per transmission group at `Telemetry.cpp:326` and
recognised only by `TelemetryReceiver.cpp:42`.

That space stayed empty because the device mechanism solved the problem instead: `ClockSync` is a
clock *as a device*, which gets it a GUID, a dictionary entry, a configurable rate, and a place in
the telemetry selection — all for free, from machinery that already existed. A control message
would have needed its own handling on both ends. It is a fair illustration of the registration
design paying off: the cheapest way to add a capability was to add a device.

### Detecting dropped frames

`CounterSource` ("Digital Counter") increments `ticks` once per tick and never skips
(`CounterSource.h:36`). When it is enabled, **a gap in `ticks` is a dropped frame** — the counter
is the ground truth for whether the recording chain, the telemetry link, or a converter lost
samples, and it is independent of any clock. `telemetrySend.cfg` downlinks `ticks` alongside the
LN-250 for exactly this check.

That makes it the more reliable integrity signal of the two: `bd_timer` can only tell you that
time passed, whereas `ticks` tells you how many frames *should* have been there.

---

## Reading a stream

The full pipeline:

```
bytes (file or serial)
  └─ InputBuffer::AddData              LN250.h:451   accumulate, track consumed
       └─ ChunkInputData               LN250.h:378   resync, validate, emit messages
            └─ FileParser<Handler>     UCSB-Datastream.h:582
                 └─ FileParserImpl     UCSB-Datastream.h:847   dispatch on MessageID
                      └─ your handler
```

### The dispatch

`DeviceHolder::FileParserImpl` (`UCSB-Datastream.h:847`) is 25 lines and decides everything:

1. Is it a dictionary message (`0xF0`/`0xF1`)? Absorb it, call `DictionaryUpdated()`, done.
2. Is `MessageID < 0x10`? Call `ControlData(messageID)`, done.
3. Otherwise it is device data. Look up `m_Dictionary[messageID]`:
   - **No such device** → `MissingDictionaryEntry(messageID)` and return false.
   - **Size mismatch** between the dictionary's expected size and `ByteCount` →
     `InvalidData(...)` and return false.
   - **Otherwise** → `ProcessData(device, payload)`.

### The handler contract

Supply an object with five members. `DefaultThrowingParserHandler`
(`UCSB-Datastream.h:908`) is the strict reference implementation.

| Callback | When | Default behaviour |
|---|---|---|
| `DictionaryUpdated()` | A definition was absorbed | nothing |
| `ControlData(BYTE)` | `MessageID < 0x10` | nothing |
| `MissingDictionaryEntry(BYTE)` | Data for an unknown device | **throws** |
| `InvalidData(...)` | Payload size disagrees with dictionary | **throws** |
| `ProcessData(Device const&, BYTE const*)` | A good, described sample | — |

### Two reader postures, and why both are right

This is the subtlety that catches people, because the format's tolerance is a **handler policy**,
not a property of the parser.

**Strict / whole-file.** `DefaultThrowingParserHandler` throws on a missing dictionary entry.
The `DataAggregationTool` that sits on top of it is documented as assuming the dictionary occurs
exactly once, treating the first non-dictionary message as the end of the dictionary, and caching
device indices permanently. Its comment is explicit:

> *"Do not use this with buffered file reads or for streamed data. It is likely to break."*

That strictness is correct for ground processing of a complete recording, where a missing entry
means genuine corruption and should stop the run.

**Tolerant / streaming.** Supply your own handler whose `MissingDictionaryEntry` returns quietly.
The parser core supports this fully — it returns `false` and moves on. This is the posture for a
live downlink, and `TelemetryReceiver` implements it (below).

If you write a new parser for archived data, start from the strict handler. If you write one for
a live feed, do not.

---

## Recovery in practice

### On disk: every file starts complete

`UpdateSharedFile` calls `g_devices.CreateFileHeader(hFile)` on each roll
(`FlightCode-01.cpp:82`), and files roll on wall-clock boundaries. So **every recorded file
begins with a full dictionary**, and any single file is independently interpretable. A truncated
or lost file costs you that file, not the ones after it.

### On the radio: the dictionary repeats every 60 seconds

`Telemetry::TickImpl` (`FlightCode-01\Telemetry.cpp:267`):

```cpp
if (now - m_lastMinute > InternalTime::oneMinute)
{
    m_sendDictionary = true;
    m_lastMinute = now;
}
...
if (m_sendDictionary)
{
    m_telemetryDictionary.CreateFileHeader(m_port.GetHandle(), &written);
    m_sendDictionary = false;
}
else
{
    // ... send data this tick instead
}
```

Note the `if`/`else`: a dictionary tick sends **no data**. A full dictionary costs one tick's
bandwidth once a minute, and that is the price of the guarantee.

**So the worst case for a ground station that joins mid-flight, reboots, or loses the link is
under 60 seconds of uninterpretable data.** That is the design goal, stated as a number.

### The ground station's behaviour

`TelemetryReceiver.cpp` implements the tolerant posture literally:

- `hasDictionary` starts `false` (`:34`).
- While it is false, the display reads **`"Waiting for Dictionary"`** (`:62-64`).
- `DictionaryUpdated()` flips it true (`:36-38`).
- `MissingDictionaryEntry` checks it first (`:164-174`):

```cpp
void MissingDictionaryEntry(BYTE dictionaryEntry)
{
    // If we have never received a dictionary there is nothing we can do
    if (!hasDictionary)
    {
        return;
    }
    UCSBUtility::LogError(..., "Dictionary Entry %d is missing\n", dictionaryEntry);
}
```

Before a dictionary arrives, unknown device numbers are *expected* and silent. After one arrives,
an unknown device number is a *real anomaly* and gets logged. The same event is noise or signal
depending on state, and the code says so in a comment rather than leaving you to infer it.

`TELEMETRY_EOL` (`0x0F`) marks the end of a transmission group, letting the receiver refresh its
display on a coherent boundary rather than mid-update (`:44`).

---

## The telemetry dictionary: a second numbering

The downlink does not carry everything. `Telemetry` maintains its own `m_telemetryDictionary`,
built from a configuration block (`CreateTelemetryDictionary`, `Telemetry.cpp:163`), holding a
**subset** of channels chosen to fit the radio budget.

That subset has its own device numbering, independent of the recorder's. `PacketConverter`
(`Telemetry.cpp:37`) bridges them, and it does so **by GUID**:

```cpp
m_outputDictionary.GetDevice(device.DeviceID(), outputDevice);   // Telemetry.cpp:56
```

This is the payoff for the design decision back in `ChannelDefinition`. Because channels are
keyed on the device GUID rather than the device number, the same physical channel can be device
`0x14` on disk and device `0x11` on the downlink, and the two dictionaries still agree on what it
is. Had channels been keyed on device number, the two numbering schemes could not coexist.

Note also that `PacketConverter` is itself a parser handler — telemetry is implemented by
*parsing the recorder's own cache* through `FileParser` and re-emitting a subset. The format is
used as its own internal transport.

---

## Downstream: how these types become FITS

**Why this conversion exists at all:** Spaceball is an internal format. It has no published
specification, no registry entry, no reader outside this tree, and — as the section above
explains — a framing layer adopted from an instrument rather than designed for interchange. It
was built to get bytes off a balloon and onto a disk reliably, and it is good at that.

None of which makes it something you can hand to an astronomer. FITS is the interchange standard
the field actually reads, so the data had to be converted before it could be used or published.
That is the entire reason `Spaceball2FITS`, `SpaceballConversion` and `ConvertSpaceballFiles`
exist: **the recording format and the publication format were never meant to be the same thing.**

`Include\ConversionTool.cpp` turns a Spaceball stream into a FITS binary table. The mapping from
`DataType` to a FITS column is `FITSTableID` (`ConversionTool.cpp:93`), which carries three
things per type: the FITS `TFORM` code, a `TZERO` offset, and a byte width.

| Spaceball type | FITS `TFORM` | Width |
|---|---|---|
| `TEXT_CHAR` | `A` | 1 |
| `UCHAR` | `B` | 1 |
| `SCHAR` | `B` | 1 |
| `USHORT_I` / `USHORT_M` | `I` | 2 |
| `SSHORT_I` / `SSHORT_M` | `I` | 2 |
| `ULONG_I` / `ULONG_M` | `J` | 4 |
| `SLONG_I` / `SLONG_M` | `J` | 4 |
| `SPFLOAT_I` / `SPFLOAT_M` | `E` | 4 |
| `DPFLOAT_I` / `DPFLOAT_M` | `D` | 8 |
| `UINT64_I` | `K` | 8 |

Note the byte-order suffix is **dropped** in this mapping — `USHORT_I` and `USHORT_M` both become
`I`. The converter is choosing a FITS type by width and signedness only.

### The unsigned offsets are switched off

FITS binary-table integer columns are signed, and the standard way to store unsigned values is a
`TZERO` offset — 32768 for unsigned 16-bit, 2147483648 for unsigned 32-bit, 2^63 for unsigned
64-bit. `FITSTableID` has a `zero` field for exactly this, and `GetZero()` emits it, returning
`""` when the value is zero so the keyword is omitted entirely.

**In the shipped version every unsigned offset is 0, with the intended value parked in a trailing
comment:**

```cpp
table[USHORT_I] = FITSTableID('I', 0, 2);   // ... Intel byte ordering //32768
table[ULONG_I]  = FITSTableID('J', 0, 4);   // ... Intel byte ordering // 2147483648
table[UINT64_I] = FITSTableID('K', 0, 8);   // 8 byte unsigned Intel   //9223372036854775808
```

So **unsigned 16-, 32- and 64-bit channels are written to FITS with no `TZERO` keyword.** A value
above the signed maximum of its column will read back negative in any standards-conforming FITS
reader. Anyone interpreting archived FITS products from this tool needs to know that, and needs
to know the original channel's declared `DataType` to correct for it.

`SCHAR` is the exception — it still carries an offset, and it carries a workaround with it:

```cpp
if (t == SCHAR) { return "-128"; }          // ConversionTool.cpp:124
```

`zero` is a `UINT64`, so a stored `-128` formatted with `%I64u` would print
`18446744073709551488`. Rather than widen or re-sign the field, `GetZero` special-cases the type
and returns the literal string.

### The aborted change, and the file that records it

`Include\` contains two near-identical parsers, `ConversionTool.cpp` and
`ConversionTool - Copy.cpp`. They are not a mystery once you line up the dates and the diff:

| | `ConversionTool - Copy.cpp` | `ConversionTool.cpp` |
|---|---|---|
| Last written | 20 Apr 2010 | 9 Nov 2010 |
| Size | 46 KB, 1529 lines | 53 KB, 1762 lines |
| In any project? | **No** | Yes — `SpaceballConversion`, `Test-UCSB-DataStream` |

The Copy is the **older** state, preserved as an Explorer-style copy and then left behind while
the original went on for another seven months. In it, the unsigned offsets are *populated*:

```cpp
table[USHORT_I] = FITSTableID('I', 32768, 2);
table[ULONG_I]  = FITSTableID('J', 2147483648, 4);
table[UINT64_I] = FITSTableID('K', 9223372036854775808), 8;     // <-- note the parenthesis
```

That last line is malformed. The closing parenthesis lands after the second argument, making it a
comma expression rather than a three-argument construction, and the literal `2^63` overflows a
signed 64-bit type without a `ULL` suffix. It is broken code.

So the sequence reads: unsigned `TZERO` support was attempted, ran into exactly the 2^63 literal
and type-width problems visible in that line, and was **backed out rather than finished** — the
offsets zeroed, the intended values preserved in comments, `SCHAR` given a cast and an output
special-case. The Copy is the snapshot taken before that surgery.

This matters for reading the code, not for fixing it: the commented-out constants are not dead
decoration, they are the record of an intent that was parked. And `ConversionTool - Copy.cpp`
belongs to no project — it compiles in no build and can be read purely as history.

---

## Writing a modern parser: gotchas

1. **Everything is `#pragma pack(1)`.** No padding, anywhere. Do not map these structs with a
   language or compiler that inserts alignment.
2. **`GUID` is the Windows layout** — `{DWORD, WORD, WORD, BYTE[8]}`, first three fields
   little-endian. It is not a flat 16-byte big-endian UUID.
3. **`displayName` is `char[30]`, not necessarily NUL-terminated.** Treat it as a fixed 30-byte
   field.
4. **Honour the `_I` / `_M` suffix on `dataType`.** Both byte orderings appear in the enum; do not
   assume host order. Note the gap: there is no `UINT64_M` and no signed 64-bit type.
5. **If you consume the headers from C++, `UCHAR` collides with the Windows typedef.** The
   authoritative table has to write `UCSB_Datastream::UCHAR` for that one entry
   (`UCSB-DataStream.cpp:21`) while its neighbours are unqualified. Qualify it or you will get a
   confusing error on exactly one enumerator.
6. **Do not trust `ByteCount` after a checksum failure.** Rescan byte by byte. The original does
   this deliberately; a parser that skips `ByteCount` bytes on a bad packet will desynchronize on
   exactly the corrupt streams the format exists to survive.
7. **Track "have I seen a dictionary" explicitly.** Without it you cannot distinguish a normal
   cold start from genuine corruption, and you will either spam errors or hide real ones.
8. **The `SPACBALL` magic is unframed.** Only useful at offset 0.
9. **A device number is meaningless without the dictionary that was current when it was written.**
   Never cache device numbers across a `DictionaryUpdated()` on a live stream.

---

## Open questions

- **What `numDeadBytes` was for.** `ChunkInputData` counts discarded bytes, which would be a
  useful link-quality metric, but the local is not obviously surfaced anywhere.
- **Whether `numDeadBytes` is reported anywhere.** `ChunkInputData` counts discarded bytes, which
  would be a useful link-quality metric, but the local is not obviously surfaced.
- **Whether the missing `TZERO` offsets ever mattered in practice** — that depends on whether any
  recorded unsigned channel actually carried values above its column's signed maximum, which the
  code cannot tell us. The archived data can.
