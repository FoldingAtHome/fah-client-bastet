# fah-client Architecture

This document describes how `fah-client` is structured.  Read it before
making non-trivial changes to the client and before writing tests.

## What the client does

fah-client is an event-loop-driven daemon.  At a high level it:

1. Requests a *work unit* (WU) assignment from a Folding@home **Assignment
   Server** (AS).
2. Downloads the WU's data from the **Work Server** (WS) the AS returned.
3. Downloads the science **core** binary for that WU (cached across WUs of
   the same type).
4. Runs the core as a subprocess on the WU.
5. Uploads results to the WS (or to a **Collection Server** (CS) if the
   WS is down).
6. Repeats, scaled to the user's CPU and GPU resources.

The user's browser-based control panel ("the frontend") connects to a
local HTTP/WebSocket port the client serves, and observes/mutates the
client's state.  A separate WebSocket to api.foldingathome.org optionally
links the client to a user account so remote control can also work.

## Major types

The whole client lives in `FAH::Client::` under `src/fah/client/`.

### App (`App.h/cpp`)

The singleton.  Owns:
- libevent base, HTTP client, SSL context.
- SQLite database (`client.db`).
- RSA keypair (signs AS/WS requests).
- The `info` dict, `config`, `groups`, `units`, `gpus` as observable JSON
  children — frontends see these via the `Remote` interface.
- `Server`, `Account`, `GPUResources`, `Cores`, `OS`, `LogTracker`.
- A list of attached `Remote`s.

`App::init` runs command-line/option setup.  `App::run` opens the DB,
calls `loadConfig`, constructs `Groups` / `Units` / `Server`, and enters
the event loop via `os->dispatch()`.

### Server (`Server.h/cpp`)

The HTTP/WebSocket server the local frontend connects to (typically on
`127.0.0.1`).  Routes WS connections to `WebsocketRemote` instances.

### Remote (`Remote.h/cpp`)

Abstract base for any external observer/controller.

- `WebsocketRemote` — a local browser frontend connected to Server.
- `NodeRemote` — a remote browser connected via the Account WS bridge.

A Remote receives every change to the observable JSON tree
(`App::notify` → `Remote::sendChanges`), can send commands back (config
edits, state changes), and watches the log/viewer streams.

### Account (`Account.h/cpp`)

Optional WebSocket connection to `api.foldingathome.org`.  When the user
links the client to an account, this bridge lets remote browsers control
this client.  Has its own state machine: `IDLE → LINK → INFO → CONNECT
→ CONNECTED`, with backoff.

### Groups / Group (`Groups.h/cpp`, `Group.h/cpp`)

A **Group** is a resource pool with its own `Config` (CPU count, GPU
list, pause state, project preferences).  The default unnamed group
inherits its config from the command-line options; additional groups
can be added at runtime by remotes.

`Group::update()` is the **scheduler** (`Group.cpp:183`).  Each tick it:
1. Triggers each unit's `next()`.
2. Reaps completed units.
3. Honors graceful shutdown.
4. Bails if paused, idle-waiting, battery-waiting, or assigning.
5. Allocates GPUs (each GPU WU gets its minimum CPUs).
6. Distributes extra CPUs to enabled GPU WUs up to their max.
7. Distributes remaining CPUs to CPU WUs.
8. Sets `pause` on units that didn't get resources.
9. Creates a new Unit (calls `Units::add`) if under the max-WU cap and
   any resources are still free.

### Config (`Config.h/cpp`)

A JSON dict subclass with type-checked insertion (rejects keys not in the
defaults template, rejects wrong types).  Each Group has its own Config
loaded from a JSON defaults resource plus DB-persisted overrides plus
command-line options.

### Units / Unit (`Units.h/cpp`, `Unit.cpp` — 1286 lines, the heart)

A `Unit` is one WU in flight.  It is the largest and most intricate
class in the client.  Its state machine is the `UnitState` enum:

```
                       +---retry on failure---+
                       v                      |
ASSIGN ──► DOWNLOAD ──► CORE ──► RUN ──► UPLOAD ──► DONE
                       │        │       ▲
                       │        │       │
                       │        └─► DUMP┘
                       └─► DUMP ──► DONE  (dumped/expired/failed)
```

`Unit::next()` (`Unit.cpp:431`) is the state-machine dispatcher.  Every
trigger (timer event, HTTP response, core exit, pause toggle) calls
`next()`, which checks expiry/pause/waiting and then dispatches to
`assign()`, `download()`, `getCore()`, `run()`, `upload()`, or `dump()`
based on `getState()`.

Per-state functions:

- `assign()` — POSTs a signed JSON request to the next AS in rotation,
  waits for `assignResponse()`.
- `download()` — POSTs to the WS returned by the AS, writes
  `work/<id>/wudata_01.dat`, transitions to `UNIT_CORE`.
- `getCore()` — asks `Cores` to download the WU's science core (cached
  by URL), callback transitions to `UNIT_RUN` and auto-pauses pending
  scheduler approval.
- `run()` — spawns `CoreProcess` with arguments derived from
  CPU/GPU/UUID config.  Starts `TailFileToLog` to mirror core output into
  the client log.
- `monitorRun()` — every second while running: reads `wuinfo_01.dat` for
  progress, reads viewer frames, updates ETA/PPD, detects clock skew.
- `finalizeRun()` — runs after core exits.  Examines `ExitCode`:
  `FINISHED_UNIT`/`INTERRUPTED`/`CORE_RESTART`/`FAILED_*`.  Reads
  `wuresults_01.dat`, signs results, transitions to `UNIT_UPLOAD` (or
  `UNIT_DUMP` on bad/missing data, or retries on `CORE_RESTART`).
- `upload()` — POSTs results.  On failure, retries against the CS list
  before counting the failure.
- `dump()` — submits a "dumped" report and cleans up.

Retry policy (`retry()`, `Unit.cpp:920`): exponential backoff
`2^min(9, retries)` seconds, with limits that depend on state.  Beyond
the limit the unit is cleaned with reason `"retries"`.

Persistence: every state-changing transition past `UNIT_CORE` writes the
unit's full JSON state to the DB.  On startup, `Units` rehydrates units
from the DB; if a unit was in `UNIT_RUN`, it's reset to `UNIT_CORE` so
the core is verified/redownloaded if needed.

### Core / Cores / CoreProcess

`Cores` is a registry, indexed by core URL.  A `Core` has its own state
machine `INIT → CERT → SIG → DOWNLOAD → TEST → READY` (or `INVALID`).
Cores are cached on disk under `cores/` and shared across all Units that
need the same one.  `CoreProcess` is a thin `cb::Subprocess` wrapper.

### GPUResources / GPUResource

`GPUResources` enumerates the local GPUs at startup via PCI, CUDA, HIP,
and OpenCL.  Each `GPUResource` stores vendor/device IDs, driver
descriptors, and a `supported` flag.  Used by Group's scheduler and
referenced by `Unit::run()` to build core arguments.

### OS

Platform-specific (`lin/`, `osx/`, `win/` subdirs).  Provides system idle
detection, battery state, keep-awake hints, and platform-specific exit
handling.  Polls every 2 seconds.

### LogTracker

Captures log lines through the cbang Logger machinery and forwards them
to attached Remotes.

## Event flow

```
   ┌──────────────────────────────────────────────────────────────┐
   │ libevent base (App::base)                                    │
   │                                                              │
   │  ┌────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────────┐ │
   │  │ Signals│  │  HTTP    │  │  Account │  │ Server (frontend│ │
   │  │SIGINT  │  │  client  │  │   WS     │  │   WS in)        │ │
   │  │SIGTERM │  │ (AS/WS)  │  │          │  │                 │ │
   │  └───┬────┘  └────┬─────┘  └────┬─────┘  └────────┬────────┘ │
   │      │            │             │                 │          │
   │      ▼            ▼             ▼                 ▼          │
   │             ┌──────────────────────────────────────┐         │
   │             │       App (observable JSON dict)     │         │
   │             │  config • groups • units • info • gpus│        │
   │             └──┬────────────────┬────────────┬─────┘         │
   │                │                │            │               │
   │      ┌─────────▼────┐ ┌─────────▼──────┐ ┌───▼────────────┐  │
   │      │   Groups     │ │   Units        │ │   Remotes      │  │
   │      │ (scheduler   │ │ (state         │ │ (notified on   │  │
   │      │  per group)  │ │  machines)     │ │  any change)   │  │
   │      └──────────────┘ └────────────────┘ └────────────────┘  │
   │                                                              │
   │                    OS event (2s tick)                        │
   │                    Save event (debounced config write)       │
   └──────────────────────────────────────────────────────────────┘
```

Every state change in any observable JSON child triggers `App::notify`
(`App.cpp:550`), which forwards a JSON change-list to all Remotes.
The frontend sees the same JSON tree the client owns.
