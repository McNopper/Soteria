# ASPICE Work Products — Soteria

Soteria follows the **Automotive SPICE® SWE process chain** (SWE.1–SWE.6) to
satisfy ISO 26262 ASIL-D traceability requirements.  This document is the
top-level index of all ASPICE work products produced for this project.

---

## Process Overview

```
SWE.1 Software Requirements Analysis
  └─► SWE.2 Software Architectural Design
        └─► SWE.3 Software Detailed Design & Unit Construction
              ├─► SWE.4 Software Unit Verification
              ├─► SWE.5 Software Integration & Integration Test
              └─► SWE.6 Software Qualification Test
```

Each step refines or tests the artefacts of the previous one.  Traceability
links (requirement ID → design element → test ID) are maintained throughout.

---

## Work Product Index

| Process | Document | ID          | Description                                          | Status   |
|---------|----------|-------------|------------------------------------------------------|----------|
| SWE.1   | [SRS.md](SWE.1/SRS.md) | SOT-SRS-001 | Software Requirements Specification — functional and safety requirements for the Soteria Vulkan SC rendering engine | ✅ Complete |
| SWE.2   | [SAD.md](SWE.2/SAD.md) | SOT-SAD-001 | Software Architectural Design — component decomposition, interfaces, and deployment view | ✅ Complete |
| SWE.2   | [IDD.md](SWE.2/IDD.md) | SOT-IDD-001 | Interface Design Document — public API contracts for each software unit | ✅ Complete |
| SWE.3   | [DDD.md](SWE.3/DDD.md) | SOT-DDD-001 | Detailed Design Document — class-level design, state machines, and unit construction notes | ✅ Complete |
| SWE.4   | [UTS.md](SWE.4/UTS.md) | SOT-UTS-001 | Unit Test Specification — planned test cases, coverage targets, pass/fail criteria | ✅ Complete |
| SWE.4   | [UTR.md](SWE.4/UTR.md) | SOT-UTR-001 | Unit Test Report — offline unit tests for all engine components | ✅ Complete |
| SWE.5   | [ITS.md](SWE.5/ITS.md) | SOT-ITS-001 | Integration Test Specification — integration strategy and test case design | ✅ Complete |
| SWE.5   | [ITR.md](SWE.5/ITR.md) | SOT-ITR-001 | Integration Test Report — cross-component interface verification | ✅ Complete |
| SWE.6   | [QTS.md](SWE.6/QTS.md) | SOT-QTS-001 | Qualification Test Specification — SRS-level test cases mapped to requirements | ✅ Complete |
| SWE.6   | [QTR.md](SWE.6/QTR.md) | SOT-QTR-001 | Qualification Test Report — traceability from every SRS requirement to a passing test | ✅ Complete |
| Plans   | [plans/SVVP.md](plans/SVVP.md) | SOT-SVVP-001 | Software Verification & Validation Plan — test strategy, levels, environments, criteria | ✅ Complete |

---

## Requirements Traceability Summary

| SRS ID        | Requirement                          | Unit Test | Integration Test | Qualification Test |
|---------------|--------------------------------------|-----------|------------------|--------------------|
| SRS-INIT-001  | VkscContext lifecycle                | UT-004–005 (offline) | IT-001, IT-002 | QT-001, QT-002 |
| SRS-INIT-002  | Prevent double initialisation        | —         | IT-001           | QT-002             |
| SRS-INIT-003  | Two-pass physical device selection   | —         | IT-001 (implicit) | QT-001 (implicit) |
| SRS-INIT-004  | Graceful failure on no device        | —         | —                | QT-004             |
| SRS-INIT-005  | Shutdown resets state                | UT-005    | IT-003           | QT-003             |
| SRS-CMD-001   | CommandPool upfront reservation      | UT-006–007 | IT-004          | QT-006             |
| SRS-SYNC-001  | FrameSync semaphore/fence creation   | UT-008–009 | IT-004          | QT-005             |
| SRS-SYNC-002  | Fence initialised signalled          | —         | IT-004           | QT-005             |
| SRS-PIPE-001  | PipelineCache binary loading         | UT-010–012 | IT-005          | QT-007             |
| SRS-PIPE-002  | PipelineCache input validation       | UT-011–012 | —               | QT-008             |

---

## Test Directory Layout

```
tests/
├── unit/            ← SWE.4: offline argument-validation tests (no device needed)
├── integration/     ← SWE.5: cross-component tests (require VulkanSC device)
└── qualification/   ← SWE.6: SRS-level contract tests (require VulkanSC device)
```

Device-dependent tests use `GTEST_SKIP()` when no VulkanSC driver is present
so the CI suite passes without hardware.

---

## Build Instructions

```bash
cmake -B build -DSOTERIA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Set `VK_ADD_DRIVER_FILES` to the VulkanSC emulation ICD JSON before running
integration or qualification tests:

```bash
export VK_ADD_DRIVER_FILES=/path/to/vksc_emu.json
ctest --test-dir build --output-on-failure
```

---

## References

- ISO 26262:2018 Part 6 — Software-level safety requirements
- Automotive SPICE® Process Reference Model v3.1 / PAM 4.0
- Vulkan SC 1.0 Specification
