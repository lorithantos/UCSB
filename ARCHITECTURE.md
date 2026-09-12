# UCSB — Architecture

**A Windows data-acquisition program for a balloon-borne polarimeter, developed 2010–2012.**
It samples a dozen instruments on independent threads and writes them into one
self-describing binary stream. This document explains how the pieces find each other,
because almost nothing about that is visible from reading `main`.

> Describes the **December 2012** working copy. The parent commit in this repository is an
> earlier mid-2011 snapshot; neither came from version control, so the diff between them is
> roughly eighteen months reconstructed after the fact. Claims about what is *unfinished* are the
> ones most likely to be snapshot-specific — several things stubbed in 2011 were complete by 2012.

Written from the code as it stands. Where I could not verify something from the source,
it says so — see [Open questions](#open-questions).

---

## Orientation: what you are looking at

Source files are stamped July 2011. Every project carries both a `.vcproj` (VS2008) and a
`.vcxproj` (VS2010), and `UpgradeLog.XML` plus `_UpgradeReport_Files\` sit at the root: this
is a VS2008 codebase that was run once through the VS2010 upgrade wizard and then stopped.
The `vc100` artifacts under `Debug\` are from that toolchain.

Nothing here is mid-flight. The idioms that look odd — the CRTP registration template, the
`BYTE`/`DWORD`/`GUID` vocabulary, the hand-rolled double buffer — are period-appropriate and
settled. Read them as finished, not as work in progress.

Domain vocabulary you will meet with no introduction:

| Term | What it means |
|---|---|
| **COFE** | The experiment. Named only in `UCSB.sln`, in a path to flight code that is not in this tree. |
| **Spaceball** | The name of the output data format. Several projects exist only to convert it. |
| **FITS** | The astronomy-standard file format the data is eventually converted *to*. |
| **ICD** | Interface Control Document — the wire-format spec. `TestICD\` exercises it; `Manuals\` may hold it. |
| **LN-250** | A Northrop Grumman inertial navigation unit. Its message framing became the container format for *everything* — see [The file format](#the-file-format). |

### The first five minutes will confuse you

1. **Three of the 21 projects in `UCSB.sln` do not exist in this tree.** `FLIR Capture to FITS -
   Original`, `MainFlightCode_new`, and `DAQ Samples` point at `Programs\...`, `Software\...`
   and `DAQ Samples\` — none of those directories are here. Visual Studio will report them as
   unloadable. This is expected, not a broken checkout.
2. **Most of the 21 projects are not the program.** The flight program is `FlightCode-01`.
   Everything else is a hardware spike, a test harness, or a downstream converter — see
   [What each project is](#what-each-project-is).
3. **`UCSB.sdf` is 36 MB and is not source.** It is the VS2010 IntelliSense database. So is
   `UCSB.suo` (user options) and `ipch\`. All three are regenerable build detritus.

---

## The spine: devices register themselves

This is the piece to understand first, because it is why `main` contains no list of
instruments and no `switch` over device types.

### The mechanism

Three cooperating parts in `Include\DataSource.h` and `Include\DataSource.cpp`:

**1. The registry** — `DataSource.cpp:40`

```cpp
std::map<std::string, CreateDeviceFn>& DeviceFactoryMap()
{ static std::map<std::string, CreateDeviceFn> retv; return retv; }
```

A function-local static: the map is constructed on first use, which is necessarily the first
registration. This is what makes registration order irrelevant — the classic
static-initialization-order problem needs a namespace-scope container, and this deliberately
is not one.

**2. `RegisterHelper<T>`** — `DataSource.h:114`

Its *constructor* performs the insertion, keyed on `T::GetName()` lowercased. Its `Create`
static is the factory: `new T`, then `SetDeviceHolder`.

**3. `AutoRegister<T>`** — `DataSource.h:143`

A CRTP base holding `static RegisterHelper<T> autoRegistration`, defined out-of-line at
`DataSource.h:163`. Constructing that static *is* the registration. A device opts in purely by
deriving:

```cpp
class Magnetometer : public nsDataSource::DataSource,
                     public nsDataSource::AutoRegister<Magnetometer>
```

### Why the `dummyVar` nonsense

Both templates carry the same comment — *"Yes this is stupid. Don't remove it unless you have
verified that the compiler is not optimizing away the very effect I am trying to create here."*

`AutoRegister`'s constructor calls `autoRegistration.dummyVar.c_str()` and throws the result
away; `RegisterHelper` holds a `std::string dummyVar` that exists only to be touched.

This is not superstition. **A static data member of a class template is only instantiated if it
is odr-used.** Without something referencing `autoRegistration`, the compiler is entitled to
never instantiate it, the constructor never runs, and the device silently never registers.
Touching the member forces instantiation. That is also why `AutoRegister` takes an unused `int`
— it gives every derived class a constructor to call, which is what drags the touch in:

```cpp
Magnetometer::Magnetometer(void) : AutoRegister<Magnetometer>(0)
```

The `0` threaded through every device's initializer list is doing that job and nothing else.

**This is safe here, and it is worth knowing why it stays safe.** `FlightCode-01` is
`<ConfigurationType>Application</ConfigurationType>` with every device `.cpp` as a direct
`ClCompile` member — no static library, so no archive-member selection, so every translation
unit is linked unconditionally. The failure mode this idiom guards against needs a linker that
can drop an object nobody references, and this build has no such step.

### What the pattern was for

`DataSource.h` is a **plugin API**, written deliberately so that a colleague could add a new
instrument without touching any central list, registry, factory or `switch`. On that goal it
works. Adding a device requires no edit to any existing file — you write one header, and the
program picks it up.

Here is the complete contract, drawn from `CounterSource` (`FlightCode-01\CounterSource.h`,
118 lines total, of which the genuinely novel logic is eight):

| You must supply | Why | Example |
|---|---|---|
| Derive from `DataSource` **and** `AutoRegister<Self>` | Registration | `class CounterSource : public DataSource, public AutoRegister<CounterSource>` |
| `: AutoRegister<Self>(0)` in the constructor | Forces the static to instantiate | `CounterSource::CounterSource(void) : AutoRegister<CounterSource>(0)` |
| `static std::string GetName()` | The config-file keyword | `return "Digital Counter";` |
| `static GUID GetClassGUID()` | Permanent identity in the stream | a literal `GUID` |
| `static vector<ChannelDefinition> GetClassDescription()` | Declares the channels | `{"ULONG_I", "ticks"}, …` |
| A POD struct of the sample data | What gets written | `struct localData { DWORD ticks; … }` |
| `AddDeviceTypes()` | One line | `GetDeviceHolder().AddDeviceType(*this);` |
| `Start()` | One line | `GetDeviceHolder().RegisterWriter(*this, GetClassGUID());` |
| `TickImpl(now)` | **The actual work** | acquire a sample, then `WriteDataWithCache(now, *this, m_Data)` |
| `Configure(beg, end)` | Optional | consume your tokens, return the new iterator |

Two things about that design are worth calling out as good, because they are load-bearing:

**The channel description is authored next to the data it describes.** `localData` declares its
fields and its `GetClassDescription()` in the same struct, a dozen lines apart. The wire format
and the in-memory layout are written together, which is what keeps them in step.

**And a mismatch is caught, not merely discouraged.** When a device writes, `DeviceHolder`
compares the dictionary's declared size against `sizeof` the actual payload and throws
`"Fatal programming error"` on disagreement (`UCSB-Datastream.h:640`). Add a field to `localData`
and forget the description and the program stops immediately rather than silently writing
records that no reader can interpret.

### Why it broke brains anyway

The pattern is cheap to *use* and expensive to *believe*, and those are different costs paid by
different people. Four specific reasons, worth knowing because they are what a newcomer runs into
in order:

1. **There is no call site.** Someone asking "where do devices get added?" greps for a registry,
   a list, an `Add` — and finds nothing, because the answer is "during static initialization, via
   a base class in the declaration you already skimmed past." Nothing in `main` mentions a device
   by name. The mechanism's whole virtue — no central list to edit — is also why there is nothing
   to read.
2. **Its one visible artifact looks like a mistake.** `autoRegistration.dummyVar.c_str();`,
   discarding the result, under a comment that opens *"Yes this is stupid."* A reader who does not
   already know the odr-use rule cannot tell whether that line is essential or leftover debris —
   and the comment, honest as it is, does not actually say *why* it works. Hence the
   [explanation above](#why-the-dummyvar-nonsense), which is the thing the comment assumes you
   already know.
3. **The `(0)` is meaningless at the call site.** `AutoRegister<CounterSource>(0)` reads like a
   placeholder argument. It is in fact the entire reason the registration fires.
4. **The static contract cannot be declared, so violating it is diagnosed badly.** `GetName`,
   `GetClassGUID` and `GetClassDescription` are *required*, but they are `static` — so in C++03
   they cannot be pure virtuals and there is no base class that states the requirement. Omit one
   and the compiler reports a failure deep inside `RegisterHelper<T>`, pointing at library code,
   rather than saying "your device is missing `GetName`." **This is the one that costs the most
   time, and it is a limitation of the language available in 2010 rather than a flaw in the
   design** — a modern equivalent would state the requirement as a concept and name the missing
   member directly.

None of this makes the pattern the wrong choice; the alternative — a central registry every
contributor edits — trades these costs for merge conflicts and a file that is wrong whenever
someone forgets it. But the trade has a shape worth naming: **self-registration optimizes for
whoever writes the fourteenth device and charges whoever reads the first.** This document exists
to refund that charge.

### Lineage: where the pattern comes from

This is a riff on **Andrei Alexandrescu's *Modern C++ Design*** (2001) and its Loki library —
specifically Chapter 8, *Object Factories*, whose `Factory` template pairs `Register(id, creator)`
with `CreateObject(id)`. `DeviceFactoryMap()` keyed on a lowercased name string, with
`RegisterHelper<T>::Create` as the creator function, is that design.

The influence is corroborated elsewhere in the tree, in code with nothing to do with devices.
`Include\utility.h:37` is the book's compile-time assertion idiom almost verbatim:

```cpp
template <bool test>
inline void STATIC_ASSERT_IMPL()
{
    char STATIC_ASSERT_FAILURE[test] = {0};   // array of size 0 is ill-formed
}
```

— including the detail that the identifier is named so that it *reads* in the compiler's
diagnostic, which was Alexandrescu's actual point about compile-time errors. In 2010, nine years
after publication and with C++03 as the ceiling, this was the state of the art.

**But the riff goes a step past the source, and that step is the whole story.** Loki's canonical
self-registration is an *explicit call* — typically a namespace-scope variable in the device's own
`.cpp`:

```cpp
namespace { const bool registered = Factory::Instance().Register("thing", CreateThing); }
```

Here, registration is **inherited instead of called**. Deriving from `AutoRegister<T>` *is* the
registration; there is no statement anywhere that performs it. That is a genuine improvement on
the ergonomics — it removes the one line a contributor could forget, and forgetting it in the Loki
form fails silently at run time.

And it is exactly what creates the `dummyVar` problem. A namespace-scope variable in a linked
translation unit is always initialized; **a static data member of a class template is only
instantiated if it is odr-used.** Moving registration from a variable into a template base bought
the ergonomics and inherited the instantiation rule, so something had to reach out and touch the
member to force it into existence.

So the elegance and the brain-breaking have one root cause, which is the honest summary of this
design: making registration something you *inherit* rather than something you *call* is what
removed the forgettable line, and is also why the machinery that replaced it cannot be read at the
place it takes effect.

### From config text to live objects

`FlightCode-01.cpp:96` → `DataSource.cpp:75` → `DataSource.cpp:46`

```
InterpretConfigurationFields(L"Devices", CreateDataSources)   // FlightCode-01.cpp:111
  └─ CreateDataSources(vector<string> const& fields)          // FlightCode-01.cpp:96
       └─ CreateDevices(begin, end, g_devices)                // DataSource.cpp:75
            └─ CreateDevice(...)  [per device, in a loop]     // DataSource.cpp:46
                 ├─ name = ToLower(*begin++)
                 ├─ DeviceFactoryMap().find(name)
                 ├─ deviceCreator->second(devices)   → new T
                 └─ dataDevice.Configure(begin, end) → returns new iterator
```

The configuration is **a flat vector of strings**, and device selection and device
configuration share it. Each device's `Configure` consumes as many tokens as it wants and
returns the position where the next device's name begins. An unknown device name logs
`"Unable to find device type '%s'"` and abandons the rest of the list (`DataSource.cpp:60`).

The config text comes from **`Devices.cfg` on disk if it exists, otherwise from an embedded Win32
resource of the same name.** `InterpretConfigurationFields` (`Include\ConfigFileReader.h:355`)
makes that choice itself — it appends `.cfg` to the name, probes for the file, and falls back to
`InterpretConfigurationResourceFields`. The commented-out block at `FlightCode-01.cpp:113-120`
is the *older, hand-rolled* version of exactly that logic; it was refactored into the helper, not
removed.

**This is the operational lever on the whole program**: dropping a `devices.cfg` beside the
executable overrides the compiled-in device list with no rebuild. See
[`CONFIGURATION.md`](CONFIGURATION.md).

### Which devices are actually live

Fourteen classes derive from `AutoRegister` across the tree. They are **not** all in the flight
program, and being in the program is not the same as being enabled — see
[`CONFIGURATION.md`](CONFIGURATION.md).

| Device | Declared in | In `FlightCode-01`? |
|---|---|---|
| `Telescope` | `FlightCode-01\Telescope.h` | yes — **the science payload** |
| `Ashtech` | `FlightCode-01\Ashtech.h` | yes |
| `ClockSync` | `FlightCode-01\ClockSync.h` | yes |
| `CounterSource` | `FlightCode-01\CounterSource.h` | yes |
| `DisplaySource` | `FlightCode-01\DisplaySource.h` | yes |
| `LN250Reader` | `FlightCode-01\LN250.cpp` | yes |
| `Magnetometer` | `FlightCode-01\Magnetometer.h` | yes |
| `MMCSource` | `FlightCode-01\MMCSource.h` | yes |
| `Telemetry` | `FlightCode-01\Telemetry.h` | yes |
| `TelemetryReceiver` | `FlightCode-01\TelemetryReceiver.h` | yes |
| `TestDevice` | `Include\DataSource.cpp` | yes — registers **privately** |
| `DaqCOMSource` | `FlightCode-01\DaqCOM.h` | **no** — `DAQCom.cpp` is not in the `.vcxproj` |
| `EncoderWrapper` | `FlightCode-01\Encoder.cpp` | **no** — `Encoder.cpp` is in no project at all |
| `IOTech` | `Include\IOTech*` | no — used by `EncoderReader` and `TestICD` |

`DAQCom.cpp` and `Encoder.cpp` sit in the `FlightCode-01\` directory and are **excluded from the
build** — still, in the December 2012 tree, as they were in mid-2011. If you are looking for why a
DAQ or encoder device name in the config produces "Unable to find device type", that is the reason.
That they survived two and a half years excluded, without being deleted, suggests parked rather
than abandoned.

---

## The device contract

`DataSource` (`Include\DataSource.h:24`) is the interface every instrument implements.

| Member | Purpose |
|---|---|
| `TickImpl(now)` | **Pure virtual — the one thing no device can skip.** Called on the device's own thread at its own rate. |
| `AddDeviceTypes()` | Pure virtual. Declares this device's channels to the `DeviceHolder`. Runs for every device *before* any file is written. |
| `Configure(beg, end)` | Consumes its tokens from the config stream, returns the new position. Default consumes none. |
| `Start()` / `Stop()` | Lifecycle hooks. Both default to doing nothing. |
| `UpdateConfigField(field, value)` | Named-field configuration. Defaults to `false` — "no call should be made in that case". |
| `GetFrequency()` / `SetFrequency()` | The device's own sample rate. **Defaults to 101** (`DataSource.h:26`) — an odd number, presumably to avoid harmonics with other devices and the 10 ms main loop. |

Each device also takes a sequential `BYTE m_index` from a shared `m_lastDevice` counter at
construction, and holds `m_mostRecentData`. That second member is the tell for the concurrency
model: **this is a most-recent-value recorder, not a queue.** A device overwrites its latest
sample; the writer periodically snapshots whatever is there.

---

## The runtime

`_tmain` at `FlightCode-01.cpp:106` is short and worth reading in full. In order:

1. **Build devices** from the `Devices` resource (above).
2. **`AddDeviceTypes()` on every device** — the comment is explicit that this must complete
   before the first file is written, because the file header *is* the device dictionary.
3. **Open the first file** on a time boundary. `GetTimeBoundary(g_secondsBoundary)` rounds to a
   wall-clock boundary so files start at predictable times, not at process start.
4. **One thread per device**, via `CreateTimedThreadRef(**beg, (*beg)->GetFrequency())`. Each
   device ticks independently at its own rate.
5. **Main loop**, every ~10 ms:
   - poll the keyboard (`_kbhit`) — any key stops collection
   - `g_devices.WriteCurrentData(g_hFile)` — drain the buffer
   - `UpdateSharedFile(...)` — roll to a new file if the boundary has passed
6. **Shutdown**: `WaitForMultipleObjects(..., 1000)`, close thread handles, one final
   `WriteCurrentData`, close the file.

### File rolling

`UpdateSharedFile` (visible around `FlightCode-01.cpp:82-94`) does the swap in a specific order
worth noting: it writes the new file's header, then swaps `g_hFile` to the new handle, then
calls `Sleep(0)` before closing the old one. The comment explains the `Sleep(0)` — it yields
the thread slice "to minimize the likelihood of a file being closed while it is still needed."

That is a race being *narrowed*, not closed. Device threads and the writer reach `g_hFile`
without holding a lock on it. In 2011, on the hardware this flew on, it evidently held.

### The double buffer

`DeviceHolder` owns two byte vectors, `m_fileData[0]` and `m_fileData[1]`, with `m_pCache`
pointing at whichever is currently accepting writes. `GetLastCache()` (`UCSB-Datastream.h:818`)
takes a named mutex, flips the pointer, and returns the *old* buffer:

```cpp
std::vector<BYTE>* GetLastCache()
{
    ScopedMutex scope(m_cacheMutex.GetMutex(L"CacheMutex"));
    std::vector<BYTE>* pRetv = m_pCache;
    m_pCache = (m_pCache == &m_fileData[0]) ? &m_fileData[1] : &m_fileData[0];
    return pRetv;
}
```

`WriteCurrentData` (`UCSB-Datastream.h:678`) calls it, then writes and clears the returned
buffer **outside** the mutex — safe because producers are now appending to the other one. This
is the entire concurrency design: many producers, one consumer, one pointer flip.

---

## The file format

The output is a **self-describing binary stream**: a dictionary describing the instruments,
followed by timestamped payload records that reference it by number.

### The container is borrowed

This is the most surprising thing in the codebase and the hardest to guess:

```cpp
inline std::vector<BYTE> WriteBlockToFile(HANDLE hFile, T const& t, BYTE blockType)
{
    return LN250::SendLNMessage(hFile, t, blockType);   // UCSB-Datastream.h:16
}
```

**Every block written — dictionary entries and science data alike — is framed as an LN-250
message.** `CacheData` (`UCSB-Datastream.h:32`) is literally
`LN250::HighSpeedPortMessageHeader` + payload + a one-byte LN-250 checksum. The message
framing of one instrument was adopted as the container format for the whole recorder.

If you are trying to parse a Spaceball file and reaching for a format spec, start at
`Include\LN250.h`.

### The dictionary

Two record types, both in `UCSB-Datastream.h`, both under `#pragma pack(1)`:

- **`DeviceDefinition`** (`:149`) — `GUID deviceID`, `char displayName[30]`, `BYTE deviceNumber`,
  `BYTE channelCount`. The comment on `deviceNumber` is a warning worth heeding: *"This is the
  biggest potential source of corruption, since this number is meaningless without the
  dictionary."*
- **`ChannelDefinition`** (`:166`) — `GUID deviceGUID`, `char displayName[30]`,
  `BYTE channelIndex`, `BYTE dataType`. Channels key off the parent device's **GUID**, not its
  number, which is deliberate: a channel definition is identical regardless of how devices were
  ordered or configured that run.

`dataType` indexes a `DataTypes` enumeration, carrying its own shouted warning: *"Changes to
that enumeration invalidate ALL data formats that depend on it. DON'T DO THAT."* Treat that
enum as a permanent on-disk contract.

### The records

`timedT<T>` (`UCSB-Datastream.h:197`) wraps any payload with `BaseData`:

```cpp
struct BaseData { UINT64 bd_timer;  BYTE bd_index; };
```

— a 64-bit computer clock and the device index. `GetClassDescription()` builds a
field-name/type list by prepending `computerClock` and `index` to the payload type's own
description, which is how the stream stays self-describing without a separate schema file.

Two independent time sources are in play: the `UINT64` computer clock on every record, and the
`ClockSync` device, which exists to relate that clock to something external. `TestComputerClock`
is a spike for the same concern.

---

## What each project is

Of 21 solution entries, one is the program.

**The program**
- `FlightCode-01` — the flight data acquisition program. Start here.

**Shared code** (no project of its own; compiled into consumers via `..\Include\`)
- `Include\` — `DataSource.*` (the registry and device contract), `UCSB-Datastream.*` (the
  format and `DeviceHolder`), `LN250.h` (the framing), `utility.h`, `internaltime.h`,
  `IOTech.h`.

**Downstream converters** — turn Spaceball output into something usable
- `Spaceball2FITS`, `SpaceballConversion`, `SpaceballUtils` (C++)
- `ConvertSpaceballFiles` (**VB.NET** — one of only two managed projects)
- `ConvertSpaceballToText` — note the display name does not match its folder, which is
  `Test-UCSB-DataStream\`

**Hardware spikes** — one instrument each, to prove it could be read at all
- `EncoderReader`, `ReadADU`, `TestDaqBoard3000USB`, `FLIR-Capture`, `TimedDownload`

**Test harnesses and experiments**
- `TestICD` — exercises the interface control document
- `TestWaitableTimers` — the second caller of `CreateDevices` (`TestWaitableTimers.cpp:95`),
  driving the factory from a hand-built vector instead of the resource. **This is the usable
  seam for exercising devices without real hardware or real config.**
- `TestComputerClock`, `Test_CCITT0` (CRC), `ProducerConsumerTest`

**Error display**
- `ErrorDisplay`, `UCSB_ErrorDisplay` (the second is the other VB.NET project)

**Dangling — not in this tree**
- `FLIR Capture to FITS - Original`, `MainFlightCode_new`, `DAQ Samples`

---

## Open questions

Things a reader will want that I could not establish from the source. Stated as gaps rather
than guesses:

- **What the instrument measured.** COFE appears exactly once, in a solution path. The science
  is not described anywhere in the code.
- **The contents of `Manuals\`.** Not read. If the ICD or the LN-250 protocol spec exists, it is
  most likely there.
- **The `Devices` resource contents** — the actual device list and per-device parameters that a
  flight used. Locating the `.rc` entry would make the config stream concrete.
- **`vb hold\`** — a directory whose name suggests parked VB code. Unexamined.
- **Why `DAQCom.cpp` and `Encoder.cpp` are excluded from the build.** No history to consult.
- **The relationship between `ErrorDisplay` and `UCSB_ErrorDisplay`**, and between the three
  Spaceball converters — whether these are supersessions or genuinely different jobs.

---

## Reading order

If you have an hour:

1. `Include\DataSource.h:113-165` — the registration mechanism (20 lines that explain the shape
   of the whole program)
2. `Include\DataSource.cpp:40-92` — the factory and the config stream
3. `FlightCode-01\FlightCode-01.cpp:96-190` — `CreateDataSources` and `_tmain`
4. `Include\UCSB-Datastream.h:149-230` — the on-disk record types
5. Any one device — `Magnetometer.h` / `.cpp` is among the smallest — to see the contract met
