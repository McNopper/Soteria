# Detailed Design Document (DDD)
## Soteria v1.0 — Safety-Critical Vulkan SC Rendering Engine (ASIL-D)

---
## Detailed Design Overview

Soteria employs a safety-critical, modular architecture aimed at meeting the highest functional safety requirements (ISO 26262 ASIL-D). The architecture enforces deterministic execution, complete traceability, and static resource allocation, all of which align with the underlying principles of Vulkan SC. Key components are mapped to software units that specialize in specific subsystems, such as resource reservation, rendering, synchronization, and data flow abstraction.

The system architecture allocates core responsibilities as follows:
- **Core Components**: These encapsulate the fundamental Vulkan Safety Critical (SC) functionality such as device context lifecycle management (`VkscContext`), memory allocation interfaces (`IMemoryAllocator`), and error handling (`Result`).
- **Rendering Pipeline**: Includes modules like `CommandPool`, `FrameSync`, `IFrameRenderer`, and `PipelineCacheSc`, which implement critical rendering operations. These units interact through clearly defined contracts to ensure modularity and maintainability.
- **Data Components**: Abstraction interfaces like `IAttitudeSource` (for real-time sensor data) and `IFrameReport` (for frame telemetry output) act as bridges between data sources, telemetry sinks, and the rendering components.
- **Window System Integration (WSI)**: Interfaces (`IRenderOutput`, `SwapchainSc`, etc.) abstract Vulkan WSI objects to enable flexibility in outputs, such as AR overlays or display panels.

Error handling is designed to propagate unambiguous `Result` status codes, avoiding both silent failures and exceptions. MISRA C++:2023 rules are enforced to ensure compliance with safety-critical coding standards.

---

## Class / Module Diagram

```uml_class
@startuml
class VkscContext {
    - VkInstance m_instance
    - VkPhysicalDevice m_physicalDevice
    - VkDevice m_device
    - VkQueue m_graphicsQueue
    + Init(VkscContextConfig config): Result
    + Shutdown(): void
    + IsInitialised(): bool
}

class CommandPool {
    - VkCommandPool m_pool
    - bool m_initialised
    + Init(VkDevice device, uint32_t queueFamilyIndex, VkDeviceSize reservedSize, uint32_t maxBuffers): Result
    + AllocateBuffers(VkDevice device, uint32_t count, VkCommandBuffer* outBuffers): Result
    + Shutdown(VkDevice device): void
}

class FrameSync {
    - VkSemaphore m_imageAvailable
    - VkSemaphore m_renderComplete
    - VkFence m_inFlight
    + Init(VkDevice device): Result
    + WaitAndReset(VkDevice device): Result
    + Shutdown(VkDevice device): void
}

class PipelineCacheSc {
    - VkPipelineCache m_cache
    + Init(VkDevice device, const uint8_t* data, uint32_t size): Result
    + Shutdown(VkDevice device): void
}

class IFrameRenderer {
    + Init(FrameRendererConfig config): Result
    + RecordFrame(uint32_t imageIndex, AttitudeData attitude): VkCommandBuffer
    + Shutdown(VkDevice device): void
}

VkscContext -- CommandPool
CommandPool -- FrameSync
VkscContext -- PipelineCacheSc
PipelineCacheSc ..> IFrameRenderer
@enduml
```

---

## Unit State Machines

### VkscContext State Machine
```uml_state
@startuml
[*] --> Uninitialised
Uninitialised --> Initialised : Init(config)
Initialised --> Shutdown : Shutdown()
Shutdown --> [*]
@enduml
```

### FrameSync State Machine
```uml_state
@startuml
[*] --> Uninitialised
Uninitialised --> Ready : Init(device)
Ready --> Waiting : WaitAndReset(device)
Waiting --> Ready : GPU completion
Ready --> Shutdown : Shutdown(device)
Shutdown --> [*]
@enduml
```

---

## Unit Interaction Sequences

### Initialization Sequence
```uml_sequence
@startuml
System -> VkscContext: Init(config)
VkscContext -> PipelineCacheSc: Init(device, data, size)
PipelineCacheSc --> VkscContext: Result::kOk
VkscContext --> System: Result::kOk
@enduml
```

### Rendering Workflow
```uml_sequence
@startuml
System -> FrameSync: WaitAndReset()
FrameSync --> System: Result::kOk
System -> IFrameRenderer: RecordFrame(imageIndex, attitude)
IFrameRenderer --> System: VkCommandBuffer
@enduml
```

---

## Data Flow and Algorithms

The core data flow is determined by the Vulkan SC pipeline mechanics, designed to honor resource constraints and determinism. Critical algorithms include:

1. **Vulkan SC Initialization**:
   - The `VkscContext` encapsulates Vulkan SC initialization. It validates configuration parameters (`VkscContextConfig`) and ensures only the required extensions and object reservations are requested.
   - Error codes from all Vulkan SC calls (e.g., `vkCreateInstance`) are translated into `Result` values to maintain a consistent safety interface.

2. **Frame Rendering Algorithm**:
   - Per-frame rendering begins with `FrameSync::WaitAndReset` synchronizing the GPU.
   - The `IFrameRenderer` records render passes into preallocated command buffers managed by `CommandPool`. Attitude data (e.g., `rollDeg`) determines geometry and view transformations.

3. **Error Handling**:
   - Errors are propagated as `Result::kError` or Vulkan-specific enums (e.g., `kVkscInstanceFailed`), ensuring no exceptions or undefined behavior.

4. **Defensive Design**:
   - Every stateful function verifies preconditions (e.g., initialized contexts) at runtime using `assert` macros for debug builds.
   - Safety-critical parameters such as `maxBuffers` in `CommandPool` are capped to prevent runtime resource overflows.

---

## Requirements Traceability

| ID             | Requirement/Artifact                   | Target Component       | Notes                              |
|-----------------|---------------------------------------|-------------------------|-------------------------------------|
| `SRS-INIT-001` | Vulkan Initialization                 | `VkscContext`          | Verified in `Init` implementation. |
| `SRS-SWP-001`  | Swapchain with max three images        | `SwapchainSc`          | Ensures compliance with limits.    |
| `SRS-WSI-001`  | Support different output modes         | `IRenderOutput`        | Flexible WSI integration.          |
| `SRS-WSI-002`  | VK_KHR_display usage for direct output| `DisplayOutput`        | Verifies display creation paths.   |

---

## Unit Test Strategy

To ensure compliance with ASIL-D standards:
- **Coverage Targets**:
  - Statement and branch coverage exceeding 95%.
  - Modified condition-decision coverage (MC/DC) mandatory for all safety-relevant paths.
- **Framework**:
  - GoogleTest is used for unit test automation, augmented by Vulkan validation layers and manual reviews.
- **Stubbing**:
  - Vulkan SC calls (`vkCreateInstance`, etc.) are stubbed on non-SC hardware, enabling consistency across test environments.
- **Assertions**:
  - Every `[[nodiscard]]` result is asserted during qualification tests.

---

## Refactoring Assessment

A review of the current implementation has highlighted several areas for long-term refinement:
1. **Tight Coupling**:
   - `VkscContext` tightly integrates resource management. Abstract factories for device handles could simplify testing mocks.
2. **Simplification Opportunities**:
   - `IFrameRenderer` can encapsulate more logic, reducing the explicit handoff of Vulkan handles between layers.
3. **Code Duplication**:
   - Default initialization blocks (e.g., `VkPipelinePoolSize`) are repeated across modules.
4. **Modularization**:
   - Modules like `FrameSync` and `CommandPool` could expose richer introspection APIs to improve integration testing.

By applying these findings iteratively, Soteria's maintainability will improve while fulfilling ISO 26262's safety and verifiability demands.
