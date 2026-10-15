# Soteria — Development Guide

> Operational companion to [ARCHITECTURE.md](ARCHITECTURE.md): environment,
> build, test, analysis lanes, and the troubleshooting table. For feature
> plans see [ROADMAP.md](ROADMAP.md).

## 1. Environment

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| CMake | ≥ 3.22 | 3.24+ required for PCC discovery on Windows |
| clang-cl | ≥ 16 | VS LLVM component (`VC\Tools\Llvm\x64\bin`) |
| VulkanSC SDK | 1.0.21 | [release `vksc1.0.21`](https://github.com/KhronosGroup/VulkanSC-SDK/releases/tag/vksc1.0.21) |
| Python | ≥ 3.10 | analysis lanes + tools |
| cppcheck | any recent | optional; enables the `cppcheck*` targets |

### SDK install

Install to the recommended versioned path (mirrors the Vulkan SDK
convention):

```
C:\VulkanSC-SDK\1.0.21\   ← bin/ include/ lib/ share/ setup-env.bat
```

The Windows ZIP embeds the `1.0.21/` folder, so extracting it at
`C:\VulkanSC-SDK\` is sufficient. Verify the archive against the SHA-256
digest published on the release page before extracting.

### Environment variables

| Variable | Value | When needed | Scope set by installer |
|----------|-------|-------------|------------------------|
| `VULKANSC_SDK` | `C:\VulkanSC-SDK\1.0.21` | configure (preset reads `$env{VULKANSC_SDK}`), PCC discovery | persistent (user) |
| `VulkanSC_ROOT` | same | `find_package(VulkanSC)` fallback | persistent (user) |
| `Path` += | `%VULKANSC_SDK%\bin` | running exes without staged DLLs | per-session (`setup-env.bat`) |
| `VK_ADD_DRIVER_FILES` | `%VULKANSC_SDK%\share\vulkansc\icd.d\vksconvk.json` | runtime: registers the emulation ICD | per-session |
| `VK_ADD_LAYER_PATH` | `%VULKANSC_SDK%\share\vulkansc\explicit_layer.d` | dev only: validation / devsim layers | per-session |
| `VKSC_EMULATION_DEBUG` | `error` | runtime: quiet emulation ICD logging | per-session |

`setup-env.bat` in the SDK root sets the session-scoped ones for `cmd.exe`.

> The MISRA C++:2023 reference copy under `references/` is licensed per-copy
> and gitignored. Never include it in release archives or CI artifacts
> (notices in [README.md](../README.md) and [DEVIATIONS.md](DEVIATIONS.md)).

PowerShell equivalent:

```powershell
$env:VULKANSC_SDK        = "C:\VulkanSC-SDK\1.0.21"
$env:Path                = "$env:VULKANSC_SDK\bin;$env:Path"
$env:VK_ADD_DRIVER_FILES = "$env:VULKANSC_SDK\share\vulkansc\icd.d\vksconvk.json"
$env:VK_ADD_LAYER_PATH   = "$env:VULKANSC_SDK\share\vulkansc\explicit_layer.d"
$env:VKSC_EMULATION_DEBUG = "error"
```

## 2. Build

Use a *Visual Studio Developer PowerShell* so clang-cl finds the MSVC
headers and libraries.

```powershell
cmake --preset debug          # configure (needs VULKANSC_SDK)
cmake --build --preset debug  # build
ctest --preset debug          # all tests
```

`release` presets exist for the production-like configuration (logging
compiled out). Both `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` are pinned
to `clang-cl` by the presets — GoogleTest enables the C language, and an
unset C compiler makes CMake pick GNU-style `clang.exe`, which rejects the
MSVC-style flags.

## 3. Test lanes

| Label | Content | Device needed? |
|-------|---------|----------------|
| `unit` | offline logic (Result, FixedString, FrameBudgetMonitor, cache-header validation, …) | no |
| `integration` | component init on a real `VkDevice` | yes (emulation ICD) |
| `qualification` | requirement-level scenarios | yes (emulation ICD) |
| `analysis` | `test_layers`, `test_tidy` | no (needs toolchain/DB) |

Device-dependent tests `GTEST_SKIP()` when no Vulkan SC device is present.
Analysis lanes exit 77 → ctest SKIP when their input is missing.

```powershell
ctest --preset debug                 # everything
ctest --test-dir build -L unit       # offline only
ctest --test-dir build -L analysis   # static analysis lanes
```

Runtime DLL staging: `soteria_stage_runtime_dlls()` (top-level
CMakeLists.txt) copies `vulkansc-1.dll` and `vksconvk.dll` next to every
executable at build time — no `Path` change needed to run tests or the demo
from the build tree.

## 4. Static analysis

| Lane | Command | Fails on |
|------|---------|----------|
| clang-tidy (MISRA profile) | `python tools/check_tidy.py` | every finding (`WarningsAsErrors: '*'`), engine/ scope |
| layer rules | `python tools/check_layers.py` | forbidden includes (see ARCHITECTURE.md) |
| cppcheck | `cmake --build build --target cppcheck` | warning/performance/portability findings |
| cppcheck strict | `cmake --build build --target cppcheck-strict` | `--enable=all --inconclusive` (opt-in) |

`check_tidy.py` resolves clang-tidy via `vswhere` and runs all engine TUs in
parallel; full output lands in `build/clang_tidy.log`.

**Local lanes are the strictest authority.** The clang-tidy profile treats
*every* MISRA C++:2023 finding as an error (`WarningsAsErrors: '*'`) —
stricter than any generic/static recommendation from outside tooling. When a
new check is adopted, it must not weaken the existing lanes; where a rule
overlaps, MISRA C++:2023 as expressed in `.clang-tidy` and
[DEVIATIONS.md](DEVIATIONS.md) decides.

**Lane scope & convergence:**

- `check_layers.py` proves the one-way include rules as a pure blacklist and
  needs no toolchain; as an *includer* it gates `engine/*` blocks only —
  `app/` and `tests/` are currently unchecked (the `tests` row in
  ARCHITECTURE.md is documented but not yet enforced).
- A stricter contract-style gate (default-deny edges, include-cycle detection,
  generated diagrams) is planned (ROADMAP P1-13). It would overlap `check_layers.py`
  on the one-way rules: keep exactly **one** gate for each fact once the
  contract gate is deterministic and CI-wired — until then the local lane
  stays, because a gate that can pass or fail nondeterministically is weaker
  evidence than a simpler one that cannot.

## 5. Pipeline cache workflow

1. Author shaders (`app/shaders/*.vert/.frag`) and pipeline JSON
   (`app/pipeline_src/*.pc.json`).
2. Compile offline with the PCC (`pcconvk`) → `app/pipeline_src/pipeline_cache.bin`.
   Pool sizes: `vkscpcinfo` lists per-pipeline memory; keep
   `HorizonRenderer::kPoolEntrySize` ≥ the largest entry.
3. Regenerate the embedded header:
   `python tools/generate_pc_header.py app/pipeline_src/pipeline_cache.bin app/rendering/pipeline_cache_data.hpp`
4. Rebuild. The unit test `PipelineCacheHeaderTest.RealEmbeddedCacheIsWellFormed`
   validates the shipped blob on every run.

## 6. Structure & change discipline

- **Two hats:** structure-only changes (extract function, de-duplication) are
  behavior-preserving. Run the full gate set before and after every batch:
  `ctest --preset debug`, `python tools/check_layers.py`,
  `python tools/check_tidy.py`, `cppcheck` — all green, no new findings.
- **Small batches:** one technique per commit-sized change, so the gates stay
  attributable to a single edit.
- **Extract on sight:** duplicated Vulkan struct-setup/query blocks and
  repeated `Result`→health-event mappings are extraction seams, not
  conventions to re-type.
- **Function size:** frame-loop stages are documented seams (ARCHITECTURE.md
  §Frame schedule); `main()` currently far exceeds any reasonable target —
  splitting it is tracked in ROADMAP P1-15.
- **Generated files:** `app/rendering/pipeline_cache_data.hpp` is generated by
  `tools/generate_pc_header.py` — never edit it directly (§5).
- **MISRA C++:2023 always wins:** no exceptions, no RTTI, no allocation after
  init, deviation at point of use + register entry
  ([DEVIATIONS.md](DEVIATIONS.md)). Generic refactoring-catalog moves that
  conflict with this (e.g. exception-based error paths, RTTI-driven
  polymorphism) are out of scope by rule.

## 7. Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `No pipeline cache compiler configuration found … Available options:` (empty) at configure | `VULKANSC_SDK` not set in the *current* shell — PCC discovery globs `$env{VULKANSC_SDK}/share/vulkansc/pcc.d/*.json` | set it in-session (see §1) and re-run `cmake --preset debug` |
| `Error running test executable` during build (gtest discovery) | test exe cannot load `vulkansc-1.dll` | rebuild — `soteria_stage_runtime_dlls()` stages the DLLs; on older trees, add `%VULKANSC_SDK%\bin` to `Path` |
| Tests skip with "VulkanSC not available" | emulation ICD not registered | set `VK_ADD_DRIVER_FILES` (§1); check `VKSC_EMULATION_DEBUG=error` output |
| App picks the native NVIDIA SC driver instead of emulation | hardware ICD coexists | already handled: `VkscContextConfig::preferredDriverId = VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN` prefers emulation (the embedded cache is built for it) |
| clang-tidy finds nothing / skips | stale or missing `compile_commands.json` | re-run `cmake --preset debug` (exports the DB), then `python tools/check_tidy.py` |
| `vksconvk.dll` copy fails at build | SDK moved | point `CMAKE_PREFIX_PATH`/`VULKANSC_SDK` at the actual SDK root and reconfigure |
