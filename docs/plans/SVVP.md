# Software Verification & Validation Plan (SVVP)

**Document ID:** SOT-SVVP-001  
**Version:** 1.0  
**Project:** Soteria — Safety-Critical Vulkan SC Rendering Engine  
**ASPICE Process:** SWE.4 / SWE.5 / SWE.6 — Verification and Validation  
**Safety Level:** ISO 26262 ASIL-D  
**Standards:** ISO 26262:2018 Part 6, Automotive SPICE® PAM 4.0

---

## 1. Purpose and Scope

This plan defines the strategy, methods, and environments used to verify and
validate that the Soteria software satisfies all requirements stated in the
Software Requirements Specification (SOT-SRS-001).

Verification confirms that the software is built correctly (meets design).
Validation confirms that the correct software is built (meets stakeholder needs).

---

## 2. Definitions

| Term | Definition |
|------|------------|
| Unit test | Tests a single software unit in isolation; no device required. |
| Integration test | Tests two or more units interacting across a defined interface. |
| Qualification test | Tests the complete product against SRS-level requirements. |
| GTEST_SKIP | GoogleTest macro used to skip device-dependent tests on offline hosts. |
| VulkanSC emulation | The `vksconvk` ICD that translates VulkanSC calls to Vulkan 1.2. |

---

## 3. Test Levels and Scope

### 3.1 Unit Verification (SWE.4)

| Attribute | Value |
|-----------|-------|
| ASPICE WP | SOT-UTS-001 (spec), SOT-UTR-001 (report) |
| Framework | GoogleTest 1.15.2 |
| Source dir | `tests/unit/` |
| Device required | No (all tests offline) |
| Coverage target | 100% statement, 100% branch, 100% MC/DC for argument-validation paths |
| Scope | Argument validation, default-state invariants, Shutdown-on-uninit |

Unit tests verify the argument-validation contract of every software unit
without a Vulkan SC device.  Each class is tested in isolation.

### 3.2 Integration Test (SWE.5)

| Attribute | Value |
|-----------|-------|
| ASPICE WP | SOT-ITS-001 (spec), SOT-ITR-001 (report) |
| Framework | GoogleTest 1.15.2 |
| Source dir | `tests/integration/` |
| Device required | Yes (VulkanSC hardware or emulation layer) |
| Coverage target | All inter-component VkDevice-passing interfaces |
| Skip policy | `GTEST_SKIP()` when `Init()` returns non-kOk |

Integration tests verify that units interact correctly when passing handles
across their interfaces.  They require a live device (or emulation ICD).

### 3.3 Qualification Test (SWE.6)

| Attribute | Value |
|-----------|-------|
| ASPICE WP | SOT-QTS-001 (spec), SOT-QTR-001 (report) |
| Framework | GoogleTest 1.15.2 |
| Source dir | `tests/qualification/` |
| Device required | Partially (offline validation path is always run) |
| Coverage target | Every SRS requirement has at least one passing test |
| Skip policy | `GTEST_SKIP()` for device-dependent test bodies |

Qualification tests verify every SRS requirement from the public API surface.
File names are `SRS_<ID>_test.cpp` for explicit traceability.

---

## 4. Test Environment

### 4.1 Offline Host (CI)

| Component | Value |
|-----------|-------|
| OS | Windows 10/11 or Linux (x86-64) |
| Compiler | Clang-cl ≥ 16 (Windows) / Clang ≥ 16 (Linux) |
| CMake | ≥ 3.22 |
| VulkanSC SDK | 1.0.21 (for headers and stub loader only) |
| VulkanSC device | Not required — device tests skipped automatically |

### 4.2 Target Host (with device)

| Component | Value |
|-----------|-------|
| VulkanSC driver | Emulation layer: `VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN` |
| ICD JSON | `vksconvk.json` (set via `VK_ADD_DRIVER_FILES`) |
| Physical device | Any Vulkan 1.2 GPU (AMD, NVIDIA, Intel) |

---

## 5. Entry and Exit Criteria

### 5.1 Entry Criteria

- `SOT-SRS-001` is at status **DRAFT** or later.
- CMake build succeeds with zero errors and zero warnings.
- All test source files compile without warnings.

### 5.2 Exit Criteria

| Level | Criterion |
|-------|-----------|
| Unit | 0 failing tests; coverage targets met. |
| Integration | 0 failing tests (excluding skips); all inter-component interfaces exercised. |
| Qualification | Every SRS requirement covered by ≥1 passing test; 0 defects open with severity ≥ Major. |

---

## 6. Coverage Requirements

| Unit | Statement | Branch | MC/DC |
|------|-----------|--------|-------|
| `engine::Result` | 100% | 100% | 100% |
| `engine::VkscContext` | ≥ 95% | ≥ 90% | ≥ 90% |
| `engine::rendering::CommandPool` | ≥ 90% | ≥ 85% | ≥ 85% |
| `engine::rendering::FrameSync` | ≥ 90% | ≥ 85% | ≥ 85% |
| `engine::rendering::PipelineCacheSc` | ≥ 95% | ≥ 95% | ≥ 95% |

MC/DC is mandatory for safety-relevant condition branches per ISO 26262 Part 6 §9.

---

## 7. Defect Classification

| Severity | Definition | Required Action |
|----------|------------|-----------------|
| Critical | Data corruption, system crash, undefined behaviour | Block release; fix before any further testing |
| Major | Wrong return code, wrong state, test regression | Fix before qualification sign-off |
| Minor | Cosmetic, doc mismatch, non-safety impact | Fix in next iteration |

---

## 8. Test Execution Method

```bash
# Configure with tests
cmake -B build -DSOTERIA_BUILD_TESTS=ON

# Build
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run with VulkanSC emulation driver
set VK_ADD_DRIVER_FILES=<path>/vksconvk.json   # Windows
export VK_ADD_DRIVER_FILES=<path>/vksconvk.json  # Linux

ctest --test-dir build --output-on-failure

# Run a specific level
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
ctest --test-dir build -L qualification --output-on-failure
```

---

## 9. Traceability to ASPICE Work Products

| This SVVP Section | ASPICE WP Reference |
|-------------------|---------------------|
| §3.1 Unit level   | SWE.4: SOT-UTS-001, SOT-UTR-001 |
| §3.2 Integration  | SWE.5: SOT-ITS-001, SOT-ITR-001 |
| §3.3 Qualification| SWE.6: SOT-QTS-001, SOT-QTR-001 |
| §6 Coverage       | ISO 26262:2018 Part 6 §9.4.3 (MC/DC for ASIL-D) |

---

## 10. References

- SOT-SRS-001 — Software Requirements Specification
- SOT-IDD-001 — Interface Design Document
- ISO 26262:2018 Part 6 — Verification of software
- Automotive SPICE® PAM 4.0 — SWE.4, SWE.5, SWE.6
- GoogleTest 1.15.2 documentation
