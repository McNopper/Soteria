# Interface Design Document (IDD)

**Document ID:** SOT-IDD-001  
**Version:** 1.0  
**Project:** Soteria — Safety-Critical Vulkan SC Rendering Engine  
**ASPICE Process:** SWE.2 — Software Architectural Design  
**Safety Level:** ISO 26262 ASIL-D

---

## 1. Purpose

This document defines the **public API contracts** for every software unit in the
Soteria engine.  It serves as the authoritative specification of:

- Function signatures and parameter types
- Pre-conditions and post-conditions
- Return values and error semantics
- Ownership and lifetime rules

All contracts are verified by unit, integration, and qualification tests.
Doxygen `@satisfies` tags in source headers cross-reference back to this document.

---

## 2. Conventions

- `[[nodiscard]]` is applied to every fallible function; callers must check the result.
- Failure codes are **negative** integers; `kOk == 0`.
- All methods are `noexcept`; exceptions are globally disabled (`-fno-exceptions`).
- Every class is **non-copyable and non-movable** unless noted otherwise.
- `Shutdown()` is always safe to call on an uninitialised object (no-op).

---

## 3. `engine::Result` — `engine/core/result.hpp`

### 3.1 Type

```cpp
enum class Result : int32_t { kOk = 0, kError = -1, kInvalidArgument = -2, ... };
```

### 3.2 Helper Functions

| Function | Signature | Contract |
|----------|-----------|---------|
| `IsOk` | `[[nodiscard]] constexpr bool IsOk(Result r) noexcept` | Returns `true` iff `r == kOk`. |
| `ResultToString` | `[[nodiscard]] constexpr const char* ResultToString(Result r) noexcept` | Returns a string literal; lifetime is program lifetime. Returns `"Unknown"` for unknown values. |

### 3.3 Error Code Table

| Code | Value | Meaning |
|------|-------|---------|
| `kOk` | 0 | Operation succeeded. |
| `kError` | -1 | Generic unclassified error. |
| `kInvalidArgument` | -2 | A required argument was null or out of range. |
| `kNotFound` | -3 | Required resource (device, queue) not found. |
| `kAlreadyInitialised` | -21 | `Init()` called on an already-initialised object. |
| `kVkscInstanceFailed` | -4 | `vkCreateInstance` returned non-VK_SUCCESS. |
| `kVkscDeviceFailed` | -5 | `vkCreateDevice` returned non-VK_SUCCESS. |
| `kVkscEnumerateFailed` | -6 | `vkEnumeratePhysicalDevices` failed or found zero devices. |
| `kVkscCacheFailed` | -7 | `vkCreatePipelineCache` failed. |
| `kVkscSurfaceFailed` | -8 | Surface creation failed. |
| `kVkscNoDisplay` | -9 | No physical display, mode, or plane found. |
| `kVkscSwapchainFailed` | -10 | `vkCreateSwapchainKHR` failed. |
| `kVkscRenderPassFailed` | -11 | `vkCreateRenderPass` failed. |
| `kVkscPipelineLayoutFailed` | -12 | `vkCreatePipelineLayout` failed. |
| `kVkscPipelineFailed` | -13 | `vkCreateGraphicsPipelines` failed or UUID mismatch. |
| `kVkscBufferFailed` | -14 | `vkCreateBuffer` or `vkAllocateMemory` failed. |
| `kVkscCommandPoolFailed` | -15 | `vkCreateCommandPool` or `vkAllocateCommandBuffers` failed. |
| `kVkscSyncFailed` | -16 | `vkCreateSemaphore` or `vkCreateFence` failed. |
| `kVkscFramebufferFailed` | -17 | `vkCreateFramebuffer` failed. |
| `kVkscAcquireFailed` | -18 | `vkAcquireNextImageKHR` returned unrecoverable error. |
| `kVkscPresentFailed` | -19 | `vkQueuePresentKHR` returned unrecoverable error. |
| `kVkscSurfaceLost` | -20 | `VK_ERROR_SURFACE_LOST_KHR` detected. |
| `kVkscTimeoutFailed` | -22 | `vkWaitForFences` timed out (possible GPU hang). |

---

## 4. `engine::VkscContext` — `engine/core/vksc_context.hpp`

### 4.1 Purpose

Manages the Vulkan SC instance, selected physical device, and logical device.
One instance per safety domain.

### 4.2 Configuration — `VkscContextConfig`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `appName` | `const char*` | `nullptr` | Written into `VkApplicationInfo`. |
| `engineName` | `const char*` | `nullptr` | Written into `VkApplicationInfo`. |
| `maxPhysicalDevices` | `uint32_t` | `1` | Size of enumeration buffer (1–8). |
| `instanceExtensions[8]` | `const char*[]` | `{}` | Extensions to enable on the instance. |
| `instanceExtensionCount` | `uint32_t` | `0` | Number of populated `instanceExtensions` entries. |
| `deviceExtensions[8]` | `const char*[]` | `{}` | Extensions to enable on the device. |
| `deviceExtensionCount` | `uint32_t` | `0` | Number of populated `deviceExtensions` entries. |
| `resources` | `VkscResourceReservation` | `{}` | Static object allocation contract. |
| `pipelineCacheInfos` | `const VkPipelineCacheCreateInfo*` | `nullptr` | Cache create info array; `nullptr` when count == 0. |
| `pipelineCacheInfoCount` | `uint32_t` | `0` | Number of pipeline caches to reserve. |
| `pipelinePoolSizes` | `const VkPipelinePoolSize*` | `nullptr` | Pool size array; `nullptr` when count == 0. |
| `pipelinePoolSizeCount` | `uint32_t` | `0` | Number of pool-size entries. |
| `preferredDriverId` | `VkDriverId` | `VkDriverId{0}` | Preferred driver for two-pass device selection; 0 = any. |

### 4.3 Methods

| Method | Signature | Pre-conditions | Post-conditions | Returns |
|--------|-----------|---------------|-----------------|---------|
| `Init` | `[[nodiscard]] Result Init(const VkscContextConfig& config) noexcept` | Object not yet initialised. | On `kOk`: `IsInitialised()==true`; all handles non-null. On failure: state unchanged. | `kOk`, `kAlreadyInitialised`, `kVkscInstanceFailed`, `kVkscEnumerateFailed`, `kNotFound`, `kVkscDeviceFailed`. |
| `Shutdown` | `void Shutdown() noexcept` | None. | All handles set to `VK_NULL_HANDLE`; `IsInitialised()==false`. | `void` |
| `Device` | `[[nodiscard]] VkDevice Device() const noexcept` | None. | — | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |
| `PhysicalDevice` | `[[nodiscard]] VkPhysicalDevice PhysicalDevice() const noexcept` | None. | — | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |
| `Instance` | `[[nodiscard]] VkInstance Instance() const noexcept` | None. | — | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |
| `GraphicsQueue` | `[[nodiscard]] VkQueue GraphicsQueue() const noexcept` | None. | — | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |
| `GraphicsQueueFamily` | `[[nodiscard]] uint32_t GraphicsQueueFamily() const noexcept` | None. | — | Valid index after `Init`, `0xFFFFFFFF` otherwise. |
| `IsInitialised` | `[[nodiscard]] bool IsInitialised() const noexcept` | None. | — | `true` iff `Init` returned `kOk` and `Shutdown` has not been called. |

---

## 5. `engine::rendering::CommandPool` — `engine/rendering/command_pool.hpp`

### 5.1 Purpose

Owns one `VkCommandPool` with upfront memory reservation.  All allocations are
from a fixed pool declared before device creation (`SRS-CMD-001`).

### 5.2 Methods

| Method | Signature | Pre-conditions | Returns |
|--------|-----------|---------------|---------|
| `Init` | `[[nodiscard]] Result Init(VkDevice device, uint32_t queueFamilyIndex, VkDeviceSize reservedSizeBytes, uint32_t maxCommandBuffers) noexcept` | `device != VK_NULL_HANDLE`. `reservedSizeBytes > 0`. `maxCommandBuffers > 0`. | `kOk`, `kInvalidArgument` (null device), `kVkscCommandPoolFailed`. |
| `Shutdown` | `void Shutdown(VkDevice device) noexcept` | None (safe uninit). | `void` |
| `AllocateBuffers` | `[[nodiscard]] Result AllocateBuffers(VkDevice device, uint32_t count, VkCommandBuffer* outBuffers) const noexcept` | Pool must be initialised. | `kOk`, `kInvalidArgument` (null device / zero count / null outBuffers), `kVkscCommandPoolFailed`. |
| `Handle` | `[[nodiscard]] VkCommandPool Handle() const noexcept` | None. | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |

### 5.3 Argument-Validation Rules

`AllocateBuffers` returns `kInvalidArgument` for any of:
- `device == VK_NULL_HANDLE`
- `count == 0`
- `outBuffers == nullptr`

---

## 6. `engine::rendering::FrameSync` — `engine/rendering/frame_sync.hpp`

### 6.1 Purpose

Creates and owns two `VkSemaphore` handles and one pre-signalled `VkFence`
per render frame.  Satisfies `SRS-SYNC-001`, `SRS-SYNC-002`, and `SRS-SYNC-003`.

### 6.2 Methods

| Method | Signature | Pre-conditions | Returns |
|--------|-----------|---------------|---------|
| `Init` | `[[nodiscard]] Result Init(VkDevice device) noexcept` | `device != VK_NULL_HANDLE`. | `kOk`, `kInvalidArgument`, `kVkscSyncFailed`. |
| `Shutdown` | `void Shutdown(VkDevice device) noexcept` | None (safe uninit). | `void` |
| `WaitAndReset` | `[[nodiscard]] Result WaitAndReset(VkDevice device) noexcept` | Pool initialised. | `kOk`, `kVkscTimeoutFailed` (5 s GPU timeout), `kError`. |
| `ImageAvailable` | `[[nodiscard]] VkSemaphore ImageAvailable() const noexcept` | None. | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |
| `RenderComplete` | `[[nodiscard]] VkSemaphore RenderComplete() const noexcept` | None. | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |
| `InFlight` | `[[nodiscard]] VkFence InFlight() const noexcept` | None. | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |

### 6.3 Timeout

The fence timeout is **5 000 000 000 ns (5 seconds)**.  On expiry,
`WaitAndReset` returns `Result::kVkscTimeoutFailed` without hanging.

---

## 7. `engine::rendering::PipelineCacheSc` — `engine/rendering/pipeline_cache.hpp`

### 7.1 Purpose

Wraps one `VkPipelineCache` created from a compile-time binary.  No filesystem
I/O occurs at runtime (`SRS-PIPE-001`).

### 7.2 Methods

| Method | Signature | Pre-conditions | Returns |
|--------|-----------|---------------|---------|
| `Init` | `[[nodiscard]] Result Init(VkDevice device, const uint8_t* data, uint32_t dataSize) noexcept` | `device != VK_NULL_HANDLE`. `data != nullptr`. `dataSize > 0`. | `kOk`, `kInvalidArgument`, `kVkscCacheFailed`. |
| `Shutdown` | `void Shutdown(VkDevice device) noexcept` | None (safe uninit). | `void` |
| `Handle` | `[[nodiscard]] VkPipelineCache Handle() const noexcept` | None. | Valid handle after `Init`, `VK_NULL_HANDLE` otherwise. |

### 7.3 Argument-Validation Rules (`SRS-PIPE-002`)

`Init` returns `kInvalidArgument` for any of:
- `device == VK_NULL_HANDLE`
- `data == nullptr`
- `dataSize == 0`

---

## 8. `engine::rendering::IFrameRenderer` — `engine/rendering/i_frame_renderer.hpp`

### 8.1 Purpose

Abstract interface that every renderer component must implement (`SRS-REND-001`).

### 8.2 Methods

| Method | Signature | Contract |
|--------|-----------|---------|
| `Init` | `[[nodiscard]] virtual Result Init(const FrameRendererConfig& cfg) noexcept = 0` | Allocates all Vulkan resources. On failure, releases all already-created resources. |
| `Shutdown` | `virtual void Shutdown(VkDevice device) noexcept = 0` | Releases all resources; must be idempotent. |
| `RecordFrame` | `[[nodiscard]] virtual VkCommandBuffer RecordFrame(uint32_t imageIndex, const data::AttitudeData& attitude) noexcept = 0` | Records and ends one frame; returns a ready-to-submit `VkCommandBuffer`. Returns `VK_NULL_HANDLE` on error. No GPU submissions or present calls allowed (`SRS-REND-002`). |

---

## 9. `engine::data::IAttitudeSource` — `engine/data/i_attitude_source.hpp`

### 9.1 Purpose

Abstract interface for attitude data acquisition.  Decouples the engine from
hardware or simulation sources (`SRS-ATT-001`).

### 9.2 `AttitudeData` Structure

| Field | Type | Convention |
|-------|------|------------|
| `rollDeg` | `float` | Right-wing-down positive; range ±180°. |
| `pitchDeg` | `float` | Nose-up positive; range ±90°. |
| `headingDeg` | `float` | Clockwise from north; range 0–360°. |
| `airspeedKt` | `float` | Calibrated airspeed in knots (≥ 0). |
| `altitudeFt` | `float` | Barometric altitude in feet. |
| `valid` | `bool` | `false` when sensor data is unavailable; renderer must show failure symbology (`SRS-ATT-002`). |

### 9.3 Methods

| Method | Signature | Contract |
|--------|-----------|---------|
| `GetAttitude` | `[[nodiscard]] virtual AttitudeData GetAttitude() const noexcept = 0` | Returns the latest attitude sample; must not allocate or block. |

---

## 10. `engine::data::IFrameReport` — `engine/data/i_frame_report.hpp`

### 10.1 Purpose

Abstract interface for frame telemetry reporting (`SRS-TEL-001`).

### 10.2 `FrameMetrics` Structure

| Field | Type | Description |
|-------|------|-------------|
| `frameNumber` | `uint64_t` | Monotonically increasing frame counter (starts at 0). |
| `renderTimeMs` | `float` | GPU submit wall time in milliseconds. |
| `presentTimeMs` | `float` | `vkQueuePresentKHR` wall time in milliseconds. |
| `displayedRollDeg` | `float` | Roll angle actually rendered. |
| `displayedPitchDeg` | `float` | Pitch angle actually rendered. |
| `framePresented` | `bool` | `false` if present was skipped due to an error. |

### 10.3 Methods

| Method | Signature | Contract |
|--------|-----------|---------|
| `OnFrameComplete` | `virtual void OnFrameComplete(const FrameMetrics&) noexcept = 0` | Called after every frame; must not block or allocate memory (`SRS-TEL-001`). |

---

## 11. `engine::wsi::IRenderOutput` — `engine/wsi/i_render_output.hpp`

### 11.1 Purpose

Abstract interface for WSI output surfaces.  Decouples the engine from any
specific display backend (`SRS-WSI-003`).

### 11.2 Methods

| Method | Signature | Contract |
|--------|-----------|---------|
| `Init` | `[[nodiscard]] virtual Result Init(VkInstance instance, VkPhysicalDevice pd) noexcept = 0` | Creates the `VkSurfaceKHR`; returns `kOk` or a `kVksc*` error. |
| `Shutdown` | `virtual void Shutdown() noexcept = 0` | Destroys the surface; safe to call before or after `Init`. |
| `Surface` | `[[nodiscard]] virtual VkSurfaceKHR Surface() const noexcept = 0` | Returns the surface handle; `VK_NULL_HANDLE` before `Init`. |
| `ColorFormat` | `[[nodiscard]] virtual VkFormat ColorFormat() const noexcept = 0` | The pixel format the swapchain must use for this backend (`SRS-SWP-002`). |
| `Mode` | `[[nodiscard]] virtual RenderOutputMode Mode() const noexcept = 0` | Output mode this backend was constructed for; immutable after `Init`. |
| `Width` | `[[nodiscard]] virtual uint32_t Width() const noexcept = 0` | Render target width in pixels. |
| `Height` | `[[nodiscard]] virtual uint32_t Height() const noexcept = 0` | Render target height in pixels. |

---

## 12. `engine::rendering::SwapchainSc` — `engine/rendering/swapchain.hpp`

### 12.1 Purpose

Manages the `VkSwapchainKHR`, images, and one `VkImageView` per image.
All arrays are fixed-size (`kMaxImages = 3`); no dynamic allocation (`SRS-SWP-001`).

### 12.2 Configuration — `SwapchainSc::Config`

| Field | Type | Description |
|-------|------|-------------|
| `physDevice` | `VkPhysicalDevice` | For surface capability queries. |
| `device` | `VkDevice` | Logical device. |
| `surface` | `VkSurfaceKHR` | Surface to create the swapchain against. |
| `queueFamilyIndex` | `uint32_t` | Graphics queue family. |
| `requiredFormat` | `VkFormat` | Must match `IRenderOutput::ColorFormat()` (`SRS-SWP-002`). |
| `preferredWidth` | `uint32_t` | Desired width; `0` = use surface `currentExtent`. |
| `preferredHeight` | `uint32_t` | Desired height; `0` = use surface `currentExtent`. |

### 12.3 Methods

| Method | Signature | Pre-conditions | Returns |
|--------|-----------|---------------|---------|
| `Init` | `[[nodiscard]] Result Init(const Config& cfg) noexcept` | All `cfg` handles non-null. | `kOk`, `kVkscSwapchainFailed`. |
| `Shutdown` | `void Shutdown(VkDevice device) noexcept` | None (safe uninit). | `void` |
| `Handle` | `[[nodiscard]] VkSwapchainKHR Handle() const noexcept` | None. | Valid handle or `VK_NULL_HANDLE`. |
| `Format` | `[[nodiscard]] VkFormat Format() const noexcept` | None. | Actual swapchain format. |
| `Extent` | `[[nodiscard]] VkExtent2D Extent() const noexcept` | None. | Pixel dimensions. |
| `ImageCount` | `[[nodiscard]] uint32_t ImageCount() const noexcept` | None. | Number of swapchain images (1–3). |
| `ImageView` | `[[nodiscard]] VkImageView ImageView(uint32_t index) const noexcept` | `index < ImageCount()`. | Image view handle. |
| `Image` | `[[nodiscard]] VkImage Image(uint32_t index) const noexcept` | `index < ImageCount()`. | Swapchain image handle. |

---

## 13. `engine::rendering::VertexBuffer` — `engine/rendering/vertex_buffer.hpp`

### 13.1 Purpose

Allocates a host-visible, host-coherent `VkBuffer` with persistent mapping.
The buffer is mapped once at `Init` and remains mapped until `Shutdown`
(`SRS-VBF-001`, `SRS-VBF-002`).

### 13.2 Methods

| Method | Signature | Pre-conditions | Returns |
|--------|-----------|---------------|---------|
| `Init` | `[[nodiscard]] Result Init(VkDevice device, VkPhysicalDevice physDevice, uint32_t sizeBytes) noexcept` | `device` and `physDevice` non-null; `sizeBytes > 0`. | `kOk`, `kInvalidArgument`, `kVkscBufferFailed`. |
| `Shutdown` | `void Shutdown(VkDevice device) noexcept` | None (safe uninit). | `void` |
| `Buffer` | `[[nodiscard]] VkBuffer Buffer() const noexcept` | None. | `VkBuffer` handle or `VK_NULL_HANDLE`. |
| `MappedPtr` | `[[nodiscard]] void* MappedPtr() const noexcept` | Between `Init` and `Shutdown`. | Persistent write pointer. |
| `MappedBytes` | `[[nodiscard]] uint8_t* MappedBytes() const noexcept` | Between `Init` and `Shutdown`. | Byte-typed write pointer. |
| `SizeBytes` | `[[nodiscard]] uint32_t SizeBytes() const noexcept` | None. | Allocated size in bytes. |

---

## 14. `engine::wsi::DisplayOutput` — `engine/wsi/display_output.hpp`

### 14.1 Purpose

Concrete `IRenderOutput` backend for opaque RGB direct-to-display output.
Uses `VK_KHR_display` to create a `VkDisplayPlaneSurfaceKHR` (`SRS-WSI-001`,
`SRS-WSI-002`).

### 14.2 Behaviour

- Enumerates physical displays (up to `kMaxDisplays = 4`).
- Selects the first available display; chooses the mode with the largest visible area.
- Selects the first display plane that supports the chosen display.
- Fixed pixel format: `VK_FORMAT_B8G8R8A8_UNORM`.
- Fixed mode: `RenderOutputMode::eDirectDisplay`.

### 14.3 Methods

| Method | Signature | Returns |
|--------|-----------|---------|
| `Init` | `[[nodiscard]] Result Init(VkInstance, VkPhysicalDevice) noexcept override` | `kOk`, `kVkscNoDisplay` (no display found), `kVkscSurfaceFailed`. |
| `Shutdown` | `void Shutdown() noexcept override` | `void`; safe to call before `Init`. |
| `Surface` | `[[nodiscard]] VkSurfaceKHR Surface() const noexcept override` | `VkDisplayPlaneSurfaceKHR` or `VK_NULL_HANDLE`. |
| `ColorFormat` | `[[nodiscard]] VkFormat ColorFormat() const noexcept override` | `VK_FORMAT_B8G8R8A8_UNORM` (constant). |
| `Mode` | `[[nodiscard]] RenderOutputMode Mode() const noexcept override` | `eDirectDisplay` (constant). |
| `Width` | `[[nodiscard]] uint32_t Width() const noexcept override` | Display width in pixels (available after `Init`). |
| `Height` | `[[nodiscard]] uint32_t Height() const noexcept override` | Display height in pixels (available after `Init`). |

---

## 15. Traceability

| IDD Section | SRS ID        | Header file                                    |
|-------------|---------------|------------------------------------------------|
| §3          | —             | `engine/core/result.hpp`                       |
| §4          | SRS-INIT-001…005 | `engine/core/vksc_context.hpp`              |
| §5          | SRS-CMD-001, SRS-CMD-002 | `engine/rendering/command_pool.hpp` |
| §6          | SRS-SYNC-001…003 | `engine/rendering/frame_sync.hpp`           |
| §7          | SRS-PIPE-001, SRS-PIPE-002 | `engine/rendering/pipeline_cache.hpp` |
| §8          | SRS-REND-001, SRS-REND-002 | `engine/rendering/i_frame_renderer.hpp` |
| §9          | SRS-ATT-001…003 | `engine/data/i_attitude_source.hpp`          |
| §10         | SRS-TEL-001, SRS-TEL-002 | `engine/data/i_frame_report.hpp`        |
| §11         | SRS-WSI-001…003 | `engine/wsi/i_render_output.hpp`             |
| §12         | SRS-SWP-001…003 | `engine/rendering/swapchain.hpp`             |
| §13         | SRS-VBF-001, SRS-VBF-002 | `engine/rendering/vertex_buffer.hpp`    |
| §14         | SRS-WSI-001, SRS-WSI-002 | `engine/wsi/display_output.hpp`         |
