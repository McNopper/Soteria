# Qualification Test Specification (QTS)

**Document ID:** SOT-QTS-001  
**Version:** 1.0  
**Project:** Soteria — Safety-Critical Vulkan SC Rendering Engine  
**ASPICE Process:** SWE.6 — Software Qualification Test  
**Safety Level:** ISO 26262 ASIL-D  
**Derived from:** SOT-SRS-001, SOT-IDD-001

---

## 1. Purpose

This document specifies **qualification test cases** that verify every SRS
requirement is covered by at least one executable test.  Tests are named
`SRS_<ID>_test.cpp` for explicit, unambiguous traceability.

The corresponding test results are recorded in SOT-QTR-001.

---

## 2. Test Framework and Build

| Item | Value |
|------|-------|
| Framework | GoogleTest 1.15.2 |
| Build target | `soteria-qualification-tests` |
| Source directory | `tests/qualification/` |
| CMake label | `qualification` |
| Run command | `ctest --test-dir build -L qualification --output-on-failure` |
| Device required | Partially — offline validation always runs; device path uses `GTEST_SKIP()` |

---

## 3. Qualification Test Cases

### SRS-INIT-001 — Vulkan SC Initialisation

Source file: `tests/qualification/SRS_INIT_001_test.cpp`

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-001 | `VkscContextCreatesInstanceAndDevice` | `Init` returns `kOk`; `Instance()`, `PhysicalDevice()`, `Device()`, `GraphicsQueue()` all non-null | Yes (skip if absent) |

### SRS-INIT-002 — Upfront Resource Reservation / Double-Init Guard

Source file: `tests/qualification/SRS_INIT_002_test.cpp`

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-002 | `DoubleInitReturnsAlreadyInitialised` | Second `Init` returns `kAlreadyInitialised`; state unchanged | Yes (skip if absent) |

### SRS-INIT-003 — Two-Pass Physical Device Selection

QT-001 covers this requirement implicitly: a successful `Init` return value confirms
that the two-pass selection algorithm ran to completion and chose a suitable device.
No independent qualification test is required.

### SRS-INIT-004 — Graceful Failure on No Device

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-004 | `InitReturnsErrorWhenNoDevicePresent` | On a host with no VulkanSC driver, `Init` returns `kVkscInstanceFailed` or `kVkscEnumerateFailed` without crash | No (condition-dependent) |

> QT-004 is satisfied by the offline skips: when a test with `GTEST_SKIP()` fires,
> the skip message logs the actual error code returned by `Init`, demonstrating
> graceful failure rather than crash or UB.

### SRS-INIT-005 — Shutdown Resets State

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-003 | `ShutdownResetsAllHandles` | After `Shutdown`: `IsInitialised()==false`; all handles `VK_NULL_HANDLE` | Yes (skip if absent) |

> QT-003 is implemented inside `tests/qualification/SRS_INIT_001_test.cpp`
> as a follow-on assertion after QT-001.

### SRS-SYNC-001 / SRS-SYNC-002 — Frame Synchronisation

Source file: `tests/qualification/SRS_SYNC_001_test.cpp`

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-005 | `FrameSyncCreatesAllThreeSyncObjects` | `Init` returns `kOk`; `ImageAvailable()`, `RenderComplete()`, `InFlight()` all non-null | Yes (skip if absent) |

### SRS-CMD-001 — Command Pool Upfront Reservation

Source file: `tests/qualification/SRS_CMD_001_test.cpp`

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-006 | `CommandPoolReservesMemoryAndAllocates` | `Init` returns `kOk`; `Handle()` non-null; `AllocateBuffers(1)` returns `kOk` | Yes (skip if absent) |

### SRS-PIPE-001 — Pipeline Cache Binary Loading

Source file: `tests/qualification/SRS_PIPE_001_test.cpp`

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-007 | `PipelineCacheScLoadsCompileTimeBinary` | `Init` with valid data returns `kOk`; `Handle()` non-null | Yes (skip if absent) |

### SRS-PIPE-002 — Pipeline Cache Input Validation

Source file: `tests/qualification/SRS_PIPE_001_test.cpp`

| Test ID | Test Name | Pass Criterion | Device? |
|---------|-----------|----------------|---------|
| QT-008a | `InitRejectsNullDevice` | Returns `kInvalidArgument` | No |
| QT-008b | `InitRejectsNullData` | Returns `kInvalidArgument` | No |
| QT-008c | `InitRejectsZeroDataSize` | Returns `kInvalidArgument` | No |

---

## 4. Skip Policy

All device-dependent qualification tests follow:

```cpp
Result r = ctx.Init(cfg);
if (!IsOk(r)) {
    GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
}
```

Offline validation paths (argument checks, default invariants) always execute
regardless of device availability.

---

## 5. Requirements Coverage Matrix

| SRS ID | Requirement Summary | Qualification Tests | Offline? |
|--------|---------------------|---------------------|----------|
| SRS-INIT-001 | VkscContext initialises Vulkan SC stack | QT-001 | No |
| SRS-INIT-002 | Double-Init returns kAlreadyInitialised | QT-002 | No |
| SRS-INIT-003 | Two-pass physical device selection | QT-001 (implicit) | No |
| SRS-INIT-004 | Graceful failure when no device | QT-004 | Yes (via skip) |
| SRS-INIT-005 | Shutdown resets all handles | QT-003 | No |
| SRS-SYNC-001 | FrameSync semaphore + fence | QT-005 | No |
| SRS-SYNC-002 | Fence initialised in signalled state | QT-005 | No |
| SRS-CMD-001 | CommandPool upfront reservation | QT-006 | No |
| SRS-PIPE-001 | PipelineCacheSc binary loading | QT-007 | No |
| SRS-PIPE-002 | PipelineCacheSc input validation | QT-008a, QT-008b, QT-008c | Yes |

---

## 6. Requirements Not Covered at Qualification Level

The following SRS requirements describe constraints that are verified by code
review or static analysis rather than runtime test cases:

| SRS ID | Verification Method |
|--------|---------------------|
| SRS-INIT-003 | Two-pass device selection algorithm verified by code review; runtime pass confirmed by QT-001 |
| SRS-WSI-001 | Compile-time backend selection: code review |
| SRS-WSI-002 | VK_KHR_display usage: code review of `display_output.cpp` |
| SRS-WSI-003 | IRenderOutput abstraction: code review |
| SRS-SWP-001 | `k_max_images = 3` constant: static assertion + code review |
| SRS-SWP-002 | Format validation: code review of `swapchain.cpp` |
| SRS-SWP-003 | View/swapchain destroy order: code review |
| SRS-CMD-002 | Primary buffers only: code review |
| SRS-REND-001 | IFrameRenderer contract: code review |
| SRS-REND-002 | No submit/present in RecordFrame: static analysis |
| SRS-ATT-001 | IAttitudeSource interface only: code review |
| SRS-ATT-002 | `valid` flag checked: code review of `horizon_renderer.cpp` |
| SRS-ATT-003 | Sign conventions: visual inspection of demo output |
| SRS-HOR-001…005 | Symbology elements: visual inspection + code review |
| SRS-TEL-001 | Non-blocking OnFrameComplete: timing measurement |
| SRS-TEL-002 | Release no-op logging: binary symbol inspection |
| SRS-MEM-001 | No runtime heap allocation: Valgrind/massif |
| SRS-MEM-002 | IMemoryAllocator injection: clang-tidy `cppcoreguidelines-no-malloc` |
| SRS-LOG-001 | Conditional compilation: Release binary inspection |
| SRS-VBF-001 | Persistent buffer mapping: code review |
| SRS-VBF-002 | Fixed buffer size: code review |
| SRS-SYNC-003 | Fence timeout returns kVkscTimeoutFailed: fault injection test (future) |
