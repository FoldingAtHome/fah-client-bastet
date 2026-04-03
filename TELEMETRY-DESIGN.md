# Fleet Telemetry and Observability Design

This document outlines a plan for building fleet-wide observability into the
Folding@home client, with the goal of understanding how the fleet operates at
scale and enabling data-driven decisions about client development, project
scheduling, and infrastructure investment.

## Motivation

The FAH client fleet is large, heterogeneous, and volunteer-run.  Today we
have limited visibility into aggregate fleet behavior.  We can see individual
client state via Web Control and we receive per-WU data at assignment and
upload time, but we lack the ability to answer fleet-level questions like:

- What is the WU failure rate on AMD GPUs with driver version X?
- How does throughput differ between macOS and Linux for a given project?
- Which client versions have elevated error rates?  (rollout canary)
- What fraction of the fleet is idle vs. computing at any given hour?
- What hardware classes are underserved by available cores?
- Are error rates trending up after a release?

Building a telemetry pipeline and data warehouse will let us answer these
questions and catch problems before users report them.


## What the client already reports

The client already sends structured data at several points in its lifecycle.
Understanding this baseline is essential before adding new telemetry.

### Assignment request (`Unit::writeRequest`)

Sent to the assignment server via `POST /api/assign` once per work unit.
Contains the richest snapshot of client state:

| Field                      | Description                              |
|----------------------------|------------------------------------------|
| `id`                       | Client ID (pseudonymous, derived from key) |
| `version`                  | Client version string                    |
| `time`                     | ISO timestamp (replay prevention)        |
| `user`, `team`, `passkey`  | Donor identity and team                  |
| `account`                  | Account ID (if linked)                   |
| `os.type`, `os.version`    | Operating system                         |
| `os.memory`                | Free memory at request time              |
| `resources.cpu.*`          | CPU vendor, model, family, stepping, feature flags |
| `resources.<gpu_id>.*`     | GPU vendor/device IDs, CUDA/HIP/OpenCL platform and driver info |
| `project.cause`            | Cause preference                         |
| `project.beta`             | Beta project opt-in                      |
| `log.errors`               | Cumulative error count (new)             |
| `log.warnings`             | Cumulative warning count (new)           |

### Result upload (`POST /results` or WebSocket)

Sent when a WU completes, fails, or is dumped:

| Field                        | Description                            |
|------------------------------|----------------------------------------|
| `id`, `number`               | WU identity                            |
| `state`, `result`            | Final state and outcome                |
| `run_time`                   | Wall-clock runtime in seconds          |
| `start_time`, `end_time`     | ISO timestamps                         |
| `cpus`, `gpus`               | Resources used                         |
| `progress`, `wu_progress`    | How far the WU got                     |
| `ppd`                        | Points-per-day estimate                |
| `error`                      | Error message (if failed)              |
| `retries`                    | Number of retries before final outcome |
| `results.status`             | ok / failed / dumped                   |
| `results.sha256`             | Result data hash                       |

### Web Control WebSocket (local only)

Real-time observable state pushed to the local UI.  Includes all of the above
plus log streaming, visualization frames, and configuration.  Not currently
sent to any central service.

### What is missing

- **No periodic heartbeat to a central service.**  We only report at WU
  boundaries (assign and upload).  A client that is idle, misconfigured, or
  stuck between WUs is invisible.
- **No structured event stream.**  Log lines are free-text.  The new
  error/warning counts are a first step, but they collapse all context into
  two integers.
- **No crash reporting.**  A crashed core is detected by exit code and the WU
  is dumped, but no dedicated crash report is sent.
- **No server-side analytical store.**  Assignment and collection servers
  consume the data operationally but do not persist it for analytical queries.


## Proposed telemetry model

### Design principles

1. **Privacy by default.**  The client ID is already pseudonymous (derived
   from a generated key, not from PII).  Telemetry must not introduce new PII.
   Location should be coarsened to country level at the server, not sent by the
   client.  Donors should be able to opt out of non-essential telemetry.

2. **Telemetry must not affect the critical path.**  Upload failures, slow
   endpoints, or schema changes in the telemetry pipeline must never delay or
   break work unit processing.

3. **Structured events over free-text logs.**  Every telemetry payload should
   be a typed, structured JSON event with a well-defined schema.  Free-text
   log streaming remains available for debugging individual clients via Web
   Control but is not a fleet analytics tool.

4. **Batch and buffer.**  Events are accumulated locally and flushed
   periodically, not sent individually.  This is resilient to transient
   network issues and reduces server load.

5. **Sample high-frequency data.**  Per-step core performance can be
   pre-aggregated client-side (min/mean/max/p99 over a window).  Low-frequency
   events (WU lifecycle, errors) should be sent exhaustively.

6. **Start simple.**  A periodic heartbeat plus WU lifecycle events covers the
   majority of fleet-level questions.  Add more signals incrementally based on
   what questions the data cannot yet answer.


### Event types

All events share a common envelope:

```json
{
  "schema": 1,
  "type": "...",
  "ts": "2026-04-02T15:30:00Z",
  "client_id": "abc123",
  "client_version": "8.4.1",
  "os": "macosx",
  "os_version": "15.4"
}
```

The envelope fields are set once per batch (see Batching below) rather than
repeated per event.

#### Heartbeat

Sent every 15-30 minutes.  The single most valuable telemetry signal — it
turns invisible clients into visible ones.

```json
{
  "type": "heartbeat",
  "uptime_s": 86400,
  "state": "running",
  "on_battery": false,
  "is_idle": true,
  "cpus_configured": 7,
  "cpus_active": 7,
  "gpus_configured": ["gpu-0"],
  "gpus_active": ["gpu-0"],
  "wus_running": 2,
  "wus_completed_since_last": 3,
  "wus_failed_since_last": 0,
  "errors_since_last": 0,
  "warnings_since_last": 12,
  "mem_free_mb": 4096
}
```

Key design choices:
- **Delta counters** (`*_since_last`) rather than cumulative totals.  Deltas
  are easier to aggregate server-side without worrying about client restarts
  resetting counters.
- **`state`** is the client's overall state (running, paused, idle, finishing),
  not per-WU state.
- **GPU list** uses internal IDs that are stable across restarts.  The
  assignment request already maps these IDs to hardware details, so the
  heartbeat does not need to repeat GPU specs.

#### Work unit lifecycle events

These are the rows of the primary fact table.  Each represents a state
transition in the WU lifecycle.

```json
{"type": "wu.assigned",  "wu_id": "...", "project": 18735, "core": "openmm",
 "cpus": 1, "gpus": ["gpu-0"]}

{"type": "wu.started",   "wu_id": "..."}

{"type": "wu.completed", "wu_id": "...", "wall_time_s": 3600, "ppd": 1200000,
 "exit_code": 0, "steps": 500000}

{"type": "wu.failed",    "wu_id": "...", "wall_time_s": 120,
 "exit_code": 73, "error": "Core crashed", "retries": 2}

{"type": "wu.dumped",    "wu_id": "...", "wall_time_s": 60,
 "reason": "user_request"}
```

Key design choices:
- **One event per transition**, not one event with all fields.  This keeps
  each event small and allows the warehouse to reconstruct the full lifecycle
  by joining on `wu_id`.
- **`exit_code`** is the core process exit code.  This is one of the most
  useful fields for diagnosing fleet-wide core issues.
- **`error`** is a short category string, not the full log output.

#### Client lifecycle events

```json
{"type": "client.started",  "prev_shutdown": "clean"}
{"type": "client.stopped",  "reason": "user_request", "uptime_s": 86400}
{"type": "client.crashed",  "signal": 11, "uptime_s": 3600}
{"type": "client.updated",  "from_version": "8.4.0", "to_version": "8.4.1"}
```

#### Hardware census (low frequency)

Sent once on first start and again when hardware changes are detected (e.g.
GPU added/removed).  This populates dimension tables.

```json
{
  "type": "census",
  "cpu": "Apple M2 Max",
  "cpu_vendor": "Apple",
  "cpu_cores": 12,
  "cpu_perf_cores": 8,
  "mem_total_mb": 32768,
  "gpus": [
    {"id": "gpu-0", "name": "Apple M2 Max", "vendor": 0x106b,
     "device": 0x0000, "vram_mb": 32768, "platform": "metal"}
  ]
}
```


### Client-side architecture

```
  LOG_ERROR(...)   WU completed    Timer fires
       |               |               |
       v               v               v
  +-----------+  +-----------+  +-----------+
  | LogTracker|  |   Unit    |  | Heartbeat |
  |  counts   |  | lifecycle |  |  timer    |
  +-----------+  +-----------+  +-----------+
       \              |              /
        \             v             /
         +---> EventBuffer <-------+
              (ring buffer or
               SQLite table)
                     |
                     | flush every N minutes
                     |   or when buffer is full
                     v
              TelemetryUploader
              (background HTTP POST,
               fire-and-forget with retry)
                     |
                     v
              POST /api/telemetry
              (assignment server or
               dedicated ingest endpoint)
```

**EventBuffer** holds structured events locally until flush.  Options:

- **Ring buffer in memory.**  Simplest.  Events are lost on crash, but this is
  acceptable for heartbeats and deltas.  A crash itself is a detectable event
  (the next startup reports `prev_shutdown: crash`).
- **SQLite table.**  More durable.  The client already uses SQLite (`client.db`).
  A `telemetry_events` table with (id, timestamp, json_blob, sent_at) would
  survive crashes and allow retry of unsent events.

**TelemetryUploader** runs on a background timer.  On each tick:

1. Read unsent events from the buffer.
2. Wrap them in a batch envelope (client_id, version, os — written once).
3. POST to the telemetry endpoint.  If the request succeeds, mark events as
   sent.  If it fails, leave them for the next tick.
4. Discard events older than 24-48 hours to bound local storage.

**Batch envelope:**

```json
{
  "schema": 1,
  "client_id": "abc123",
  "client_version": "8.4.1",
  "os": "macosx",
  "os_version": "15.4",
  "events": [
    {"type": "heartbeat", "ts": "...", ...},
    {"type": "wu.completed", "ts": "...", ...}
  ]
}
```

### Opt-out

A boolean config flag (`send-telemetry`, default true) controls whether the
uploader runs.  When false, no events are buffered or sent.  The assignment
request and result upload are unaffected — those are required for work unit
processing and already contain the operational data the servers need.

Telemetry scope should be documented clearly so donors know what is collected.


## Server-side architecture

### Ingest

```
Client  ──POST──>  Ingest API  ──>  Message Queue  ──>  ETL  ──>  Warehouse
                   (stateless)      (Kafka, SQS,       |
                                     or Pub/Sub)        +--> Real-time
                                                              (Grafana, alerts)
```

The ingest API is a thin stateless HTTP endpoint that validates the batch
envelope, assigns a server-side receive timestamp, and publishes to a message
queue.  It returns 202 Accepted immediately.  No processing happens in the
request path.

For a simpler starting point, the ingest API can write batches as newline-
delimited JSON (NDJSON) files to object storage (S3, GCS) and rely on the
warehouse's bulk import to process them periodically.

### Warehouse schema (dimensional model)

The warehouse uses a star schema optimized for analytical queries.

#### Fact tables

**`fact_wu_lifecycle`** — one row per WU state transition.

| Column             | Type      | Description                         |
|--------------------|-----------|-------------------------------------|
| event_id           | BIGINT    | Auto-increment                      |
| client_key         | INT FK    | → dim_client                        |
| time_key           | INT FK    | → dim_time                          |
| gpu_key            | INT FK    | → dim_gpu (nullable)                |
| event_type         | ENUM      | assigned, started, completed, failed, dumped |
| wu_id              | STRING    |                                     |
| project            | INT       |                                     |
| core               | STRING    |                                     |
| wall_time_s        | INT       | NULL except on terminal events      |
| exit_code          | INT       |                                     |
| ppd                | INT       |                                     |
| error_category     | STRING    | Short error classification          |
| cpus               | INT       | CPUs used                           |

**`fact_heartbeat`** — one row per heartbeat received.

| Column                    | Type      | Description                  |
|---------------------------|-----------|------------------------------|
| event_id                  | BIGINT    | Auto-increment               |
| client_key                | INT FK    | → dim_client                 |
| time_key                  | INT FK    | → dim_time                   |
| state                     | ENUM      | running, paused, idle, etc.  |
| on_battery                | BOOL      |                              |
| is_idle                   | BOOL      |                              |
| wus_running               | INT       |                              |
| wus_completed_since_last  | INT       |                              |
| wus_failed_since_last     | INT       |                              |
| errors_since_last         | INT       |                              |
| warnings_since_last       | INT       |                              |

**`fact_client_lifecycle`** — start, stop, crash, update events.

| Column          | Type      | Description                           |
|-----------------|-----------|---------------------------------------|
| event_id        | BIGINT    |                                       |
| client_key      | INT FK    | → dim_client                          |
| time_key        | INT FK    | → dim_time                            |
| event_type      | ENUM      | started, stopped, crashed, updated    |
| uptime_s        | INT       |                                       |
| prev_shutdown   | STRING    | clean, crash, unknown                 |
| from_version    | STRING    | For update events                     |

#### Dimension tables

**`dim_client`** — one row per unique client.  Slowly changing dimension
(SCD Type 2) to track version and hardware changes over time.

| Column          | Type      | Description                           |
|-----------------|-----------|---------------------------------------|
| client_key      | INT PK    | Surrogate key                         |
| client_id       | STRING    | Natural key (pseudonymous)            |
| first_seen      | TIMESTAMP |                                       |
| last_seen       | TIMESTAMP |                                       |
| client_version  | STRING    |                                       |
| os              | STRING    |                                       |
| os_version      | STRING    |                                       |
| cpu_model       | STRING    |                                       |
| cpu_cores       | INT       |                                       |
| mem_total_mb    | INT       |                                       |
| country         | STRING    | Derived server-side from IP, coarsened |
| valid_from      | TIMESTAMP | SCD tracking                          |
| valid_to        | TIMESTAMP |                                       |

**`dim_gpu`** — one row per unique GPU model.

| Column          | Type      | Description                           |
|-----------------|-----------|---------------------------------------|
| gpu_key         | INT PK    | Surrogate key                         |
| vendor_id       | INT       |                                       |
| device_id       | INT       |                                       |
| name            | STRING    | Human-readable name                   |
| vram_mb         | INT       |                                       |
| platform        | STRING    | cuda, hip, opencl, metal              |
| driver_version  | STRING    |                                       |

**`dim_time`** — standard date dimension for time-based analysis.

| Column          | Type      | Description                           |
|-----------------|-----------|---------------------------------------|
| time_key        | INT PK    | YYYYMMDDHH                            |
| date            | DATE      |                                       |
| hour            | INT       |                                       |
| day_of_week     | INT       |                                       |
| week_of_year    | INT       |                                       |
| month           | INT       |                                       |
| quarter         | INT       |                                       |

### Example queries

**WU failure rate by GPU model (last 7 days):**

```sql
select
  g.name                                      as gpu,
  g.driver_version                            as driver,
  count(*)                                    as total_wus,
  sum(case when f.event_type = 'failed' then 1 else 0 end) as failures,
  round(100.0 * sum(case when f.event_type = 'failed'
    then 1 else 0 end) / count(*), 2)        as failure_pct
from fact_wu_lifecycle f
join dim_gpu g on f.gpu_key = g.gpu_key
join dim_time t on f.time_key = t.time_key
where f.event_type in ('completed', 'failed')
  and t.date >= current_date - 7
group by g.name, g.driver_version
order by failure_pct desc
```

**Error rate by client version (rollout canary):**

```sql
select
  c.client_version,
  count(distinct c.client_id)                 as clients,
  sum(h.errors_since_last)                    as total_errors,
  round(sum(h.errors_since_last)::numeric
    / count(*), 2)                            as errors_per_heartbeat
from fact_heartbeat h
join dim_client c on h.client_key = c.client_key
join dim_time t on h.time_key = t.time_key
where t.date >= current_date - 3
group by c.client_version
order by c.client_version desc
```

**Fleet activity by hour of day:**

```sql
select
  t.hour,
  count(distinct h.client_key)                as active_clients,
  sum(h.wus_running)                          as total_wus_running,
  round(avg(case when h.state = 'running'
    then 1.0 else 0.0 end) * 100, 1)         as pct_running
from fact_heartbeat h
join dim_time t on h.time_key = t.time_key
where t.date >= current_date - 7
group by t.hour
order by t.hour
```

**Top crash exit codes (last 30 days):**

```sql
select
  f.exit_code,
  f.core,
  f.error_category,
  count(*)                                    as occurrences,
  count(distinct f.client_key)                as affected_clients
from fact_wu_lifecycle f
join dim_time t on f.time_key = t.time_key
where f.event_type = 'failed'
  and t.date >= current_date - 30
group by f.exit_code, f.core, f.error_category
order by occurrences desc
limit 20
```

### Technology options

| Scale             | Warehouse             | Ingest              | Dashboards      |
|-------------------|-----------------------|---------------------|-----------------|
| < 50K clients     | PostgreSQL + TimescaleDB | Direct insert    | Grafana         |
| 50K - 500K        | ClickHouse (self-hosted) | Kafka → ClickHouse | Grafana      |
| 500K+             | BigQuery or Snowflake | Pub/Sub or Kinesis → GCS/S3 → bulk load | Looker, Grafana |

For a starting point, **ClickHouse** is a strong fit: open-source, columnar,
handles billions of rows on modest hardware, and has excellent SQL support for
the analytical query patterns above.  It can ingest NDJSON directly.


## Implementation roadmap

### Phase 1: Foundation (leverage existing data)

No client changes needed.  Persist the data that assignment and collection
servers already receive.

1. Add a server-side write path that appends assignment request and result
   upload payloads to an analytical store (NDJSON files on disk or direct
   ClickHouse insert).
2. Build `dim_client` and `dim_gpu` from assignment request data.
3. Build `fact_wu_lifecycle` from assignment + result upload data.
4. Stand up Grafana with a handful of dashboards: WU failure rate, throughput
   by project, fleet hardware census, client version distribution.

This phase alone answers most of the motivating questions with zero additional
client network traffic.

### Phase 2: Heartbeat

Add a periodic heartbeat from the client to a telemetry endpoint.

1. Implement `EventBuffer` (ring buffer or SQLite table in `client.db`).
2. Implement `TelemetryUploader` on a 15-minute timer using the existing
   event system.
3. Send heartbeat events with delta counters.
4. Add `fact_heartbeat` table server-side.
5. Add dashboards: fleet activity over time, idle vs. active clients, error
   rate trends.

### Phase 3: Structured lifecycle events

Replace the implicit WU lifecycle tracking (derived from assignment + upload)
with explicit client-side events.

1. Emit `wu.assigned`, `wu.started`, `wu.completed`, `wu.failed`, `wu.dumped`
   events into the EventBuffer at the corresponding points in `Unit.cpp`.
2. Emit `client.started` and `client.stopped` events in `App::run()` and
   shutdown.
3. Detect unclean shutdown (missing `client.stopped` before next
   `client.started`) and emit `client.crashed` retroactively.
4. Add hardware census event on first start and on GPU change detection.

### Phase 4: Alerting and advanced analytics

1. Set up alerts on key metrics: sudden spike in WU failure rate, error rate
   regression after version rollout, drop in active client count.
2. Build a version rollout dashboard that compares error rates between the
   new version and the previous version in real time.
3. Add per-core performance tracking (pre-aggregated client-side) to identify
   performance regressions in new core releases.
4. Explore cohort analysis: retention of new donors, hardware upgrade patterns.


## Privacy and consent

- **Client ID** is already pseudonymous (SHA-256 of a generated RSA key).  It
  is not derived from hardware serial numbers, IP addresses, or user accounts.
- **Location** is not sent by the client.  If country-level location is needed
  for fleet distribution analysis, it should be derived server-side from the
  client IP at ingest time and immediately coarsened (country only, no
  city/region).  The raw IP must not be stored.
- **Opt-out** via a `send-telemetry` config flag.  When false, no heartbeats
  or lifecycle events are sent.  Assignment and result upload (required for
  work unit processing) are unaffected.
- **Data retention** should be bounded.  Raw events can be retained for 90
  days; aggregated data (daily/weekly rollups) can be retained indefinitely.
- **Transparency.**  The telemetry schema and this design document should be
  public so donors can inspect exactly what is collected.
- **No PII.**  Telemetry must never include hostnames, file paths, usernames,
  IP addresses, or any other personally identifiable information.  The
  `mach_name` field currently exposed via Web Control must not be included in
  telemetry payloads.


## Open questions

- **Endpoint placement.**  Should telemetry go to the existing assignment
  servers (simplest, but adds load to a critical path) or to a dedicated
  ingest service (more infrastructure, but isolated from WU scheduling)?
- **Compression.**  Heartbeat payloads are small (~500 bytes) but at fleet
  scale the aggregate bandwidth matters.  Should batches be gzip-compressed?
  (Probably yes, with minimal client cost.)
- **Sampling strategy.**  At what fleet size does sampling become necessary?
  For heartbeats at 15-minute intervals from 1M clients, that is ~67K
  events/minute — well within ClickHouse capacity but worth planning for.
- **Backward compatibility.**  How do we handle schema evolution?  The
  `schema` field in the envelope allows versioned parsing, but the ETL and
  warehouse need to handle multiple schema versions during rollout.
