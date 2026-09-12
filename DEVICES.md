# Device catalogue

**What each instrument is, what it publishes, and how to find it in the stream.**

> Describes the **December 2012** working copy (`UCSB.sln`, last written 2012-12-20). An earlier
> mid-2011 snapshot is the parent commit in this repository; where the two differ materially the
> text says so. Verify against the code before trusting a claim about what is or is not finished —
> that distinction is exactly what changed between the two.

**The payload is `Telescope`**: a 16-channel polarimeter publishing Stokes `T`, `Q` and `U` per
channel. Everything else here is supporting instrumentation — three attitude references to say
where each sample pointed, a clock to date it, and a counter to prove none were lost.

Every entry here is a `DataSource` subclass that registers itself via `AutoRegister` — see
[`ARCHITECTURE.md`](ARCHITECTURE.md#the-spine-devices-register-themselves). It is named in
`devices.cfg` by its **display name** (see [`CONFIGURATION.md`](CONFIGURATION.md)), and it appears
in the recorded stream under its **GUID** (see [`FILE-FORMAT.md`](FILE-FORMAT.md)).

---

## A `DataSource` is not necessarily a data source

The first thing to know, because the name misleads. Three of the registered devices publish no
channels at all — they have no `GetClassGUID`, no `GetClassDescription`, and nothing appears in
the dictionary for them:

| Device | What it actually does |
|---|---|
| `CommandLineDisplay` | Renders current values to the console |
| `Telemetry` | Reads the shared cache and transmits a subset downlink |
| `TelemetryReceiver` | Receives and decodes a downlink |

`DataSource` is really **"anything that needs to be ticked on its own thread at its own rate."**
Production is the common case, not the definition. `Telemetry` is the clearest example: it
consumes the recorder's cache through the same `FileParser` a ground station would use, then
re-emits a reduced selection.

---

## Quick reference

| Config keyword | Class | GUID | Channels | In `FlightCode-01`? | Enabled? |
|---|---|---|---:|---|---|
| `[Telescope]` | `Telescope` | `A38A47EE-D74E-479a-A2C1-FE7EFDC2C4CF` | 49 | yes | **yes**, 10 Hz |
| `[LN-250]` | `LN250Reader` → `HybridInertialData` | `3EE2B4C5-D1EB-4fec-B72B-DB25F1DF69ED` | 15 | yes | no |
| `[LN-250-MCD]` | LN-250 Mode Command Data | — | — | yes | no |
| `[Ashtech]` | `Ashtech` | `188072F4-E523-4c3a-B0D0-4A0045475070` | 9 | yes | no |
| `[Magnetometer]` | `Magnetometer` | `7330215D-0399-44fb-982D-4F0F61BC91C6` | 7 | yes | no |
| `[Digital Counter]` | `CounterSource` | `D6FE3751-EB74-44e0-AA98-5BD70D42A3E7` | 5 | yes | **yes**, ×2 (10 Hz, 30 Hz) |
| `[ClockSync]` | `ClockSync` | `E247A5C7-4CCA-403c-BA60-089334450943` | 1 | yes | **yes**, 1 Hz |
| `[MMC Device]` | `MMCSource` | `83DE51AC-7648-466a-9ED4-6B140977C4AC` | 8 | yes | no |
| `[CommandLineDisplay]` | `DisplaySource` | — | none | yes | **yes**, 10 Hz |
| `[Telemetry]` | `Telemetry` | — | none | yes | **yes**, 10 Hz, COM1 |
| `[TelemetryReceiver]` | `TelemetryReceiver` | — | none | yes | no |
| `[TestDevice]` | `TestDevice` | — | — | yes (registers privately) | n/a |
| `[DaqCOM]` | `DaqCOMSource` | `751CC155-0B96-460d-9792-40D2401D348B` | 32 | **no** — not in the build | — |
| `[IOTech]` | `EncoderWrapper` | `3C452666-DBEA-42ff-93C1-F6D0A183161B` | ? | **no** — `Encoder.cpp` in no project | — |
| *(library)* | `IOTech` | ? | ? | no — used by `EncoderReader`, `TestICD` | — |

Two further GUIDs are not devices: `GUID_InternalTimer`
(`7DD3D860-4F19-4a5a-B318-3D9A0278D693`, `FlightCode-01.cpp:47`), and the LN-250's additional
message types `LN-250 INS Status` and `LN-250 Mode Command Data` (`Include\LN250.h`), which are
data *shapes* the reader can publish rather than configurable devices.

**Channel counts above are each device's *own* channels.** The on-disk dictionary reports two
more for every device — `computerClock` and `index`, prepended by `timedT<T>` — so `Telescope`
appears as 51, `Digital Counter` as 7, `ClockSync` as 3. Confirmed against real files with
[`tools\SpaceballToJson`](tools/SpaceballToJson/README.md).

**Note a device can be instantiated more than once.** The shipped config runs two
`[Digital Counter]` instances at different rates — the factory creates a new object per config
line, and each gets its own sequential `m_index`. They share one dictionary entry and are told
apart by `index` in the record, which is visible in the decoded output: two independent `ticks`
sequences interleaved under `index` 0 and 1.

---

## Instruments

### LN-250 — inertial navigation

`[LN-250] [COM port] [baud]` · flown at `\\.\COM28`, **1500000 baud**

The primary attitude and position reference: a Northrop Grumman LN-250 inertial navigation unit,
read over RS-422. Its message framing is also the container format for the entire recording —
see [`FILE-FORMAT.md`](FILE-FORMAT.md#why-it-is-borrowed).

**Two classes, two names.** `LN250Reader` (`LN250.cpp:27`) is the `DataSource` — it owns the
serial port and answers to the config keyword `"LN-250"`. But the *device* in the dictionary is
`HybridInertialData` (`LN250.h:97`), whose `GetName()` returns `"LN-250 Hybrid Inertial Device"`
and whose GUID and channels are what get registered
(`LN250.cpp:94` — `RegisterWriter(*this, HybridInertialData::GetClassGUID())`).
**The config keyword and the recorded device name are different strings.**

The source message is `MESSAGE_ID = 0x32` on the instrument's own protocol.

**It arrives big-endian.** `HybridInertialData::Reorder` (`LN250.h:115`) byte-reverses every
field on receipt, and the channels are declared with Motorola (`_M`) types accordingly. This is
the concrete reason the stream format carries Intel/Motorola variants at all.

| Channel | Type | Units |
|---|---|---|
| `SystemTimer` | `DPFLOAT_M` | sec |
| `GPSTime` | `DPFLOAT_M` | sec |
| `OutputDataValidity` | `SSHORT_M` | discrete word |
| `HybridLatitude` | `DPFLOAT_M` | **radians** |
| `HybridLongitude` | `DPFLOAT_M` | **radians** |
| `HybridAltitude` | `DPFLOAT_M` | meters (HAE) |
| `HybridNorthVelocity` | `SPFLOAT_M` | m/s |
| `HybridEastVelocity` | `SPFLOAT_M` | m/s |
| `HybridVerticalVelocity` | `SPFLOAT_M` | m/s (+up) |
| `HybridHeadingAngle` | `SPFLOAT_M` | **radians** |
| `HybridPitchAngle` | `SPFLOAT_M` | **radians** |
| `HybridRollAngle` | `SPFLOAT_M` | **radians** |
| `HybridYawAngle` | `SPFLOAT_M` | **radians** |
| `HybridFOM` | `UCHAR` | figure of merit |

Angles are in **radians** and altitude is height-above-ellipsoid. "Hybrid" is inertial-navigation
vocabulary for a solution blended from inertial and GPS inputs — these are not raw gyro readings.
The units and byte offsets in `LN250.h` were transcribed from the instrument's interface
document, which is the only place they are recorded.

### Ashtech — GPS

`[Ashtech] [read frequency Hz] [PortName] [BaudRate]` · flown at 4 Hz, `\\.\COM5`, 38400

GPS receiver providing position and an independent attitude solution from a multi-antenna
baseline.

| Channel | Type |
|---|---|
| `GPSTime` | `SPFLOAT_I` |
| `heading` | `SPFLOAT_I` |
| `pitch` | `SPFLOAT_I` |
| `roll` | `SPFLOAT_I` |
| `baseline` | `SPFLOAT_I` |
| `reset` | `SCHAR` |
| `latitude` | `SPFLOAT_I` |
| `longitude` | `SPFLOAT_I` |
| `altitude` | `SPFLOAT_I` |

Note this is a **second, independent** source of heading/pitch/roll and lat/long alongside the
LN-250 — at a much lower rate (4 Hz vs the inertial unit) and in single precision. The two exist
to be compared. `baseline` is the antenna separation the attitude solution depends on, and
`reset` flags a receiver reset.

### Magnetometer

`[Magnetometer] [read frequency Hz] [PortName] [BaudRate]` · flown at 40 Hz, `\\.\COM6`, 57600

A **third** attitude reference, and the only absolute one — magnetic field vector plus a derived
orientation.

| Channel | Type |
|---|---|
| `heading` | `DPFLOAT_I` |
| `pitch` | `DPFLOAT_I` |
| `roll` | `DPFLOAT_I` |
| `magLength` | `DPFLOAT_I` |
| `magX` | `DPFLOAT_I` |
| `magY` | `DPFLOAT_I` |
| `magZ` | `DPFLOAT_I` |

Both the raw vector (`magX/Y/Z`), its magnitude (`magLength`), and the derived angles are
recorded. Keeping the raw components means the derivation can be redone on the ground.

### ClockSync — the time base

`[ClockSync] [frequency Hz]` · **enabled at 1 Hz**

| Channel | Type |
|---|---|
| `filetime` | `UINT64_I` |

**This is the device that makes a recording self-dating.** `bd_timer` on every record is a raw
`rdtsc` count, not a wall clock. `ClockSync` samples the Windows `FILETIME` in `TickImpl`:

```cpp
m_Data.filetime = InternalTime::internalTime::Now();   // GetSystemTimeAsFileTime, 100 ns units
```

**One channel is all it needs**, which is the neat part. The record's own `bd_timer` already holds
the `rdtsc` for the same tick, so a single `ClockSync` record *is* a `(tsc, filetime)` pair — the
counter in the header, the wall clock in the payload. Fit those across a file and you recover the
counter's rate and origin.

An earlier version declared a second channel, `cpu`, holding the TSC explicitly; the field and its
channel definition were both removed once it was clear the record header already carried it. The
calibration did not get weaker, it got deduplicated.

It is also one of only two devices in the telemetry selection, so the pair reaches the ground as
well as the disk. `TestComputerClock` is a standalone spike on the same problem and is not used by
the flight program.

Details for data users in
[`FILE-FORMAT.md`](FILE-FORMAT.md#clocksync-supplies-the-calibration).

### Telescope — the science instrument

`[Telescope] [frequency Hz] [PortName] [BaudRate]` · **enabled at 10 Hz**, `\\.\COM4`, 9600 ·
GUID `A38A47EE-D74E-479a-A2C1-FE7EFDC2C4CF`

**49 channels of its own**: `Rev`, plus **16 detector channels × three Stokes parameters**, all
`SPFLOAT_I`. On disk the dictionary reports **51**, because `timedT<T>` prepends `computerClock`
and `index` — giving a 205-byte record (8 + 1 + 4 + 48×4).

| Channel | Type | Meaning |
|---|---|---|
| `Rev` | `SPFLOAT_I` | revolution / rotation index |
| `Channel 00T` … `Channel 15T` | `SPFLOAT_I` | total intensity |
| `Channel 00Q` … `Channel 15Q` | `SPFLOAT_I` | Stokes Q — linear polarization |
| `Channel 00U` … `Channel 15U` | `SPFLOAT_I` | Stokes U — linear polarization at 45° |

`T`/`Q`/`U` is polarimetry vocabulary: **this is a 16-channel polarimeter**, and it is what the
whole apparatus exists to serve. The three attitude references (LN-250, Ashtech, magnetometer)
are there to say where each sample was pointing, and `Rev` is the modulation index the Q and U
demodulation depends on — which is why it sits beside them in the same record rather than being
derived later.

Everything else in this catalogue is supporting instrumentation. This is the payload.

Its `Configure` is also the newer, composable style: it consumes the frequency itself and
delegates the rest to its serial port object —

```cpp
SetFrequency(UCSBUtility::ToINT<DWORD>(*beg++));
return m_port.Configure(beg, end);
```

— so the port name and baud rate are parsed by the port, not by the device. Older devices
(`Ashtech`, `Magnetometer`) parse all three inline.

### Digital Counter — a known-good signal

`[Digital Counter] [frequency Hz]` · **enabled**, 10 Hz

| Channel | Type |
|---|---|
| `ticks` | `ULONG_I` |
| `ones` | `ULONG_I` |
| `tens` | `ULONG_I` |
| `hundreds` | `ULONG_I` |
| `thousands` | `ULONG_I` |

Synthetic, not hardware: `TickImpl` increments a counter and decomposes it into decimal digits
(`CounterSource.h:36`). It is a **test pattern** — a channel whose correct value at every instant
is known in advance.

**Its practical use is dropped-frame detection.** `ticks` increments once per tick and never
skips, so in any recording where this device was enabled, **a gap in `ticks` is a lost frame** —
in the recording chain, over the telemetry link, or in a converter. It is the only integrity
signal here that is independent of any clock, and it is strictly better than the record timestamp
for the purpose: `bd_timer` can tell you that time passed, while `ticks` tells you how many frames
*should* have been there. Its presence in `telemetrySend.cfg` alongside the LN-250 shows it was
downlinked deliberately for that check.

The decimal decomposition (`ones`/`tens`/`hundreds`/`thousands`) gives the same information
redundantly in a form that is readable at a glance on the console display, and that would make
single-bit corruption visible as a digit that disagrees with `ticks`.

### MMC Device — discrete inputs

`[MMC Device]` · **enabled**, no parameters

| Channels | Type |
|---|---|
| `bit0` … `bit7` | `UCHAR` |

Eight single-bit discretes, one byte each. No configuration at all — the only device in the
catalogue that takes zero parameters.

### DaqCOM — 32-channel analogue *(not built)*

`[DaqCOM] [read frequency Hz] [number of scans]`

32 channels named `Channel 00` … `Channel 31`, all `SPFLOAT_I`. Generic names, so what each was
physically wired to is not recorded in the source.

`DAQCom.cpp` is present in `FlightCode-01\` but **excluded from the project**, so this device does
not register in the flight program. See
[`ARCHITECTURE.md`](ARCHITECTURE.md#which-devices-are-actually-live).

### IOTech — DAQ board *(not in FlightCode-01)*

`[IOTech] [board name] [read frequency Hz] [number of scans] [ClearOnZ true|false]`

Example board name from the config: `DaqBoard3031USB{325188}` — a Measurement Computing /IOtech
DAQ board with its serial number embedded. Declared in `Include\IOTech.h`, compiled into
`EncoderReader` and `TestICD` rather than the flight program. `Include\cbw.h` is the vendor's SDK
header.

### Encoder *(orphaned)*

`EncoderWrapper` (`Encoder.cpp:21`), GUID `3C452666-DBEA-42ff-93C1-F6D0A183161B`. `Encoder.cpp`
is in **no project at all**, so this never registers anywhere.

It is not forgotten, though — `telemetrySend.cfg` carries a commented-out block for exactly this
GUID:

```
; [3C452666-DBEA-42ff-93C1-F6D0A183161B]  [1]
; [Encoder]
```

So an encoder channel was downlinked at some point and both the code and its telemetry selection
were disabled together. Curiously, `EncoderWrapper::GetName()` returns
`"DaqBoard3031USB{325188}"` (`Encoder.cpp:42`) — a board identifier rather than a device label,
and the same string `[IOTech]` takes as its first parameter.

---

## How the telemetry selection maps back

`telemetrySend.cfg` selects downlink channels by **device GUID**, and the GUIDs are the ones in
the table above — deliberately. From `Telemetry.cpp:176`:

> *"In order to preserve the ability to see which devices are which, the new dictionary relies on
> the old dictionary's device GUIDS. It merely adds fewer channels."*

A GUID for the telemetry block itself was written and then commented out
(`Telemetry.cpp:172`) in favour of that reuse. So the two dictionaries — recorded and downlinked —
disagree about device *numbers* but agree about device *identities*, which is the property that
lets a ground station and an archive be reconciled later.

What the flown selection actually carried:

| Device | Channels downlinked |
|---|---|
| LN-250 (`3EE2B4C5…`) | `computerClock`, `index`, `SystemTimer`, `GPSTime`, `HybridPitchAngle`, `HybridRollAngle`, `HybridYawAngle` |
| Digital Counter (`D6FE3751…`) | `computerClock`, `index`, `ticks` |

Seven of the LN-250's fifteen channels, and the counter as an integrity reference. Latitude,
longitude and heading are present but commented out — one of them, `HybridHeadingAngle`, with a
`:)` left beside it.

---

## Open questions

- **`IOTech`'s channel list and GUID** — not extracted; the class is not in the flight program.
- **`EncoderWrapper`'s channels** — not extracted, file is in no build.
- **What the 32 `DaqCOM` channels were wired to.** Generic names, and no wiring record in the
  source.
- **What `MMCSource`'s eight bits represent**, and what "MMC" stands for here.
- **`TestDevice`** (`DataSource.cpp:15`) registers privately in every project that compiles
  `DataSource.cpp`, including the flight program. Its channels and purpose were not examined.
- **Physical units for the Ashtech and Magnetometer channels — unresolved.** The LN-250's units
  are documented in `LN250.h`, transcribed from its interface document. The other two instruments
  have no equivalent comments, so whether their angles are degrees or radians is not recorded
  anywhere in this tree, and the vendor manuals are not present. Resolving them will need the
  original documentation or inference from recorded data — angles are distinguishable by range.
- **What `Telescope`'s `T`/`Q`/`U` values are in** — raw ADC counts, volts, or calibrated
  antenna temperature. They are `SPFLOAT_I`, which suggests something already converted rather
  than raw counts, but nothing states it.
- **What `Rev` counts** — a rotation index of some kind, and the demodulation reference for Q and
  U, but its units and origin are not recorded.
- **Which of the 16 `Telescope` channels correspond to which detectors or frequency bands.** The
  names are positional only.

**The recordings in this repository cannot answer them.** `FlightCode-01\20121220\` holds seven
`.spaceball` files from 20 December 2012, but they are **development test recordings made while
working on the software**, not flight data. `Telescope` is configured and appears in every file's
dictionary with all 49 channels declared — and emits **zero samples**. The only data is from
`ClockSync` and the two `Digital Counter` instances.

Verified with [`tools\SpaceballToJson`](tools/SpaceballToJson/README.md) across all seven files:
13,000+ records, none of them from the telescope.

That makes these files an excellent test of the *recorder* and useless as a source of *science*.
Answering the questions above needs a recording made with the instrument attached and responding.
