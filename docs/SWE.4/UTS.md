# Unit Test Specification (UTS)

**Document ID:** SOT-UTS-001  
**Version:** 1.0  
**Project:** Soteria — Safety-Critical Vulkan SC Rendering Engine  
**ASPICE Process:** SWE.4 — Software Unit Verification  
**Safety Level:** ISO 26262 ASIL-D  
**Derived from:** SOT-SRS-001, SOT-IDD-001

---

## 1. Purpose

This document specifies **what to test** at the unit level: test case IDs, test
objectives, pass/fail criteria, and traceability to SRS requirements.  The
corresponding test results are recorded in SOT-UTR-001.

Unit tests are **offline** — no VulkanSC device is required.  All tests exercise:
- Argument-validation paths (null/zero inputs → `kInvalidArgument`)
- Default-state invariants (handles null before `Init`)
- Shutdown-on-uninit safety (no crash, state unchanged)

---

## 2. Test Framework and Build

| Item | Value |
|------|-------|
| Framework | GoogleTest 1.15.2 (via CMake `FetchContent`) |
| Build target | `soteria-unit-tests` |
| Source directory | `tests/unit/` |
| CMake label | `unit` |
| Run command | `ctest --test-dir build -L unit --output-on-failure` |

---

## 3. Test Cases — `engine::Result`

Source file: `tests/unit/result_test.cpp`

| Test ID | Test Name | Objective | Pass Criterion | SRS ID |
|---------|-----------|-----------|----------------|--------|
| UT-001 | `IsOkReturnsTrueForKOk` | `IsOk` returns true for `kOk` | `EXPECT_TRUE(IsOk(Result::kOk))` passes | — |
| UT-002 | `IsOkReturnsFalseForError` | `IsOk` returns false for `kError` | `EXPECT_FALSE(IsOk(Result::kError))` passes | — |
| UT-003 | `ResultToStringReturnsCorrectString` | `ResultToString` returns correct strings | Known codes return expected literals; unknown code returns "Unknown" | — |

---

## 4. Test Cases — `engine::VkscContext`

Source file: `tests/unit/vksc_context_test.cpp`

| Test ID | Test Name | Objective | Pass Criterion | SRS ID |
|---------|-----------|-----------|----------------|--------|
| UT-004 | `IsNotInitialisedByDefault` | Default-constructed context is uninitialised | `IsInitialised()==false`; all four handles `VK_NULL_HANDLE` | SRS-INIT-001 |
| UT-005 | `ShutdownOnUninitIsNop` | `Shutdown()` on uninit object is safe | No crash; `IsInitialised()==false`; `Device()==VK_NULL_HANDLE` | SRS-INIT-005 |

> Device-dependent tests (Init succeeds, double-init guard, Shutdown resets) are
> defined in `tests/unit/vksc_context_test.cpp` with `GTEST_SKIP()` guards;
> their SRS IDs are SRS-INIT-001, SRS-INIT-002, SRS-INIT-005 respectively.

---

## 5. Test Cases — `engine::rendering::CommandPool`

Source file: `tests/unit/command_pool_test.cpp`

| Test ID | Test Name | Objective | Pass Criterion | SRS ID |
|---------|-----------|-----------|----------------|--------|
| UT-006 | `HandleIsNullBeforeInit` | `Handle()` is `VK_NULL_HANDLE` before `Init` | `EXPECT_EQ(pool.Handle(), VK_NULL_HANDLE)` | SRS-CMD-001 |
| UT-007a | `InitFailsWithNullDevice` | `Init` rejects null device | Returns `kInvalidArgument` | SRS-CMD-001 |
| UT-007b | `ShutdownOnUninitIsNop` | `Shutdown(VK_NULL_HANDLE)` on uninit is safe | No crash; handle still null | SRS-CMD-001 |
| UT-007c | `AllocateBuffersFailsWithNullDevice` | `AllocateBuffers` null device check | Returns `kInvalidArgument` | SRS-CMD-001 |
| UT-007d | `AllocateBuffersFailsWithZeroCount` | `AllocateBuffers` zero count check | Returns `kInvalidArgument` | SRS-CMD-001 |
| UT-007e | `AllocateBuffersFailsWithNullOutputArray` | `AllocateBuffers` null outBuffers check | Returns `kInvalidArgument` | SRS-CMD-001 |

---

## 6. Test Cases — `engine::rendering::FrameSync`

Source file: `tests/unit/frame_sync_test.cpp`

| Test ID | Test Name | Objective | Pass Criterion | SRS ID |
|---------|-----------|-----------|----------------|--------|
| UT-008 | `HandlesAreNullBeforeInit` | All three handles null before `Init` | All `VK_NULL_HANDLE` | SRS-SYNC-001 |
| UT-009 | `InitFailsWithNullDevice` | `Init` rejects null device | Returns `kInvalidArgument` | SRS-SYNC-001 |
| UT-009b | `HandlesRemainNullAfterFailedInit` | Failed `Init` leaves handles unchanged | All still `VK_NULL_HANDLE` | SRS-SYNC-001 |
| UT-009c | `ShutdownOnUninitIsNop` | `Shutdown(VK_NULL_HANDLE)` on uninit is safe | No crash; handles still null | SRS-SYNC-001 |

---

## 7. Test Cases — `engine::rendering::PipelineCacheSc`

Source file: `tests/unit/pipeline_cache_test.cpp`

| Test ID | Test Name | Objective | Pass Criterion | SRS ID |
|---------|-----------|-----------|----------------|--------|
| UT-010 | `HandleIsNullBeforeInit` | `Handle()` is `VK_NULL_HANDLE` before `Init` | `EXPECT_EQ(cache.Handle(), VK_NULL_HANDLE)` | SRS-PIPE-001 |
| UT-011 | `InitFailsWithNullDevice` | `Init` rejects null device | Returns `kInvalidArgument` | SRS-PIPE-002 |
| UT-012 | `InitFailsWithNullData` | `Init` rejects null data pointer | Returns `kInvalidArgument` | SRS-PIPE-002 |
| UT-012b | `InitFailsWithZeroDataSize` | `Init` rejects zero data size | Returns `kInvalidArgument` | SRS-PIPE-002 |
| UT-012c | `ShutdownOnUninitIsNop` | `Shutdown(VK_NULL_HANDLE)` on uninit is safe | No crash; handle still null | SRS-PIPE-001 |

---

## 8. Coverage Targets

| Unit | Statement | Branch | MC/DC | Rationale |
|------|-----------|--------|-------|-----------|
| `engine::Result` | 100% | 100% | 100% | Simple enum, fully exercised by UT-001–003 |
| `engine::VkscContext` (offline paths) | 100% | 100% | 100% | All default-init and Shutdown paths reachable offline |
| `engine::rendering::CommandPool` | 100% | 100% | 100% | All validation branches tested |
| `engine::rendering::FrameSync` | 100% | 100% | 100% | All validation and null-state branches tested |
| `engine::rendering::PipelineCacheSc` | 100% | 100% | 100% | All three argument-validation branches tested |

> Device-dependent branches (Vulkan call failures) are covered at integration and
> qualification level, not at unit level.

---

## 9. Requirements → Test Traceability

| SRS ID | Requirement Summary | Test IDs |
|--------|---------------------|----------|
| SRS-INIT-001 | VkscContext initialises Vulkan SC handles | UT-004 (offline invariant) |
| SRS-INIT-005 | Shutdown is safe on uninit | UT-005 |
| SRS-CMD-001 | CommandPool upfront reservation | UT-006, UT-007a–e |
| SRS-SYNC-001 | FrameSync creates semaphore + fence | UT-008, UT-009, UT-009b–c |
| SRS-PIPE-001 | PipelineCacheSc binary loading | UT-010, UT-012c |
| SRS-PIPE-002 | PipelineCacheSc input validation | UT-011, UT-012, UT-012b |
