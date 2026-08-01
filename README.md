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
| [tools](tools) | `generate_pc_header.py` — embeds a pipeline-cache binary as a C++ header. |

## Safety constraints (engine/)

- **MISRA C++:2023** — rule references in code and `.clang-tidy` follow the
  October 2023 numbering (not the retired MISRA C++:2008 `0-x-y` style).
- High warning level treated as errors (`/W4 /WX` plus `-Wshadow`,
  `-Wconversion`, `-Wsign-conversion`, `-Wnull-dereference`, `-Wswitch-enum`).
- No exceptions, no RTTI, no dynamic allocation after init.
- All logging eliminated at compile time in Release builds.
- Pre-compiled pipeline cache embedded as a `constexpr` array.
- Deterministic ordered shutdown.
- Deviations are annotated at the point of use with `MISRA_DEVIATION()`
  ([engine/core/safety_macros.hpp](engine/core/safety_macros.hpp)) plus a
  matching `// NOLINT(check-name)` comment. Currently one deviation exists:
  Rule 8.2.6 (void* cast) for Vulkan SC mapped memory in
  [engine/rendering/vertex_buffer.cpp](engine/rendering/vertex_buffer.cpp).
  A second deviation covers the debug-only use of C stdio logging
  (Rule 30.0.1, compiled out of production builds) in
  [engine/core/log.cpp](engine/core/log.cpp).

## Static analysis

`.clang-tidy` holds a MISRA C++:2023-aligned profile for `engine/` (rule
references verified against the published document). CMake exports
`compile_commands.json`, so no extra configuration is needed:

```powershell
$clangTidy = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
Get-ChildItem engine -Recurse -Filter *.cpp | ForEach-Object {
    & $clangTidy -p build $_.FullName
}
```

All findings are treated as errors (`WarningsAsErrors: '*'`).

## Tests

| Location | Description |
|----------|-------------|
| [tests/unit](tests/unit) | Unit tests |
| [tests/integration](tests/integration) | Integration tests |
| [tests/qualification](tests/qualification) | Qualification tests |

Tests are built by default (GoogleTest is fetched automatically; disable with
`-DSOTERIA_BUILD_TESTS=OFF`). Run them with the Vulkan SC runtime on `PATH`:

```powershell
$env:Path = "$env:VULKANSC_SDK\bin;$env:Path"
$env:VK_ADD_DRIVER_FILES = "$env:VULKANSC_SDK\share\vulkansc\icd.d\vksconvk.json"
ctest --test-dir build --output-on-failure
```

Integration and qualification tests that need a real device skip automatically
when no compatible Vulkan SC device is available.

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
$env:VK_ADD_DRIVER_FILES  = "C:\VulkanSC-SDK\1.0.21\share\vulkansc\icd.d\vksconvk.json"
$env:VKSC_EMULATION_DEBUG = "error"
.\build\app\soteria-sim.exe
```

## License

MIT — see [LICENSE](LICENSE).

> Vulkan SC is a trademark of The Khronos Group Inc.
> This project is not affiliated with or endorsed by The Khronos Group.
