# SWE.1 — Software Requirements Specification (SRS)
## Soteria — Vulkan SC Safety-Critical Rendering Engine

| Field            | Value                                               |
|------------------|-----------------------------------------------------|
| Document ID      | SOT-SRS-001                                        |
| Version          | 0.1.0 (draft)                                      |
| Date             | 2025                                               |
| Status           | DRAFT — pending SWE.1 review (consensus not reached) |
| Standards        | ISO 26262:2018 (ASIL-D), DO-178C (DAL-A)           |
| Process          | ASPICE PAM 4.0 SWE.1                               |
| Source repo      | McNopper/Soteria                                   |
| SRS artefact ID  | docs/SWE.1/SRS.md                                   |

---

## 1. Scope and System Context

### 1.1 System Overview

Soteria is an early-stage safety-critical 2D/3D rendering engine built on **Vulkan SC**
(Safety Critical), targeting:

- **ISO 26262:2018 ASIL-D** — automotive functional safety (highest integrity level)
- **DO-178C DAL-A** — airborne software (highest design assurance level)

The system renders real-time avionics and automotive HMI graphics.  The reference
demonstration implements an **avionics artificial horizon** (attitude indicator): a
primary flight display (PFD) element showing aircraft roll and pitch relative to the
horizon.

### 1.2 System Boundary

Soteria software covers:

- Vulkan SC device context lifecycle management
- WSI output surface management (direct display, AR overlay, AR see-through HUD)
- Swapchain, frame synchronisation, and command recording
- Geometry generation for attitude-dependent flight symbology
- Attitude data acquisition via injected data source interface
- Frame telemetry reporting via injected reporting interface

Out of scope (handled externally or by the OS/SDK):

- Vulkan SC driver and firmware
- VulkanSC SDK installation
- Physical display hardware
- Inertial measurement unit (IMU) or AHRS hardware

### 1.3 Stakeholders

| Stakeholder         | Role                                                    |
|---------------------|---------------------------------------------------------|
| System integrator   | Integrates Soteria into automotive/avionics product     |
| Safety assessor     | Audits compliance with ISO 26262 / DO-178C              |
| Pilot / operator    | Relies on attitude symbology for flight/vehicle state   |
| Software developer  | Implements and maintains Soteria                        |

---

## 2. Definitions and Abbreviations

| Term / Abbrev | Definition                                            |
|---------------|-------------------------------------------------------|
| AHRS          | Attitude and Heading Reference System                 |
| ASIL          | Automotive Safety Integrity Level (A–D)               |
| DAL           | Design Assurance Level (A–E, A=highest)               |
| HMI           | Human-Machine Interface                               |
| NDC           | Normalised Device Coordinates                         |
| PFD           | Primary Flight Display                                |
| RTTI          | Run-Time Type Information                             |
| SC            | Safety Critical                                       |
| WSI           | Window System Integration                             |
| VkSC          | Vulkan Safety Critical API (Khronos)                  |

---

## 3. Requirements

Requirements are numbered `SRS-<category>-<NNN>` and use **shall** language.
Each requirement includes:
- **Source** — stakeholder, standard, or code artefact that originated it
- **Acceptance criterion** — measurable condition that proves the requirement is met
- **ASIL** — safety integrity level applicable (D = highest; `-` = non-safety)

### 3.1 System Startup and Initialisation

---

**SRS-INIT-001**  
The software shall initialise the Vulkan SC instance, physical device, logical device,
and graphics queue during system startup, before any rendering activity begins.

- Source: `engine/core/vksc_context.hpp`, ISO 26262 Part 6 §8.4.5  
- Acceptance: System startup completes without error; all handles are non-null.  
- ASIL: D

---

**SRS-INIT-002**  
The software shall declare all Vulkan SC resource reservations upfront via
`VkscResourceReservation` / `VkscContextConfig` before device creation, and shall not
exceed those reservations at runtime.

- Source: Vulkan SC spec §2.6 (pre-allocation mandate), `vksc_context.cpp`  
- Acceptance: No Vulkan SC resource-allocation error occurs after device creation;
  resource-count assertions pass during qualification testing.  
- ASIL: D

---

**SRS-INIT-003**  
The software shall select the physical device using a two-pass algorithm: first
attempt to match a caller-specified driver ID; if no match is found, select the
first available device that supports the required queue family and surface.

- Source: `vksc_context.cpp`, system integrator requirement (deterministic device selection)  
- Acceptance: Device selection succeeds on targets with exactly one GPU; on multi-GPU
  targets, the preferred driver ID takes priority.  
- ASIL: D

---

**SRS-INIT-004**  
The software shall validate the availability of all required Vulkan SC extensions and
device features at startup and shall return an unambiguous error code if any required
capability is absent.

- Source: `engine/core/result.hpp` (Result enum), ISO 26262 Part 6 §8.4.4  
- Acceptance: Startup with a minimal-feature device returns `Result::kNotFound`;
  missing-extension case is covered by a qualification test.  
- ASIL: D

---

**SRS-INIT-005**  
The software shall perform deterministic, ordered shutdown of all Vulkan SC objects
in the reverse order of creation, with no outstanding GPU operations at the time
of destruction.

- Source: README.md "Deterministic ordered shutdown", `main.cpp` shutdown sequence  
- Acceptance: Valgrind / Vulkan SC validation layer reports zero handle-leak warnings
  after clean shutdown.  
- ASIL: D

---

### 3.2 Output Surface and WSI

---

**SRS-WSI-001**  
The software shall support at minimum three output surface modes selectable at
integration time:

| Mode identifier         | Vulkan format              | Use case                  |
|-------------------------|----------------------------|---------------------------|
| `eDirectDisplay`        | `VK_FORMAT_B8G8R8A8_UNORM` | Standalone display panel  |
| `eArOverlay`            | `VK_FORMAT_R8G8B8A8_UNORM` | Composited AR overlay     |
| `eArSeeThroughHud`      | `VK_FORMAT_R16G16B16A16_SFLOAT` | HDR see-through HUD  |

- Source: `engine/wsi/i_render_output.hpp`, system integrator requirement  
- Acceptance: Each mode compiles and initialises without error on a reference target.  
- ASIL: D

---

**SRS-WSI-002**  
The `eDirectDisplay` backend shall select the display using `VK_KHR_display`: it shall
enumerate available displays, select the first display, select the plane with the
largest available mode, and create a `VkDisplayPlaneSurfaceKHR`.

- Source: `engine/wsi/display_output.cpp`, Vulkan SC spec VK_KHR_display  
- Acceptance: Surface creation succeeds on a single-monitor target; surface handle is non-null.  
- ASIL: D

---

**SRS-WSI-003**  
The WSI backend selection shall be configurable at compile time or link time via the
`IRenderOutput` interface; the engine shall not hard-code any specific output backend.

- Source: `engine/wsi/i_render_output.hpp` (abstract interface pattern)  
- Acceptance: Two different `IRenderOutput` implementations can be linked against the
  same engine library with no engine source changes.  
- ASIL: `-`

---

### 3.3 Swapchain

---

**SRS-SWP-001**  
The software shall create a Vulkan SC swapchain with a maximum of **three images** and
shall not attempt to acquire more images than the swapchain contains.

- Source: `engine/rendering/swapchain.hpp` (`k_max_images = 3`), ISO 26262 bounded-resource constraint  
- Acceptance: Swapchain creation succeeds with 1, 2, or 3 images; creation with 4+ images
  is rejected at configuration time.  
- ASIL: D

---

**SRS-SWP-002**  
The software shall validate that the swapchain surface format matches the format
required by the active `IRenderOutput` backend and shall return an error if it does not.

- Source: `engine/rendering/swapchain.cpp` format validation, `engine/wsi/i_render_output.hpp`  
- Acceptance: Swapchain creation with a mismatched format returns `Result::kVkscSwapchainFailed`.  
- ASIL: D

---

**SRS-SWP-003**  
The software shall create one `VkImageView` per swapchain image; views shall be
destroyed before the swapchain handle.

- Source: `engine/rendering/swapchain.hpp/.cpp`, Vulkan resource lifetime rules  
- Acceptance: Validation layer reports no dangling-view errors on swapchain destruction.  
- ASIL: D

---

### 3.4 Frame Synchronisation

---

**SRS-SYNC-001**  
The software shall use per-frame semaphores (image-available and render-complete) and a
per-frame fence to ensure that a command buffer is not re-recorded while the GPU is
still executing it.

- Source: `engine/rendering/frame_sync.hpp/.cpp`, ISO 26262 race-condition prevention  
- Acceptance: Under 10 000 consecutive frames, no frame-overlap GPU corruption is
  detected by the Vulkan SC validation layer.  
- ASIL: D

---

**SRS-SYNC-002**  
The per-frame fence shall be initialised in the **signalled** state so that the first
call to `WaitAndReset` succeeds immediately without blocking.

- Source: `engine/rendering/frame_sync.cpp` (fence creation with `VK_FENCE_CREATE_SIGNALED_BIT`)  
- Acceptance: First-frame fence wait completes within one render cycle without timeout.  
- ASIL: D

---

**SRS-SYNC-003**  
Fence wait shall time out after a maximum of **5 seconds**; on timeout, the software
shall return an unambiguous error code (`Result::kVkscTimeoutFailed`) and shall not
proceed to command buffer recording.

- Source: `engine/rendering/frame_sync.cpp` (5 s timeout), safety requirement — prevent hang  
- Acceptance: Injected GPU stall of >5 s triggers `Result::kVkscTimeoutFailed` without process hang.  
- ASIL: D

---

### 3.5 Pipeline Cache

---

**SRS-PIPE-001**  
The software shall load the Vulkan SC pipeline cache exclusively from a pre-compiled
binary embedded at compile time as a `constexpr` array; it shall not load pipeline
cache data from the filesystem at runtime.

- Source: `engine/rendering/pipeline_cache.hpp/.cpp`, README "Pre-compiled pipeline cache",
  Vulkan SC spec §10.7 (pipeline cache mandatory for SC), DO-178C §12 (no runtime
  compilation of safety-critical code)  
- Acceptance: The production binary contains zero file I/O calls related to pipeline data;
  confirmed by static analysis or binary inspection.  
- ASIL: D

---

**SRS-PIPE-002**  
The `tools/generate_pc_header.py` tool shall convert an offline-compiled Vulkan SC
pipeline cache binary into a C++ `constexpr` byte-array header that can be compiled
into the engine binary.

- Source: `tools/generate_pc_header.py`, SRS-PIPE-001 derived requirement  
- Acceptance: The generated header compiles without warnings; a `static_assert` verifies
  the embedded array size matches the source binary byte count.  
- ASIL: `-` (build tool, not runtime)

---

### 3.6 Vertex Buffer

---

**SRS-VBF-001**  
The software shall allocate the vertex buffer as **host-visible and host-coherent**
with **persistent mapping**: the buffer shall be mapped once at creation and remain
mapped for the lifetime of the vertex buffer object; no per-frame map/unmap calls
are permitted.

- Source: `engine/rendering/vertex_buffer.hpp/.cpp`, ISO 26262 constraint: eliminate
  dynamic allocation at runtime  
- Acceptance: The buffer map function is called exactly once per buffer lifetime,
  confirmed by code review and static analysis.  
- ASIL: D

---

**SRS-VBF-002**  
The vertex buffer size shall be specified at construction time as a byte count; the
software shall not resize the buffer after creation.

- Source: `engine/rendering/vertex_buffer.hpp`, bounded-resource constraint  
- Acceptance: Attempt to upload more data than the buffer capacity returns an error
  or is rejected at compile time.  
- ASIL: D

---

### 3.7 Command Pool and Command Buffers

---

**SRS-CMD-001**  
The software shall create the Vulkan SC command pool with upfront reservation parameters
(`VkCommandPoolMemoryReservationCreateInfo`) sufficient for the maximum number of
command buffers and commands used in one render frame.

- Source: `engine/rendering/command_pool.hpp/.cpp`, Vulkan SC spec §6.1 (upfront reservation)  
- Acceptance: Command pool creation succeeds; Vulkan SC validation reports no
  reservation-exceeded events during a 10 000-frame soak test.  
- ASIL: D

---

**SRS-CMD-002**  
The command pool shall allocate only **primary** command buffers; secondary command
buffers are not used.

- Source: `engine/rendering/command_pool.hpp`  
- Acceptance: Code review confirms `VK_COMMAND_BUFFER_LEVEL_PRIMARY` exclusively.  
- ASIL: `-`

---

### 3.8 Renderer Interface

---

**SRS-REND-001**  
Every renderer component shall implement the `IFrameRenderer` interface, which defines:

| Method       | Contract                                             |
|--------------|------------------------------------------------------|
| `Init`       | Allocate all Vulkan resources; return Result         |
| `Shutdown`   | Release all Vulkan resources; must be idempotent     |
| `RecordFrame`| Record one frame into the provided command buffer    |

- Source: `engine/rendering/i_frame_renderer.hpp`  
- Acceptance: Any class that implements `IFrameRenderer` can replace `HorizonRenderer`
  in `main.cpp` with zero engine changes.  
- ASIL: D

---

**SRS-REND-002**  
`IFrameRenderer::RecordFrame` shall accept the command buffer, the framebuffer image
index, and the current `AttitudeData`; it shall not perform GPU submissions or present
operations — those are the caller's responsibility.

- Source: `engine/rendering/i_frame_renderer.hpp`, separation-of-concerns safety pattern  
- Acceptance: `RecordFrame` contains no `vkQueueSubmit`, `vkQueuePresentKHR`, or
  equivalent calls; confirmed by static analysis.  
- ASIL: D

---

### 3.9 Attitude Data Interface

---

**SRS-ATT-001**  
The software shall acquire all attitude data through the `IAttitudeSource` interface;
the engine shall not read attitude data directly from hardware registers, files, or
network sockets.

- Source: `engine/data/i_attitude_source.hpp`, testability and safety isolation requirement  
- Acceptance: Replacing `DemoAttitudeSource` with a stub in the test harness requires
  no engine source changes.  
- ASIL: D

---

**SRS-ATT-002**  
The `AttitudeData` structure shall carry a `valid` flag; the renderer shall check
this flag on every frame and shall display unambiguous failure symbology when
`valid == false`.

- Source: `engine/data/i_attitude_source.hpp` (valid field), `app/rendering/horizon_renderer.cpp`
  (failure symbology), ISO 26262 Part 8 §6.5 (safe-state on sensor failure)  
- Acceptance: Setting `valid = false` in the injected source causes the horizon
  renderer to display the defined failure indicator within one frame with no crash.  
- ASIL: D

---

**SRS-ATT-003**  
Attitude angles shall be defined with the following sign conventions:

| Angle   | Convention                           | Range          |
|---------|--------------------------------------|----------------|
| Roll    | Right-wing-down positive             | ±180°          |
| Pitch   | Nose-up positive                     | ±90°           |
| Heading | Clockwise from north, 0 = north      | 0° .. 360°     |

- Source: `engine/data/i_attitude_source.hpp` (documented conventions), aviation standard  
  (ARINC 429, DO-229E)  
- Acceptance: DemoAttitudeSource oscillates to ±25° roll and ±10° pitch; visual
  inspection confirms correct symbol orientation.  
- ASIL: D

---

### 3.10 Horizon Renderer (HMI Symbology)

---

**SRS-HOR-001**  
The horizon renderer shall render the following symbology elements on every valid frame:

| Symbol              | Description                                            |
|---------------------|--------------------------------------------------------|
| Sky / earth         | Blue upper half and brown lower half split at horizon  |
| Horizon line        | Straight line rotated by roll angle                    |
| Pitch ladder        | Horizontal lines at ±5°, ±10°, ±15° pitch              |
| Roll arc            | Arc indicating roll angle with tick marks at ±10/20/30/45/60° |
| Zero-roll marker    | Fixed triangle at 0° roll position                     |
| Aircraft symbol     | Fixed centred aircraft reference mark                  |

- Source: `app/rendering/horizon_geometry.hpp/.cpp`, avionics PFD standard
  (ARINC 661, SAE AS8034B)  
- Acceptance: Visual inspection of demo output confirms all six elements are rendered;
  automated pixel-comparison test against a reference screenshot passes.  
- ASIL: D

---

**SRS-HOR-002**  
The horizon geometry shall be computed entirely in NDC (Normalised Device Coordinates)
using only deterministic fixed-point-compatible floating-point arithmetic; no
allocation shall occur during geometry generation.

- Source: `app/rendering/horizon_geometry.hpp/.cpp`  
- Acceptance: `HorizonGeometry` generates exactly **94 vertices** per frame; no heap
  allocation occurs during a 10 000-frame soak; `valgrind --tool=massif` baseline
  shows flat heap after init.  
- ASIL: D

---

**SRS-HOR-003**  
The horizon renderer shall use **push constants** of exactly **32 bytes** to pass
per-frame rendering parameters (cosRoll, sinRoll, pitchNdc, RGBA) to the shader;
it shall not use uniform buffers for per-frame data.

- Source: `app/rendering/horizon_renderer.hpp/.cpp` (32-byte push constant layout),
  ISO 26262 deterministic-latency requirement  
- Acceptance: Shader compilation succeeds; push constant layout is validated against
  the pipeline layout at Init time.  
- ASIL: D

---

**SRS-HOR-004**  
The horizon renderer shall use exactly **two graphics pipelines**: one for the background
(sky/earth fill) and one for line primitives (horizon, pitch ladder, roll arc, symbols).

- Source: `app/rendering/horizon_renderer.cpp` (BG pipeline + LINE pipeline)  
- Acceptance: Renderer Init creates exactly two `VkPipeline` handles, confirmed by
  code inspection.  
- ASIL: `-`

---

**SRS-HOR-005**  
The horizon renderer's `RecordFrame` method shall not be called concurrently from
multiple threads; single-threaded execution is the design assumption.

- Source: `app/rendering/horizon_renderer.hpp` (`@warning` comment), MISRA rule  
- Acceptance: Thread-safety contract is documented and enforced by a static analysis
  rule; no mutex is required in the current design.  
- ASIL: D

---

### 3.11 Frame Telemetry

---

**SRS-TEL-001**  
The software shall report frame metrics (frame number, render duration, present
timestamp) through the `IFrameReport` interface after every frame; it shall not
call any blocking operation or allocate memory within `OnFrameComplete`.

- Source: `engine/data/i_frame_report.hpp`, DO-178C §12 (no allocation in task context)  
- Acceptance: `OnFrameComplete` returns within 10 µs on the reference target;
  verified by deadline-monitoring in the test harness.  
- ASIL: D

---

**SRS-TEL-002**  
The `ConsoleFrameReport` shall log one telemetry line per 60 frames in Debug builds
and shall compile to a no-op in Release builds.

- Source: `app/demo/console_frame_report.hpp`  
- Acceptance: Release binary contains zero logging instructions related to frame
  telemetry, confirmed by binary symbol inspection.  
- ASIL: `-`

---

### 3.12 Memory and Resource Management

---

**SRS-MEM-001**  
The software shall perform no dynamic memory allocation (heap allocation) on the
render-critical path after system initialisation is complete.

- Source: README "No dynamic allocation after init", ISO 26262 Part 6 §8.4.5,
  DO-178C §12  
- Acceptance: `valgrind --tool=massif` profile shows flat heap from end of
  Init phase through end of a 10 000-frame soak test.  
- ASIL: D

---

**SRS-MEM-002**  
All engine components that require custom allocation shall accept an `IMemoryAllocator`
interface injection; components shall not call `new`, `delete`, `malloc`, or `free`
directly.

- Source: `engine/core/i_memory_allocator.hpp`  
- Acceptance: Static analysis (clang-tidy rule `cppcoreguidelines-no-malloc`)
  reports no direct heap calls in engine source files.  
- ASIL: D

---

### 3.13 Logging

---

**SRS-LOG-001**  
All logging calls shall be conditionally compiled: present only in Debug builds
(`VKSC_ENABLE_LOGGING` defined) and compiled to empty inline functions in Release
builds.

- Source: `engine/core/log.hpp`, README "All logging eliminated at compile time in Release"  
- Acceptance: Preprocessor-stripped Release binary contains no string literals from
  log messages; confirmed by `strings` inspection of the Release binary.  
- ASIL: D

---

**SRS-LOG-002**  
Log output shall be written exclusively via `fputs()` to `stdout`; no `printf`,
`std::cout`, formatted I/O functions, or OS-specific logging APIs are permitted.

- Source: `engine/core/log.cpp`, MISRA C++:2023 rule compliance  
- Acceptance: Static analysis confirms only `fputs` is used; MISRA deviation for
  discarded return value is documented with `MISRA_DEVIATION` macro.  
- ASIL: `-`

---

### 3.14 Safety and Coding Standards

---

**SRS-SAFE-001**  
The software shall comply with **MISRA C++:2023** coding rules enforced via
`clang-tidy` with the project `.clang-tidy` configuration file; all deviations
shall be documented using the `MISRA_DEVIATION(rule, reason)` macro.

- Source: README "MISRA C++:2023 enforced", `engine/core/safety_macros.hpp`, ISO 26262 Part 6  
- Acceptance: `clang-tidy` run on the full source tree reports zero unresolved
  violations; every `MISRA_DEVIATION` annotation is logged in the deviation register.  
- ASIL: D

---

**SRS-SAFE-002**  
The software shall be compiled with C++ exceptions **disabled** (`-fno-exceptions` /
`/EHs-c-`); no `try`, `throw`, or `catch` statement shall appear in the source code.

- Source: `CMakeLists.txt`, ISO 26262 Part 6 (deterministic control flow requirement)  
- Acceptance: Compiler flags are verified in CI; static analysis confirms absence of
  exception keywords.  
- ASIL: D

---

**SRS-SAFE-003**  
The software shall be compiled with RTTI **disabled** (`-fno-rtti` / `/GR-`); no
`dynamic_cast` or `typeid` shall appear in the source code.

- Source: `CMakeLists.txt`, MISRA C++:2023 rule A18-1-1  
- Acceptance: Compiler flags verified; static analysis confirms absence of
  `dynamic_cast` and `typeid`.  
- ASIL: D

---

**SRS-SAFE-004**  
All error returns from the engine shall use the `Result` enumeration; no function
shall return raw `VkResult` to the application layer.

- Source: `engine/core/result.hpp`, ISO 26262 requirement for unambiguous error codes  
- Acceptance: All public engine API functions return `Result`; no `VkResult` appears
  in any public header.  
- ASIL: D

---

### 3.15 Build and Toolchain

---

**SRS-BUILD-001**  
The software shall build with **CMake ≥ 3.22**, **Clang / clang-cl ≥ 16**, and
**VulkanSC SDK 1.0.21** on Windows; the build shall not require any other dependencies.

- Source: README prerequisites  
- Acceptance: Clean checkout builds successfully on a Windows machine with only
  the listed tools installed.  
- ASIL: `-`

---

**SRS-BUILD-002**  
The build system shall export `compile_commands.json` to enable static analysis
tooling (clang-tidy, IDE indexers) without any additional configuration.

- Source: `CMakeLists.txt` (`CMAKE_EXPORT_COMPILE_COMMANDS ON`)  
- Acceptance: `compile_commands.json` is generated in the build directory after
  CMake configure; clang-tidy can be invoked on all sources using it.  
- ASIL: `-`

---

## 4. Requirements Summary

| Category       | Count | ASIL-D | Non-safety |
|----------------|-------|--------|------------|
| Initialisation | 5     | 5      | 0          |
| WSI / Output   | 3     | 2      | 1          |
| Swapchain      | 3     | 3      | 0          |
| Frame Sync     | 3     | 3      | 0          |
| Pipeline Cache | 2     | 1      | 1          |
| Vertex Buffer  | 2     | 2      | 0          |
| Command Pool   | 2     | 1      | 1          |
| Renderer Iface | 2     | 2      | 0          |
| Attitude Data  | 3     | 3      | 0          |
| Horizon Render | 5     | 4      | 1          |
| Frame Telemetry| 2     | 1      | 1          |
| Memory         | 2     | 2      | 0          |
| Logging        | 2     | 1      | 1          |
| Safety / Coding| 4     | 4      | 0          |
| Build          | 2     | 0      | 2          |
| **Total**      | **42**| **34** | **8**      |

---

## 5. Open Issues / TBDs

| ID     | Issue                                                           | Owner  |
|--------|-----------------------------------------------------------------|--------|
| TBD-01 | Heading display requirements not yet defined (no HOR symbology) | SWE.1  |
| TBD-02 | Memory allocator injection pattern not yet used in all components | SWE.3  |
| TBD-03 | Pixel-comparison reference screenshots for SRS-HOR-001 not yet created | SWE.4  |
| TBD-04 | DO-178C DAL-A formal verification strategy (MC/DC coverage) TBD | Safety |
| TBD-05 | Multi-display / multi-output support requirements deferred      | Future |
| TBD-06 | AR overlay compositing protocol (eArOverlay) not yet specified  | Future |

---

## 6. Traceability to Standards

| Requirement(s)               | Standard / Reference                              |
|------------------------------|---------------------------------------------------|
| SRS-INIT-001..005            | ISO 26262:2018 Part 6 §8.4.4, §8.4.5             |
| SRS-INIT-002, SRS-PIPE-001   | Vulkan SC Specification §2.6, §10.7               |
| SRS-SYNC-001..003            | ISO 26262:2018 Part 6 §8.4.5 (race-free access)  |
| SRS-ATT-002                  | ISO 26262:2018 Part 8 §6.5 (safe-state on failure)|
| SRS-ATT-003                  | ARINC 429, DO-229E (attitude sign convention)     |
| SRS-HOR-001                  | ARINC 661, SAE AS8034B (PFD symbology)            |
| SRS-MEM-001..002, SRS-TEL-001| DO-178C §12 (no dynamic memory in task context)   |
| SRS-SAFE-001                 | MISRA C++:2023, ISO 26262:2018 Part 6 Table 1     |
| SRS-SAFE-002..003            | ISO 26262:2018 Part 6 §8.4.5 (deterministic flow) |
| SRS-PIPE-001                 | DO-178C §12 (no runtime compilation)              |
| SRS-LOG-001                  | ISO 26262:2018 Part 6 (no production log overhead)|

---

*End of SOT-SRS-001 v0.1.0 — DRAFT*
