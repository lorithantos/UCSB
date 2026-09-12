# Configuration

**The flight program's device list is data, not code — and the data survives.** Three `.cfg`
files in `FlightCode-01\` are compiled into the executable as resources, and any of them can be
overridden at run time by dropping a file of the same name beside the `.exe`.

They are also the best documentation in the repository. Each one carries its parameter grammar as
commented examples, which is how you learn what arguments a device takes without reading its
`Configure` method.

Companion to [`ARCHITECTURE.md`](ARCHITECTURE.md) (how devices register and get built),
[`FILE-FORMAT.md`](FILE-FORMAT.md) (what they write) and [`DEVICES.md`](DEVICES.md) (what each
one is).

> Describes the **December 2012** working copy. The `devices.cfg` documented here is dated
> 2012-12-20 — the same day as the seven recordings in `FlightCode-01\20121220\`, so this is the
> configuration that produced the sample data in this repository.

---

## Where configuration comes from

Declared in `FlightCode-01\ReadICD_IOTech.rc:55` — note the resource script is named after a
different program, one of several signs this project file grew by copying:

```
devices                TXT    "devices.cfg"
configuration          TXT    "configuration.cfg"
telemetrySend          TXT    "telemetrySend.cfg"
```

Three custom resources of type `TXT`, each embedding the named file at build time.

### File wins over resource

`InterpretConfigurationFields` (`Include\ConfigFileReader.h:355`) resolves a config name like
this:

```cpp
wstring filename = configName + L".cfg";

if (UCSBUtility::FileExits(filename))
{
    UCSBUtility::Logging::LogError("Using File %ls\n", filename.c_str());
    return InterpretConfigurationFileFields(filename, processFields);
}

UCSBUtility::Logging::LogError("Using Resource %ls\n", configName.c_str());
return InterpretConfigurationResourceFields(configName, processFields);
```

So `InterpretConfigurationFields(L"Devices", ...)` looks for `Devices.cfg` in the working
directory first, and only falls back to the compiled-in copy if it is absent. **You can change
which instruments the program talks to without rebuilding it**, and the error log records which
source was used — `"Using File ..."` or `"Using Resource ..."`. That log line is the first thing
to check when the program is not reading the device you expected.

`EncoderReader` calls `InterpretConfigurationResourceFields` directly
(`EncoderReader.cpp:150`, resource `devicesConfiguration`), so it has **no** file override.
Only the paths that go through `InterpretConfigurationFields` get one.

---

## Syntax

Line-oriented. `;` begins a comment. Values are wrapped in square brackets, whitespace between
them is insignificant, and the brackets are stripped into a flat token stream — which is the
stream `Configure` consumes, described in
[`ARCHITECTURE.md`](ARCHITECTURE.md#from-config-text-to-live-objects).

```
[Device Name] [param] [param]
```

The first token on a line names the device; the rest are its parameters, and **each device
decides how many tokens to take**.

---

## `devices.cfg` — the device list

### The keyword is the display name, not the class name

This is the single most confusing thing about the configuration, and there is no way to guess it
from either side. The factory is keyed on `ToLower(T::GetName())`, and `GetName()` returns a
human-readable label:

| Config keyword | C++ class | Declared in |
|---|---|---|
| `[MMC Device]` | `MMCSource` | `MMCSource.h:23` |
| `[Digital Counter]` | `CounterSource` | `CounterSource.h:71` |
| `[CommandLineDisplay]` | `DisplaySource` | `DisplaySource.h:48` |
| `[LN-250]` | `LN250Reader` | `LN250.cpp:153` |
| `[Ashtech]` | `Ashtech` | `Ashtech.h:163` |
| `[Magnetometer]` | `Magnetometer` | `Magnetometer.h:183` |
| `[ClockSync]` | `ClockSync` | `ClockSync.h:66` |
| `[Telemetry]` | `Telemetry` | `Telemetry.h:53` |
| `[TelemetryReceiver]` | `TelemetryReceiver` | `TelemetryReceiver.h:67` |
| `[DaqCOM]` | `DaqCOMSource` | `DaqCOM.h:452` — *not built into FlightCode-01* |
| `[TestDevice]` | `TestDevice` | `DataSource.cpp:20` |

Searching the source for `"Digital Counter"` finds it; searching for `CounterSource` in the
config finds nothing. Start from this table.

### Parameter grammar, per device

Taken from the commented examples in `devices.cfg` itself, which is where this is documented:

| Device | Parameters |
|---|---|
| `[IOTech]` | `[board name] [read frequency Hz] [number of scans] [ClearOnZ true\|false]` |
| `[DaqCOM]` | `[read frequency Hz] [number of scans]` |
| `[LN-250]` | `[COM port] [baud]` |
| `[Magnetometer]` | `[read frequency Hz] [PortName] [BaudRate]` |
| `[Ashtech]` | `[read frequency Hz] [PortName] [BaudRate]` |
| `[CommandLineDisplay]` | `[frequency Hz]` |
| `[Telemetry]` | `[frequency Hz] [PortName] [BaudRate]` |
| `[TelemetryReceiver]` | `[frequency Hz] [output path] [PortName] [BaudRate]` |
| `[Digital Counter]` | `[frequency Hz]` |
| `[MMC Device]` | *none* |

Ports are given in Windows extended form — `\\.\COM28` — which is what lets port numbers above 9
work at all.

### What was actually configured

The shipped `devices.cfg` is dated **20 December 2012**, and the same date appears on the seven
recordings in `FlightCode-01\20121220\` — so this is the configuration that produced the sample
data in the repository. Six device instances are live:

```
[Digital Counter] [10]
[Digital Counter] [30]
[ClockSync] [1]
[CommandLineDisplay] [10]
[Telemetry] [10] [\\.\COM1] [9600]
[Telescope] [10] [\\.\COM4] [9600]
```

Reading it as a session: the **`Telescope` payload at 10 Hz** is the subject; `ClockSync` at 1 Hz
makes the recording self-dating; two `Digital Counter` instances at different rates give
dropped-frame detection at two cadences; `CommandLineDisplay` puts it on screen; and `Telemetry`
is transmitting live down COM1.

**`[Digital Counter]` appears twice**, which is legal and deliberate — the factory creates one
object per config line, so two counters run at 10 Hz and 30 Hz. They share a dictionary entry (same
class, same GUID) and are told apart by the `bd_index` on each record. If you are wondering what
`bd_index` is *for*, this is it.

It is an **instrument-and-telemetry configuration without navigation** — no LN-250, no
magnetometer, no Ashtech GPS. The commented-out lines preserve the settings those instruments used
and are worth reading as a record of the hardware:

```
; [IOTech] [DaqBoard3031USB{325188}] [50] [100] [false]
; [LN-250] [\\.\COM28] [1500000]
; [Magnetometer] [40] [\\.\COM6] [57600]
; [Ashtech] [4] [\\.\COM5] [38400]
; [Telemetry] [10] [\\.\COM1] [9600]
; [TelemetryReceiver] [1] [.\] [\\.\COM4] [9600]
```

Note the rates, which say something about each instrument's role: the LN-250 inertial unit at
1.5 Mbaud, the magnetometer at 40 Hz, the Ashtech GPS at 4 Hz, telemetry down a 9600-baud link.
The gap between a 1.5 Mbaud instrument and a 9600-baud downlink is the reason the telemetry
dictionary exists at all — see below.

Two commented `[Telemetry]` lines point at a **file** rather than a COM port:

```
; [Telemetry] [20] [c:\temp\testData.spaceball] [9600]
; [TelemetryReceiver] [10] [.\] [C:\Temp\telemetry\191222.telemetry] [9600]
```

Since everything downstream takes a `HANDLE`, a file substitutes for a serial port transparently.
That is how the telemetry path was tested without radios.

---

## `telemetrySend.cfg` — the downlink channel selection

This file defines the **reduced dictionary** transmitted over the radio. Its format differs from
`devices.cfg`: each line names a device by **keyword *and* GUID**, gives a channel count, then
lists the channels by display name — all on one line.

```
; Name of the data block we are creating
; [Full Telemetry]

; Remember to change the channel count if removing any channels
[Digital Counter] [D6FE3751-EB74-44E0-AA98-5BD70D42A3E7] [7]  [computerClock] [index] [ticks] [ones] [tens] [hundreds] [thousands]
[ClockSync] [E247A5C7-4CCA-403C-BA60-089334450943] [3]  [computerClock] [index] [filetime]
```

The channel count is **manually maintained** and the file says so — *"Remember to change the
channel count if removing any channels."* It is a hand-kept invariant, not a derived one.

The selection is small and purposeful: the counter's full five channels for frame-integrity
checking, and `ClockSync`'s `filetime` paired with `computerClock` so the ground station can
convert the `rdtsc` timestamps on every record to wall clock. Those two devices make a downlink
*self-validating and self-dating* before any science channel is added.

An earlier format is preserved beside it in `telemetrySend.old.cfg`, which keyed devices by GUID
alone with each channel on its own line, and selected LN-250 attitude channels
(`HybridPitchAngle`, `HybridRollAngle`, `HybridYawAngle`, `SystemTimer`, `GPSTime`). Useful as a
record of what a navigation-carrying downlink looked like — including a `;[HybridHeadingAngle]:)`
with a smiley left beside the commented-out line.

**This is the file that justifies the GUID-keyed channel design.** The downlink carries its own
device numbering, independent of the recorder's, and `PacketConverter` bridges the two by GUID
(`Telemetry.cpp:56`). Selecting channels here by device GUID rather than device number is what
makes that work. See
[`FILE-FORMAT.md`](FILE-FORMAT.md#the-telemetry-dictionary-a-second-numbering).

The file's own warning, worth repeating because it describes a silent failure:

> *"a duplicate device ID will result in overwriting the data selected from that device. That is,
> if four channels are selected in one definition and two are selected in a later definition then
> the result will be to choose the two channels. It is not possible to repeat a device later."*

A second block for the same GUID does not add channels — it **replaces** the selection.

This version is consistent with `devices.cfg` — both enable `ClockSync` and the counters, and
neither enables the LN-250. The `.old.cfg` beside it is the one that drifted.

---

## `configuration.cfg` — empty, and pointing at a file that is gone

In full:

```
; For devices, modify devices.cfg
; For the dictionary, modify dictionary.cfg
; This file doesn't have anything real in it
```

It is a signpost with nothing behind it. Note the second line: **`dictionary.cfg` does not exist
anywhere in this tree.** Whether it was ever created, or the dictionary moved into
`telemetrySend.cfg` and the comment went stale, is not recoverable from the source.

---

## Other configuration files in the tree

Each project carries its own, and they do not share a schema:

| File | Belongs to |
|---|---|
| `EncoderReader\devices.cfg`, `configuration.cfg` | `EncoderReader` (resource-only, no file override) |
| `TestICD\configuration.cfg`, `configuration0.cfg` | `TestICD` |
| `Test-UCSB-DataStream\interpreter.cfg`, `reader.cfg` | `ConvertSpaceballToText` |
| `TimedDownload\Configuration.cfg` | `TimedDownload` |
| `ConvertSpaceballFiles\config.cfg` | the VB.NET converter |

---

## Open questions

- **`dictionary.cfg`** — referenced by `configuration.cfg`, absent from the tree.
- **Whether a flight-configured `devices.cfg` exists elsewhere.** The one here enables three
  devices and is dated February 2011; the commented lines imply a fuller configuration was used
  at some point, but no such file survives in this tree.
- **The `[Basic Telemetry]` block name** — it is read as the name of the data block being
  created, but where that name surfaces in the output was not traced.
- **`IOTech`'s config keyword** was not confirmed from source; `[IOTech]` is taken from the
  example line in `devices.cfg`. The class is not compiled into `FlightCode-01` in any case.
- **`EncoderWrapper::GetName()` returns `"DaqBoard3031USB{325188}"`** (`Encoder.cpp:42`) — a
  board identifier rather than a device label, and the same string that appears as `[IOTech]`'s
  first parameter. The file is in no project, so this was not pursued.
