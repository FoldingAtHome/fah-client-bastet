# Possible Testing Directions

See ARCHITECTURE.md.

## External dependencies (test isolation points)

Anything we want to test in isolation has to be replaceable here:

| Dependency | Where it lives | Effect |
|---|---|---|
| Assignment Servers | `assignment-servers` option | HTTP POSTs from `Unit::assign()` |
| Work Servers | URL in AS response | `Unit::download()`, `Unit::upload()` |
| Collection Servers | URL list in WU data | Fallback upload destinations |
| `api.foldingathome.org` | `api-server` option | `Account` WS connection |
| Core URLs (download) | URL in AS response | `Core::download()` |
| FahCore binaries (exec) | `cores/...` | `CoreProcess::exec()` |
| Filesystem | `work/<id>/`, `cores/`, `credits/`, `client.db` | WU data, results, credits, persistence |
| Time | `cb::Time::now()` | Backoff, deadlines, clock skew |
| System info | `cb::SystemInfo` | CPU count, machine ID, memory |
| GPU detection | `cb::PCI`, OpenCL/CUDA/HIP probes | `GPUResources::detect()` |
| Power management | `cb::PowerManagement` | Battery, idle, keep-awake |
| Signals | `SIGINT`, `SIGTERM`, `SIGHUP` | Shutdown |

## Wire-protocol artifacts

These are part of the public API between client, AS, WS, and frontend.
Changes here break compatibility:

- `UnitState` enum names (`UNIT_ASSIGN`, `UNIT_DOWNLOAD`, `UNIT_CORE`,
  `UNIT_RUN`, `UNIT_UPLOAD`, `UNIT_DUMP`, `UNIT_DONE`) — JSON values.
- `CoreState` enum names.
- `ExitCode` enum values (numeric, set by science core binary).
- AS request/response JSON shape — see `Unit::writeRequest`,
  `Unit::assignResponse`.
- WS download/upload/dump JSON shape — see `Unit::downloadResponse`,
  `Unit::upload`, `Unit::dump`.
- The observable JSON tree the frontend sees — see `App::loadConfig`
  for the `info` shape and the default group JSON
  (`src/resources/group.json`) for `config`.

## Critical functionality worth testing

In approximate priority order:

1. **Unit state machine progression** — happy path
   `ASSIGN → DOWNLOAD → CORE → RUN → UPLOAD → DONE`.
2. **Retry / backoff** — failure responses trigger correct delay,
   correct retry-cap behavior, switch to CS list on upload failure.
3. **Dump path** — invalid response, missing results, expired WU, user
   "dump" command from all reachable states.
4. **Pause / resume** — during each state, scheduler-driven pause vs.
   user-driven pause, finish-on-complete semantics.
5. **Group scheduler resource allocation** — CPU/GPU partition under
   competing units, max-WU cap, finish mode, GPU minimum-CPU requirement.
6. **Persistence** — unit serialized to DB across each transition;
   restart loads units and resumes correctly; `UNIT_RUN` is reset to
   `UNIT_CORE` on load.
7. **Core process exit-code handling** — every `ExitCode` mapped to the
   correct next state in `finalizeRun()`.
8. **Signature verification** — AS/WS response with bad cert / bad
   signature / wrong key-usage is rejected (negative tests).
9. **Clock skew detection** — large `Time::now()` jump during run gets
   detected and time estimates adjusted.
10. **Account state machine** — link/connect/retry with backoff.
11. **Config merge** — defaults + DB + options + remote updates.

## Where to look next

- `Unit.cpp:431` — `next()`, the state-machine dispatcher.
- `Group.cpp:183` — `update()`, the resource scheduler.
- `App.cpp:480-528` — `init()`/`run()`, the startup path.
- `src/resources/group.json` — default config shape (the JSON the
  frontend sees).
- `Account.cpp` — the account-bridge state machine, the part of the
  client most likely to surprise you.
