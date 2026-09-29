# Soteria

> *Named after [Soteria](https://en.wikipedia.org/wiki/Soteria_(mythology)) — the Greek goddess of safety and preservation.*

Early-stage 2D/3D rendering engine built on **Vulkan SC**. The primary goal is
a working Vulkan SC renderer, with the project set up to stay as close as
practical to automotive **ASIL-D** (ISO 26262) and **MISRA C++:2023** practices.

Includes an **avionics artificial horizon** demo.

## Layout

| Location | Description |
|----------|-------------|
| [engine](engine) | Safety-critical core: Vulkan SC context, swapchain, pipeline cache, vertex buffer, frame sync, command pool, WSI and data interfaces. Written to MISRA C++:2023. |
| [app](app) | Simulation/desktop demo: artificial-horizon renderer, demo attitude source, console telemetry. Not part of the certifiable layer. |
| [tests](tests) | GoogleTest suites (see below). |
| [tools](tools) | `generate_pc_header.py` — embeds a pipeline-cache binary as a C++ header. `check_tidy.py` / `check_layers.py` — static-analysis and layer-rule lanes (see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)). |
| [docs](docs) | [Architecture](docs/ARCHITECTURE.md) · [development guide](docs/DEVELOPMENT.md) · [roadmap](docs/ROADMAP.md) · [MISRA deviation register](docs/DEVIATIONS.md) · [research](docs/research/vulkan-sc-safety-critical-features.md) ([index](docs/README.md)) |
| [cmake](cmake) | cppcheck integration (see Static analysis). |

## Safety constraints (engine/)

- **MISRA C++:2023** — rule references in code and `.clang-tidy` follow the
  October 2023 numbering (not the retired MISRA C++:2008 `0-x-y` style),
  verified against the published document.
- High warning level treated as errors (`/W4 /WX` plus `-Wshadow`,
  `-Wconversion`, `-Wsign-conversion`, `-Wnull-dereference`, `-Wswitch-enum`).
- No exceptions, no RTTI, no dynamic allocation after init.
- All logging eliminated at compile time in Release builds.
- Pre-compiled pipeline cache embedded as a `constexpr` array and
  integrity-validated at init (56-byte Vulkan SC header: size, version,
  index bounds; vendor/device mismatch logged — the driver stays the
  authoritative compatibility check).
- **Fault & health supervision** — core Vulkan SC fault handling
  (`vkGetFaultData` drained per frame), classified health events
  (`IHealthMonitor`), frame-deadline monitoring (`FrameBudgetMonitor`) and a
  heartbeat seam (`IHeartbeat`) for external watchdogs. Detection lives in
  the engine; reaction policy is injected by the application.
- Implementation identity (vendor/device/driver/api) captured at init
  (SCID-7 configuration record).
- Deterministic ordered shutdown.
- Deviations are annotated at the point of use with `MISRA_DEVIATION()`
  ([engine/core/safety_macros.hpp](engine/core/safety_macros.hpp)) plus a
  matching `// NOLINT(check-name)` comment, and recorded with full rationale
  in the [deviation register](docs/DEVIATIONS.md) — currently DEV-001
  (Rule 8.2.6, `void*` cast for mapped memory) and DEV-002 (Rule 30.0.1,
  debug-only C stdio logging).

## Static analysis

`.clang-tidy` holds a MISRA C++:2023-aligned profile for `engine/` (rule
references verified against the published document). All findings are treated
as errors (`WarningsAsErrors: '*'`). Three analysis lanes are wired into the
normal `ctest` flow (each skips with code 77 when its toolchain is absent):

```powershell
# clang-tidy over engine/ translation units (parallel, needs a configured build)
python tools/check_tidy.py

# Architectural layer rules — include-graph proof, no toolchain needed
python tools/check_layers.py

# cppcheck — second, independent analyzer
cmake --build build --target cppcheck

# or all of them through ctest:
ctest --test-dir build -L analysis --output-on-failure
```

`tools/check_tidy.py` resolves clang-tidy from the newest Visual Studio
install via `vswhere`; `cmake/cppcheck.cmake` registers `cppcheck`,
`cppcheck-strict` and `cppcheck-xml` targets with justified suppressions in
[cppcheck.supp](cppcheck.supp).

## Tests

| Location | Description |
|----------|-------------|
| [tests/unit](tests/unit) | Unit tests |
| [tests/integration](tests/integration) | Integration tests |
| [tests/qualification](tests/qualification) | Qualification tests |

Tests are built by default (GoogleTest is fetched automatically; disable with
`-DSOTERIA_BUILD_TESTS=OFF`). Runtime DLLs (`vulkansc-1.dll`, `vksconvk.dll`)
are staged next to every test executable at build time — running tests only
needs the emulation ICD registered:

```powershell
$env:VK_ADD_DRIVER_FILES = "$env:VULKANSC_SDK\share\vulkansc\icd.d\vksconvk.json"
ctest --preset debug                     # or: ctest --test-dir build --output-on-failure
```

Test labels: `unit` (offline), `integration`, `qualification` (device
needed), `analysis` (static analysis lanes). Select with
`ctest --test-dir build -L <label>`. Integration and qualification tests that
need a real device skip automatically when no compatible Vulkan SC device is
available; analysis lanes skip when their toolchain is absent.

## Prerequisites

- CMake ≥ 3.22
- Clang / clang-cl ≥ 16
- [VulkanSC SDK 1.0.21](https://github.com/KhronosGroup/VulkanSC-SDK/releases/tag/vksc1.0.21)

## Build (Windows)

The build uses the **Vulkan SC SDK** and the Clang/MSVC toolchain. Run the
commands from a *Visual Studio Developer PowerShell* (so `clang-cl` finds the
MSVC headers and libraries).

> **Note:** both `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` must be set to
> `clang-cl`. GoogleTest (fetched for the test suite) enables the C language, so
> leaving the C compiler unset makes CMake pick GNU-style `clang.exe` and the
> MSVC-style flags fail to compile.

```powershell
$env:VULKANSC_SDK = "C:\VulkanSC-SDK\1.0.21"

# Preset flow (CMakePresets.json):
cmake --preset debug
cmake --build --preset debug

# Equivalent explicit flow:
cmake -S . -B build `
      -DCMAKE_BUILD_TYPE=Debug `
      -DCMAKE_PREFIX_PATH="$env:VULKANSC_SDK" `
      -DCMAKE_C_COMPILER=clang-cl `
      -DCMAKE_CXX_COMPILER=clang-cl `
      -GNinja

cmake --build build --config Debug
```

## Run

```powershell
$env:VK_ADD_DRIVER_FILES  = "$env:VULKANSC_SDK\share\vulkansc\icd.d\vksconvk.json"
$env:VKSC_EMULATION_DEBUG = "error"
.\build\app\soteria-sim.exe
```

The demo renders 600 frames (~10 s at 60 fps) and shuts down cleanly.
Environment details, the pipeline-cache regeneration workflow and a
troubleshooting table are in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## License

MIT — see [LICENSE](LICENSE).

This project references **MISRA C++:2023** rule numbers and categories for
compliance traceability (see [docs/DEVIATIONS.md](docs/DEVIATIONS.md) and
`.clang-tidy`); rule descriptions are short paraphrases. The guidelines
document itself is © The MISRA Consortium Limited, licensed per-copy, and is
**not** distributed with this project — the local reference copy under
`references/` is gitignored. "MISRA", "MISRA C" and "MISRA C++" are
registered trademarks of The MISRA Consortium Limited; no endorsement is
implied.

> Vulkan SC is a trademark of The Khronos Group Inc.
> This project is not affiliated with or endorsed by The Khronos Group.
