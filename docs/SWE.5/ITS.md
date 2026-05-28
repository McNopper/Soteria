# Integration Test Specification (ITS)

**Document ID:** SOT-ITS-001  
**Version:** 1.0  
**Project:** Soteria — Safety-Critical Vulkan SC Rendering Engine  
**ASPICE Process:** SWE.5 — Software Integration and Integration Test  
**Safety Level:** ISO 26262 ASIL-D  
**Derived from:** SOT-SRS-001, SOT-IDD-001, SOT-SAD-001

---

## 1. Purpose

This document specifies **integration test cases** that verify correct interaction
between software units across their defined interfaces.  The corresponding test
results are recorded in SOT-ITR-001.

Integration tests require a VulkanSC-capable host (hardware or emulation ICD).
Tests are automatically skipped via `GTEST_SKIP()` when no driver is present.

---

## 2. Test Framework and Build

| Item | Value |
|------|-------|
| Framework | GoogleTest 1.15.2 |
| Build target | `soteria-integration-tests` |
| Source directory | `tests/integration/` |
| CMake label | `integration` |
| Run command | `ctest --test-dir build -L integration --output-on-failure` |
| Device required | Yes — set `VK_ADD_DRIVER_FILES` to emulation ICD JSON |

---

## 3. Integration Strategy

Integration follows a **bottom-up** strategy: `VkscContext` is verified first
(foundational), then rendering components that consume the device handle are
tested in increasing integration depth.

```
Level 1:  VkscContext alone         (lifecycle, double-init, idempotent Shutdown)
Level 2:  VkscContext → CommandPool (device handle passed to Init, AllocateBuffers)
Level 2:  VkscContext → FrameSync   (device handle passed to Init, WaitAndReset)
Level 2:  VkscContext → PipelineCacheSc  (device handle passed to Init)
```

---

## 4. Interface Boundaries Under Test

| Boundary | Components | Interface Contract |
|----------|------------|-------------------|
| Context lifecycle | `VkscContext` | `Init` / `Shutdown` lifecycle, double-init guard, idempotent Shutdown |
| Context → CommandPool | `VkscContext`, `CommandPool` | `VkDevice` handle transfer; `Init`, `AllocateBuffers`, `Shutdown` |
| Context → FrameSync | `VkscContext`, `FrameSync` | `VkDevice` handle transfer; `Init`, `WaitAndReset`, `Shutdown` |
| Context → PipelineCacheSc | `VkscContext`, `PipelineCacheSc` | `VkDevice` handle transfer; `Init`, `Shutdown` |

---

## 5. Test Cases

### 5.1 VkscContext Lifecycle

Source file: `tests/integration/context_lifecycle_test.cpp`

| Test ID | Test Name | Objective | Pass Criterion | SRS ID |
|---------|-----------|-----------|----------------|--------|
| IT-001 | `FullInitShutdownLifecycle` | Full Init → handle access → Shutdown cycle | `Init` returns `kOk`; all four handles non-null; after `Shutdown` all handles null | SRS-INIT-001, SRS-INIT-005 |
| IT-002 | `SecondInitReturnsAlreadyInitialised` | Double-Init guard | Second `Init` returns `kAlreadyInitialised`; state unchanged | SRS-INIT-002 |
| IT-003 | `ShutdownIsIdempotent` | Safe double-Shutdown | Second `Shutdown` does not crash; all handles remain null | SRS-INIT-005 |

### 5.2 Rendering Pipeline Integration

Source file: `tests/integration/rendering_pipeline_test.cpp`

| Test ID | Test Name | Objective | Pass Criterion | SRS ID |
|---------|-----------|-----------|----------------|--------|
| IT-004 | `CommandPoolAndFrameSyncInitOnRealDevice` | CommandPool and FrameSync init against a real VkDevice | Both `Init` calls return `kOk`; `Handle()` and all three sync handles non-null | SRS-CMD-001, SRS-SYNC-001 |
| IT-005 | `PipelineCacheScInitOnRealDevice` | PipelineCacheSc init against a real VkDevice | `Init` returns `kOk`; `Handle()` non-null | SRS-PIPE-001 |
| IT-006 | `OrderedShutdownCommandPoolFrameSync` | Shutdown in reverse init order | No crash; all handles null after Shutdown | SRS-INIT-005 |

---

## 6. Skip Policy

All device-dependent integration tests follow this pattern:

```cpp
Result r = ctx.Init(cfg);
if (!IsOk(r)) {
    GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
}
```

This ensures the test suite remains green in offline CI environments and the
skip is logged with a diagnostic message.

---

## 7. Requirements → Integration Test Traceability

| SRS ID | Requirement Summary | Integration Tests |
|--------|---------------------|-------------------|
| SRS-INIT-001 | VkscContext initialises Vulkan SC stack | IT-001 |
| SRS-INIT-002 | Double-Init returns kAlreadyInitialised | IT-002 |
| SRS-INIT-005 | Shutdown in reverse order, idempotent | IT-001, IT-003, IT-006 |
| SRS-CMD-001 | CommandPool upfront reservation | IT-004 |
| SRS-SYNC-001 | FrameSync semaphore + fence creation | IT-004 |
| SRS-PIPE-001 | PipelineCacheSc binary loading | IT-005 |
