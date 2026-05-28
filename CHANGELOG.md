# Changelog

All notable changes to Soteria are documented here.  This file serves as
configuration management evidence per ISO 26262:2018 Part 8 §7 and
Automotive SPICE® PAM 4.0 SUP.8 (Configuration Management).

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version scheme: `MAJOR.MINOR.PATCH` (SemVer).

---

## [Unreleased]

### Changed
- Renamed `aspice/` directory to `docs/` — aligns with GitHub documentation conventions;
  ASPICE `SWE.x/` subdirectory structure preserved.
- Renamed `simulation/` directory to `app/` — more accurately describes the demo
  application; `simulation` implied test infrastructure rather than a runnable executable.
- Updated `CMakeLists.txt` root to reference `add_subdirectory(app)`.
- Updated `README.md` run path from `build/simulation/` to `build/app/`.

### Fixed
- `docs/SWE.1/SRS.md` SRS-SYNC-003: corrected error code from `Result::eVulkanSyncFailed`
  (non-existent) to `Result::kVkscTimeoutFailed`.
- `docs/SWE.1/SRS.md` metadata: corrected `SRS artefact ID` field from
  `aspice/SWE.1_SRS.md` to `docs/SWE.1/SRS.md`.
- `docs/SWE.1/SRS.md`: updated all `simulation/` source file paths to `app/`.
- `docs/SWE.3/DDD.md`: corrected `PipelineCacheSc::Init` signature —
  `uint8_t*` → `const uint8_t*` to match actual API.
- Engine headers: added `@satisfies SRS-xxx` Doxygen tags to align header
  documentation with SRS requirement IDs.

### Added
- `docs/SWE.2/IDD.md` (SOT-IDD-001) — Interface Design Document covering all
  public API contracts.
- `docs/SWE.4/UTS.md` (SOT-UTS-001) — Unit Test Specification (planned test cases).
- `docs/SWE.5/ITS.md` (SOT-ITS-001) — Integration Test Specification.
- `docs/SWE.6/QTS.md` (SOT-QTS-001) — Qualification Test Specification.
- `docs/plans/SVVP.md` (SOT-SVVP-001) — Software Verification & Validation Plan.
- `CHANGELOG.md` — Configuration management change log (this file).
- `tests/unit/` — Unit test source files (Result, VkscContext, CommandPool,
  FrameSync, PipelineCacheSc).
- `tests/integration/` — Integration test source files (context lifecycle,
  rendering pipeline).
- `tests/qualification/` — Qualification test source files (SRS_INIT_001,
  SRS_INIT_002, SRS_CMD_001, SRS_SYNC_001, SRS_PIPE_001).
- CMakeLists.txt files for all three test subdirectories.

---

## [0.1.0] — 2025

### Added
- Initial Soteria engine implementation:
  - `engine/core/` — `VkscContext`, `Result`, `IMemoryAllocator`, logging, safety macros.
  - `engine/rendering/` — `CommandPool`, `FrameSync`, `PipelineCacheSc`, `SwapchainSc`,
    `VertexBuffer`, `IFrameRenderer`.
  - `engine/wsi/` — `IRenderOutput`, `DisplayOutput`.
  - `engine/data/` — `IAttitudeSource`, `IFrameReport`.
  - `app/` — Avionics artificial horizon demo application (`soteria-sim`).
  - `app/rendering/` — `HorizonRenderer`, `HorizonGeometry`.
  - `tools/generate_pc_header.py` — Pipeline cache binary → C++ header converter.
- `docs/SWE.1/SRS.md` (SOT-SRS-001) — Software Requirements Specification.
- `docs/SWE.2/SAD.md` (SOT-SAD-001) — Software Architectural Design.
- `docs/SWE.3/DDD.md` (SOT-DDD-001) — Detailed Design Document.
- `docs/SWE.4/UTR.md` (SOT-UTR-001) — Unit Test Report.
- `docs/SWE.5/ITR.md` (SOT-ITR-001) — Integration Test Report.
- `docs/SWE.6/QTR.md` (SOT-QTR-001) — Qualification Test Report.

---

[Unreleased]: https://github.com/McNopper/Soteria/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/McNopper/Soteria/releases/tag/v0.1.0
