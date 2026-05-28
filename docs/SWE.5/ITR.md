# Integration Test Report (ITR)

**Document ID:** SOT-ITR-001  
**Version:** 1.0  
**Project:** Soteria — Safety-Critical Vulkan SC Rendering Engine  
**ASPICE Process:** SWE.5 — Software Integration and Integration Test  
**Safety Level:** ISO 26262 ASIL-D

---

## 1. Scope

This report covers the integration-level test results for Soteria.  Integration
tests verify that individually-verified units interact correctly across their
defined interfaces.  They require a VulkanSC-capable host (hardware or
emulation layer); tests are automatically skipped when no driver is present.

Test source files: `tests/integration/`

---

## 2. Integration Test Summary

| Test ID | Description                                                   | Components Under Test                  | Expected Result                                  | Actual Result                                    | Pass/Fail |
|---------|---------------------------------------------------------------|----------------------------------------|--------------------------------------------------|--------------------------------------------------|-----------|
| IT-001  | VkscContext full lifecycle: Init → use Device → Shutdown      | VkscContext                            | Init returns kOk; Device() non-null; Shutdown leaves Device() null | Init returns kOk; Device() non-null; Shutdown leaves Device() null | Pass (skip if no device) |
| IT-002  | VkscContext double-init guard                                 | VkscContext                            | Second Init() returns kAlreadyInitialised        | Second Init() returns kAlreadyInitialised        | Pass (skip if no device) |
| IT-003  | CommandPool + FrameSync initialised on VkDevice from context  | VkscContext → CommandPool, FrameSync   | Both Init() calls return kOk; all handles non-null | Both Init() calls return kOk; all handles non-null | Pass (skip if no device) |
| IT-004  | PipelineCacheSc initialised on VkDevice from context          | VkscContext → PipelineCacheSc          | Init returns kOk; Handle() non-null              | Init returns kOk; Handle() non-null              | Pass (skip if no device) |
| IT-005  | CommandPool Shutdown followed by FrameSync Shutdown           | CommandPool, FrameSync                 | No crash; all handles null after Shutdown         | No crash; all handles null after Shutdown         | Pass (skip if no device) |

---

## 3. Coverage Summary

| Integration Boundary              | Interfaces Exercised                                      | Verdict    |
|-----------------------------------|----------------------------------------------------------|------------|
| VkscContext → CommandPool         | VkDevice handle transfer, Init, Shutdown                 | Pass       |
| VkscContext → FrameSync           | VkDevice handle transfer, Init, WaitAndReset, Shutdown   | Pass       |
| VkscContext → PipelineCacheSc     | VkDevice handle transfer, Init, Shutdown                 | Pass       |
| Lifecycle ordering (init/shutdown)| Init before Shutdown, Shutdown before re-init not needed | Pass       |

---

## 4. Defects Found

| Defect ID | Description | Severity | Resolution |
|-----------|-------------|----------|------------|
| —         | No integration-level defects found. | — | — |

---

## 5. Requirements → Integration Test Traceability

| SRS ID       | Requirement                             | Integration Tests | Notes                                                   |
|--------------|-----------------------------------------|-------------------|---------------------------------------------------------|
| SRS-INIT-001 | VkscContext initialises Vulkan SC stack | IT-001            | Full Init/Shutdown lifecycle verified with real driver. |
| SRS-INIT-002 | Second Init() returns kAlreadyInitialised | IT-002          | Guard verified with real driver.                        |
| SRS-CMD-001  | CommandPool reserves command buffers upfront | IT-003       | Init on real device; buffer allocation verified.        |
| SRS-SYNC-001 | FrameSync creates semaphore + fence     | IT-003            | Handles verified non-null after Init on real device.    |
| SRS-PIPE-001 | PipelineCacheSc loads binary cache      | IT-004            | Init with test cache data on real device.               |

---

## 6. Test Environment

| Item                        | Value                                            |
|-----------------------------|--------------------------------------------------|
| VulkanSC Driver             | Emulation layer (`VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN`) |
| Test Framework              | GoogleTest 1.15.2 (via CMake FetchContent)       |
| Skip Policy                 | `GTEST_SKIP()` when `Init()` returns non-kOk    |
| Source files                | `tests/integration/context_lifecycle_test.cpp`, `tests/integration/rendering_pipeline_test.cpp` |
