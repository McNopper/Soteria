# Soteria — Roadmap

> Feature plan for the Vulkan SC safety-critical rendering engine.
> Grounded in online research ([docs/research/vulkan-sc-safety-critical-features.md](research/vulkan-sc-safety-critical-features.md)):
> the Vulkan SC 1.0 spec, the Khronos CTS, ISO 26262 / DO-178C / ARINC 653
> practice, and the commercial stacks (CoreAVI VkCore SC + TrueCore, DiSTI,
> Ansys SCADE, Basemark, Altia). Deviation register: [DEVIATIONS.md](DEVIATIONS.md).
> Last full review: 2026-10-15 (incl. cross-project tooling review).
>
> Legend: ✅ done · ◐ partially done · ☐ planned

## P0 — Safety-case backbone (MVP)

| # | Feature | State | Notes |
|---|---------|-------|-------|
| P0-1 | Static resource contract (`VkDeviceObjectReservationCreateInfo` + `VkPhysicalDeviceVulkanSC10Features`) | ◐ | Full reservation wired through `VkscContextConfig`; pool bucket sizes still hand-picked, derive from `pcinfo` |
| P0-2 | Pipeline-cache integrity & compatibility validation at init | ✅ | `ValidatePipelineCacheHeader` (56-byte SC header: size/version/validation-version/index bounds) runs before `vkCreatePipelineCache`; vendor/device mismatch logged, driver stays authoritative; unit-tested offline incl. the shipped blob |
| P0-3 | Fault handling: `vkGetFaultData` per-frame drain → `IFaultSink`; level/type classification | ✅ | Polling design (rationale in `engine/core/i_fault_sink.hpp`); CRITICAL faults escalate to health events |
| P0-4 | Device-lost / GPU-hang policy: bounded fence waits, `VK_ERROR_DEVICE_LOST` classification | ✅ | 5 s fence timeout + `eGpuHang`/`eDeviceLost` health events; recovery = safe shutdown (restart is system scope) |
| P0-5 | Frame deadline monitoring + heartbeat | ✅ | `FrameBudgetMonitor` (escalating consecutive-miss counter) + `IHeartbeat` seam kicked per presented frame |
| P0-6 | Safe-state indication: pre-provisioned minimal "safe frame" pipeline | ☐ | Horizon demo shows red on invalid data; a dedicated always-available safe pipeline is open |
| P0-7 | Zero dynamic allocation after init | ✅ | By construction + static analysis; `pAllocator == NULL` everywhere |
| P0-8 | Input data integrity: `AttitudeData.valid` propagation | ◐ | Failure indication rendered; plausibility/range checks and freshness counters are app-side (contract documented) |
| P0-9 | Version/variant & identity capture (SCID-7) | ✅ | `VkscDeviceIdentity` (vendor/device/driver/api + driverID + name) captured at Init |
| P0-10 | Command-pool memory budgeting (`vkGetCommandPoolMemoryConsumption`) | ☐ | Reservation is generous constant; measure + assert in dev builds |
| P0-11 | Dev/test hygiene: no exceptions/RTTI ✅, MISRA checker wired ✅ | ✅ | clang-tidy lane (`test_tidy`), layer gate (`test_layers`), cppcheck targets; lanes run on demand only — CI execution tracked in P0-14 |
| P0-12 | Health/telemetry report: `IFrameReport` ✅ + `IHealthMonitor` + fault counts | ✅ | Classified events: GPU hang, device/surface lost, acquire/submit/present/record failures, invalid input, deadline miss, driver fault |
| P0-13 | Structural coverage lane (statement/branch; MC/DC target) | ☐ | Top certification gap: no coverage instrumentation anywhere today; llvm-cov over the clang-cl build, wired into ctest (`analysis` label); ISO 26262-6 evidence |
| P0-14 | Analysis lanes executed in CI | ☐ | `.github/` has no workflows — `test_layers`, `test_tidy` and cppcheck run on demand only; gate the PR path with the existing lanes first |

## P1 — Expected of a credible ASIL-D display engine

| # | Feature | State | Notes |
|---|---------|-------|-------|
| P1-1 | Telltale/safety-region tagging + per-frame signature/CRC hook | ☐ | Engine computes or interfaces to display-controller signature unit (Altia ASM / Cypress pattern) |
| P1-2 | Freeze-frame detection: frame counter in output + monitor-visible freshness | ◐ | `frameNumber` telemetry exists; composite/sign per frame is open |
| P1-3 | Degraded mode: reduced pre-compiled pipeline set, formal entry/exit | ☐ | Builds on P0-6 safe frame |
| P1-4 | SEU mitigations: prefer `VK_MEMORY_HEAP_SEU_SAFE_BIT`; `VK_KHR_object_refresh` policy | ☐ | Optional device extension; query + schedule `vkCmdRefreshObjectsKHR` |
| P1-5 | Deterministic frame pacing aligned to partition schedule | ☐ | Fixed cadence, defined overrun behavior (skip/dup frame); decide whether acquire/submit/present orchestration stays app-side or moves into `engine/` |
| P1-6 | Capability-matrix testing via devsim (property variations) | ☐ | `commandPoolResetCommandBuffer`, `commandBufferSimultaneousUse`, recycle flags |
| P1-7 | Golden-image/invariance regression tests (spec Invariance Rules 1–7) | ☐ | Per-pipeline repeatability; no shader side effects in safety pipelines. Prerequisite: demo attitude is system-uptime driven (`GetTickCount64`) — two runs never show identical frame content; needs a deterministic frame-index/fixed-step time source or a fixed-attitude capture mode. Observed on the emulation ICD: static regions render bit-exact across runs (all deltas confined to attitude-driven geometry). Planned harness: timed window capture + PSNR/SSIM/changed-pixel threshold against baselines |
| P1-8 | Two-pass self-check init (CTS-inspired reservation discovery in dev builds) | ☐ | Verify declared reservations ≥ actual usage |
| P1-9 | Tool-qualification plan (ISO 26262-8 TCL) for pipeline/PCC/build tooling | ☐ | Cache CRC validation (P0-2) already improves TD; scope the plan to the Python gate scripts (`tools/*.py`) too — they are verification tooling |
| P1-10 | WCET / frame-time characterization, documented frame WCET claim | ☐ | Timestamps in dev; optionally `VK_KHR_performance_query` |
| P1-11 | Deterministic ordered dither for horizon gradients (if target panel needs it) | ☐ | Fixed pattern only — stochastic dither breaks invariance |
| P1-12 | Boot-to-first-valid-frame metric | ☐ | Measure and minimize (instant-on expectation) |
| P1-13 | Machine-provable architecture contract (contract block in ARCHITECTURE.md, generated drift-gated diagrams, file-level include-cycle gate, default-deny edges) | ☐ | Layer matrix proven today; closes the documented `tests → app` gap and adds cycle/default-deny rules the current gate lacks. Needs a deterministic include-resolution fix first (ambiguous `rendering/…` includes resolve hash-order-dependently), then `check_layers.py` retires — one opinion per fact |
| P1-14 | Third-party provenance: `THIRD-PARTY.md`, Vulkan SC SDK license record, dependency-vulnerability scan, SBOM | ☐ | GoogleTest v1.15.2 = BSD-3-Clause (fetched, not vendored); Vulkan SC SDK license currently unrecorded; no copyleft in tree; release archives must keep excluding `references/` |
| P1-15 | Frame-loop modularisation & code-size discipline | ☐ | `main()` ≈ 385 lines; duplicated device-query blocks (`vksc_context.cpp` ×3) and `Result`→health-event mappings (`main.cpp` ×4); behavior-preserving extraction only, all lanes green before/after |
| P1-16 | Present robustness under surface geometry change (emulation window) | ☐ | Resize/move of the demo window → `vkQueuePresentKHR` failure → classified `ePresentFailed` → controlled shutdown (policy correct, reproduced 3×); fixed panels unaffected — needs swapchain-recreation handling or an explicit demo-only guard |

## P2 — Differentiators / later phases

| # | Feature | Notes |
|---|---------|-------|
| P2-1 | Dual-channel comparison support (deterministic, inspectable output) | Mostly system-level; engine contributes invariance |
| P2-2 | ARINC 661-style widget/UA separation API | Avionics CDS architectures |
| P2-3 | Multi-display / display-group failover | Emulation ICD JSON display configs help testing |
| P2-4 | OpenWFD/NvSci presentation backend behind `IRenderOutput` | NVIDIA DRIVE/Jetson targets |
| P2-5 | Fault-injection test suite on the emulation ICD (COMMAND_BUFFER_FULL, DEVICE_LOST, INVALID_API_USAGE) | Emulation ICD exists for exactly this |
| P2-6 | SPIR-V+JSON debug-info caches in dev builds | Full validation-layer SPIR-V checks; strip for production |
| P2-7 | Dev ring-buffer trace for certification debug sessions | Compiled out in release (policy already set) |
| P2-8 | Compute pipeline support | SC supports compute; not needed for 2D/3D MVP |

## Explicitly not engine scope

- Display-path CRC hardware, panel overlay layers, ECC flash, safe boot → **platform/HW**
- Partition scheduling, MMU enforcement, hardware watchdog kicking → **RTOS/platform**
- Vulkan SC driver/PCC correctness, TrueCore-class GPU monitoring → **driver vendor** (consumed as environmental assumptions from their safety manuals)
- Which displayed items are safety-related; system hazard analysis → **app/OEM** (engine provides tagging/monitoring mechanisms)
