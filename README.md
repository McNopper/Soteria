# Soteria

> *Named after [Soteria](https://en.wikipedia.org/wiki/Soteria_(mythology)) — the Greek goddess of safety and preservation.*

Early-stage 2D/3D rendering engine built on **Vulkan SC**. The primary goal is
a working Vulkan SC renderer, with the project set up to stay as close as
practical to automotive **ASIL-D** (ISO 26262) and **MISRA C++:2023** practices.

Includes an **avionics artificial horizon** demo.

## Safety constraints

- MISRA C++:2023 — high warning level treated as errors (`/W4 /WX`); `.clang-tidy` holds a MISRA-aligned static-analysis profile for `engine/`, run separately via the exported `compile_commands.json`
- No exceptions, no RTTI, no dynamic allocation after init
- All logging eliminated at compile time in Release builds
- Pre-compiled pipeline cache embedded as a `constexpr` array
- Deterministic ordered shutdown

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
