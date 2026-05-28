# Soteria

> *Named after [Soteria](https://en.wikipedia.org/wiki/Soteria_(mythology)) — the Greek goddess of safety and preservation.*

Early-stage safety-critical 2D/3D rendering engine built on **Vulkan SC**,
targeting ISO 26262 (ASIL-D) and DO-178C (DAL-A).

Includes an **avionics artificial horizon** demo.

## Safety constraints

- MISRA C++:2023 enforced via Clang `/WX` + `.clang-tidy`
- No exceptions, no RTTI, no dynamic allocation after init
- All logging eliminated at compile time in Release builds
- Pre-compiled pipeline cache embedded as a `constexpr` array
- Deterministic ordered shutdown

## ASPICE compliance

Soteria follows the **Automotive SPICE® SWE process chain** (SWE.1–SWE.6).
All work products live under `docs/`:

| Process | Document | Description |
|---------|----------|-------------|
| SWE.1 | [docs/SWE.1/SRS.md](docs/SWE.1/SRS.md) | Software Requirements Specification |
| SWE.2 | [docs/SWE.2/SAD.md](docs/SWE.2/SAD.md) | Software Architectural Design |
| SWE.2 | [docs/SWE.2/IDD.md](docs/SWE.2/IDD.md) | Interface Design Document |
| SWE.3 | [docs/SWE.3/DDD.md](docs/SWE.3/DDD.md) | Detailed Design Document |
| SWE.4 | [docs/SWE.4/UTS.md](docs/SWE.4/UTS.md) | Unit Test Specification |
| SWE.4 | [docs/SWE.4/UTR.md](docs/SWE.4/UTR.md) | Unit Test Report |
| SWE.5 | [docs/SWE.5/ITS.md](docs/SWE.5/ITS.md) | Integration Test Specification |
| SWE.5 | [docs/SWE.5/ITR.md](docs/SWE.5/ITR.md) | Integration Test Report |
| SWE.6 | [docs/SWE.6/QTS.md](docs/SWE.6/QTS.md) | Qualification Test Specification |
| SWE.6 | [docs/SWE.6/QTR.md](docs/SWE.6/QTR.md) | Qualification Test Report |
| Plans | [docs/plans/SVVP.md](docs/plans/SVVP.md) | Software Verification & Validation Plan |

See [docs/README.md](docs/README.md) for the full process overview and traceability summary.

## Prerequisites

- CMake ≥ 3.22
- Clang / clang-cl ≥ 16
- [VulkanSC SDK 1.0.21](https://github.com/KhronosGroup/VulkanSC-SDK/releases/tag/vksc1.0.21)

## Build (Windows)

```powershell
$env:VULKANSC_SDK = "C:\VulkanSC-SDK\1.0.21"

cmake -S . -B build `
      -DCMAKE_BUILD_TYPE=Debug `
      -DCMAKE_PREFIX_PATH="$env:VULKANSC_SDK" `
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
