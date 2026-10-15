# Soteria — Architecture

> *Soteria: Vulkan SC safety-critical rendering engine. This document is the
> single source of truth for the layering, ownership and lifecycle rules that
> the code — and the `test_layers` gate — enforce.*

## Layered structure

```
┌─────────────────────────────────────────────────────────────┐
│ app/        simulation & demo (NOT certifiable)             │
│             main.cpp orchestrator, horizon renderer, demo    │
│             data sources, console telemetry/health monitor   │
├─────────────────────────────────────────────────────────────┤
│ engine/     safety-critical core (MISRA C++:2023)            │
│                                                             │
│   wsi/        IRenderOutput seam, DisplayOutput              │
│               (VK_KHR_display)            ▲                  │
│   rendering/  SwapchainSc, PipelineCacheSc (+ cache header   │
│               validation), VertexBuffer, CommandPool,        │
│               FrameSync, IFrameRenderer seam                 │
│   data/       IAttitudeSource, IFrameReport, IHealthMonitor, │
│               IHeartbeat (pure interfaces)                   │
│   core/       Result, log, FixedString, VkscContext          │
│               (+ DrainFaults, VkscDeviceIdentity),           │
│               FrameBudgetMonitor, IFaultSink, safety macros, │
│               IMemoryAllocator                               │
└─────────────────────────────────────────────────────────────┘
        Vulkan SC loader + ICD (offline-compiled pipelines)
```

## Layer rules (mechanically enforced)

`python tools/check_layers.py` (registered as `ctest -R test_layers`, label
`analysis`) proves these one-way include rules by scanning the include graph:

| Block              | May include        | Must NOT include                    |
|--------------------|--------------------|-------------------------------------|
| `engine/core`      | core only          | rendering, wsi, data, app, tests    |
| `engine/data`      | core interfaces    | rendering, wsi, app, tests          |
| `engine/rendering` | core, data         | wsi, app, tests                     |
| `engine/wsi`       | core               | rendering, data, app, tests         |
| `app`              | engine (any)       | tests                               |
| `tests`            | engine (any), `app/rendering/pipeline_cache_data.hpp` (generated blob under test) | rest of `app/` |

The engine must never depend on application code; a violation breaks the
certifiable boundary and fails the gate.

The gate enforces the `engine/*` rows today; `app`/`tests` are not yet
checked as includers, so the `tests` row above is documented here and proven
once the machine-provable contract gate lands (ROADMAP P1-13). `tests` include
the generated pipeline-cache header in unit, integration and qualification
suites — that single file is the only permitted `tests → app` edge.

### Folder scope bands

| Folder | Band | Certifiable layer? |
|--------|------|--------------------|
| `engine/{core,data,rendering,wsi}` | runtime — certifiable core | yes (MISRA C++:2023) |
| `app/` | runtime — demo: frame orchestration, horizon renderer, demo data sources, telemetry | no |
| `tests/`, `tools/`, `cmake/` | build & verification side band | no (verification tooling) |
| `docs/`, `references/`, root configs | contract and reference material | n/a (never shipped) |

- `engine/core/vksc_context` is the sanctioned API-boundary object: it names
  `VkInstance`/`VkDevice` yet lives in `core/` by design (created first,
  destroyed last).
- `engine/data` holds the seam interfaces the *application* implements —
  policy stays app-side, the engine only consumes.
- Frame orchestration (acquire/submit/present) currently lives in
  `app/main.cpp`; whether it moves into `engine/` is an open decision tied to
  deterministic pacing (ROADMAP P1-5).

## Ownership and seams

- **VkscContext** owns `VkInstance`, `VkPhysicalDevice` (borrowed), `VkDevice`,
  and the graphics queue. First created, last destroyed.
- **IRenderOutput** (`wsi/`) is the display seam: `DisplayOutput` today
  (direct display), AR overlay/HUD modes declared in `RenderOutputMode` for
  future backends. Returns the `VkSurfaceKHR` and the mandated color format
  per mode.
- **SwapchainSc** owns `VkSwapchainKHR`, swapchain images and image views.
  In Vulkan SC there is no `vkDestroySwapchainKHR` — the object is reclaimed
  by `vkDestroyDevice`; `Shutdown()` releases the image views and drops the
  handle.
- **IFrameRenderer** (`rendering/`) is the frame seam: given an image-view
  index and an `AttitudeData` snapshot it returns a recorded command buffer.
  It never sees the swapchain, semaphores, fences, or queues — the caller
  orchestrates acquire/submit/present. `HorizonRenderer` (app/) is the demo
  implementation.
- **IAttitudeSource / IFrameReport** (`data/`) are the data-in / telemetry-out
  seams. Implementations must be wait-free and allocation-free.

## Lifecycle contract

Every engine object follows the same pattern:

1. `Init(...)` — `[[nodiscard]] Result`, validates arguments, creates handles,
   cleans up everything on any failure path.
2. `Shutdown(...)` — idempotent, safe on partial init, releases handles in
   reverse creation order.

Application-level init order (see `app/main.cpp`):
`VkscContext → DisplayOutput → SwapchainSc → IFrameRenderer → FrameSync`,
shutdown strictly in reverse after `vkDeviceWaitIdle`.

## Frame schedule

One iteration of the deterministic frame loop (all waits bounded, no
unbounded blocking):

```
DrainFaults          poll vkGetFaultData → IFaultSink (driver fault records)
WaitAndReset         fence wait, 5 s timeout (GPU hang → eGpuHang)
GetAttitude          sample IAttitudeSource (invalid → eInputDataInvalid)
AcquireNextImage     bounded by the swapchain; failure → eAcquireFailed/
                     eDeviceLost/eSurfaceLost
RecordFrame          IFrameRenderer records into a pre-allocated command
                     buffer; failure → eRecordFailed
QueueSubmit          failure → eSubmitFailed
QueuePresent         failure → ePresentFailed; success → IHeartbeat.Kick()
Telemetry            FrameBudgetMonitor.RecordFrame(renderMs);
                     escalation → eFrameDeadlineMiss; IFrameReport sink
```

Every failure path reports exactly one classified health event before the
safe shutdown; the loop never continues past an unrecoverable Vulkan error.

## Static-allocation contract (Vulkan SC)

All object counts are declared upfront in
`VkDeviceObjectReservationCreateInfo` via `VkscContextConfig::resources`;
pipelines are pre-compiled offline and loaded through `PipelineCacheSc` from
an embedded `constexpr` byte array (`app/rendering/pipeline_cache_data.hpp`,
generated by `tools/generate_pc_header.py` — generated content: edit the
shader/PCC sources and regenerate, never hand-edit). No runtime shader
compilation, no filesystem I/O, no dynamic allocation after init.

## Error handling

No exceptions (`/EHs-c-`, `-fno-exceptions`), no RTTI. Every fallible
function returns `engine::Result` (negative error codes, `kOk == 0`) and is
`[[nodiscard]]`. `ResultToString` covers every enumerator plus a `default`
branch (MISRA C++:2023 Rule 9.4.2). Logging is compile-time gated
(`VKSC_ENABLE_LOGGING`, Debug only) and fully stripped from production
binaries.

## Health & fault supervision

Detection and reaction are separated: the engine detects and classifies,
the application owns the reaction policy through injected seams.

- **Driver faults** (core Vulkan SC 1.0): `VkscContext::DrainFaults` polls
  `vkGetFaultData` once per frame and forwards records to an
  `engine::core::IFaultSink`. Polling replaces the `VkFaultCallbackInfo`
  callback by design — the callback carries no user context (would force
  global state, MISRA Rules 6.7.1/6.7.2), must not call Vulkan commands, and
  conflicts with SCID-3's no-async-callback determinism goal.
- **Health events**: `engine::data::IHealthMonitor` receives classified
  events (GPU hang, device/surface lost, acquire/submit/present/record
  failures, invalid input data, frame-deadline miss, critical driver fault).
  `CRITICAL` driver faults escalate to `eDriverFault`.
- **Frame deadline monitoring**: `engine::core::FrameBudgetMonitor` tracks
  render time against a configurable budget and escalates sustained
  consecutive misses (edge-triggered `eFrameDeadlineMiss`).
- **Heartbeat**: `engine::data::IHeartbeat::Kick` once per presented frame —
  the liveness evidence an external watchdog or ARINC 653-style health
  monitor consumes.
- **Identity capture** (SCID-7): `VkscDeviceIdentity` records
  vendor/device/driver/api versions at Init for certification configuration
  records.
- **Asset integrity**: `ValidatePipelineCacheHeader` verifies the 56-byte
  Vulkan SC cache header (size, version, validation version, index bounds)
  before the blob reaches the driver; vendor/device ID mismatches are logged
  (compatibility remains the driver's authoritative check —
  `VK_ERROR_INVALID_PIPELINE_CACHE_DATA`).

## Deviations

MISRA deviations are annotated at the point of use with `MISRA_DEVIATION()`
(`engine/core/safety_macros.hpp`) plus a matching `// NOLINT`. The register
with full rationale, risk mitigation and verification lives in
[DEVIATIONS.md](DEVIATIONS.md) — currently DEV-001 (Rule 8.2.6, `void*` cast
for mapped memory) and DEV-002 (Rule 30.0.1, debug-only C stdio logging).

## Validation lanes

| Lane | Command | Gate |
|------|---------|------|
| Build | `cmake --preset debug && cmake --build --preset debug` | `/W4 /WX` (+ `-Wshadow -Wconversion -Wsign-conversion -Wnull-dereference -Wswitch-enum`) |
| Unit/integration/qualification | `ctest --preset debug` | GTest; device tests skip without a Vulkan SC device |
| clang-tidy (MISRA profile) | `ctest -R test_tidy` or `python tools/check_tidy.py` | `WarningsAsErrors: '*'` on `engine/` |
| Layer rules | `ctest -R test_layers` or `python tools/check_layers.py` | include-graph proof, no toolchain needed |
| cppcheck | `cmake --build build --target cppcheck` | second analyzer, `--error-exitcode=1` |

Analysis lanes exit `77` (ctest SKIP) when their toolchain is absent.

Planned lanes — structural coverage (P0-13), CI execution (P0-14) and the
machine-provable architecture contract (P1-13) — are tracked in
[ROADMAP.md](ROADMAP.md); adopting them must not weaken the lanes above.
