# Software Architecture Document (SAD)  
## Soteria v1.0 – Safety-Critical Vulkan SC Rendering Engine (ASIL-D)

---

## Architecture Overview

The Soteria rendering engine uses a safety-critical, hierarchical architecture. It is focused on meeting ISO 26262 ASIL-D requirements while adhering to Vulkan SC static allocation and deterministic constraints. The architecture is modular, ensuring clear allocation of safety requirements and traceability. Key design constraints arise from functional-safety principles including non-reliance on dynamic memory allocation, conservative resource control, and freedom from interference (FFI) between critical and non-critical system components.

The **core design drivers** for Soteria include:
- *Functional safety*: All components are aligned with ISO 26262 ASIL-D requirements. The architecture enforces determinism and static configurations (e.g., Vulkan SC preallocation).
- *Scalability*: The software interfaces are designed for reusable components to maximize modularity across safety-related and non-critical rendering infrastructures.
- *Integration simplicity*: Interfaces abstract the Vulkan SC mechanics, acting as stable APIs in avionics or automotive contexts.
- Components supporting real-time 2D/3D graphics must retain deterministic frame composition via precompiled command operations, pipeline cache optimizations, and static swapchain allocations controlled via `SwapchainSc`.

Critical interfaces cleanly separate concerns such as rendering pipelines (`IFrameRenderer`), input data (`IAttitudeSource`), and output surfaces (`IRenderOutput`). The `VkscContext` centralizes Vulkan device initialization and management, while coordination with `FrameSync` provides deterministic synchronization for rendering and presentation operations.

---

## Component Diagram

```uml_component
@startuml
package "Engine Core" {
  [VkscContext] -- [IMemoryAllocator]
  [VkscContext] -- [Result]
}

package "Engine Data" {
  [IAttitudeSource] --> [AttitudeData]
  [IFrameReport] --> [FrameMetrics]
}

package "Rendering Pipeline" {
  [IFrameRenderer] -- [IAttitudeSource]
  [IFrameRenderer] --> [PipelineCacheSc]
  [IFrameRenderer] --> [VertexBuffer]
  [IFrameRenderer] --> [CommandPool]
}

package "Window System Integration (WSI)" {
  [IRenderOutput] --> [SwapchainSc]
  [IRenderOutput] --> [DisplayOutput]
}
@enduml
```

---

## System State Machine

```uml_state
@startuml
[*] --> Init : Engine boots
Init : Vulkan SC setup
Init --> Running : Device ready
Running : Nominal frame execution
Running --> SafeState : Error detected
SafeState --> Shutdown : Cleaned up
Running --> Shutdown : Exit request
SafeState : Render-stop error handling
Shutdown --> [*] : Safe termination
@enduml
```

---

## Key Interaction Sequences

### Initialization Workflow
```uml_sequence
@startuml
actor System
System -> VkscContext: Init(VkscContextConfig)
VkscContext -> PipelineCacheSc: PrepareCaches()
PipelineCacheSc -> VkDevice: CreatePipelineCache
VkscContext <-- System: Result::kOk
@enduml
```

### Main Loop Rendering
```uml_sequence
@startuml
System -> IAttitudeSource: GetAttitude()
System -> FrameSync: WaitAndReset()
System -> IFrameRenderer: RecordFrame()
IFrameRenderer -> VkCommandBuffer: Render commands
IFrameRenderer <-- System: CommandBuffer
@enduml
```

---

## Requirements Allocation Matrix

| ID             | Requirement/Artifact                                   | Target Component       | Notes                                        |
|-----------------|-------------------------------------------------------|-------------------------|---------------------------------------------|
| `SRS-INIT-001` | Vulkan Initialization                                  | `VkscContext`          | Covers instance, device, and resource init. |
| `SRS-INIT-002` | Upfront Resource Reservation                           | `VkscContext`          | Alignment with Vulkan SC preallocation.     |
| `SRS-WSI-001`  | Support output modes (`eDirectDisplay` etc.)           | `DisplayOutput`        | Abstracts WSI-surfaces.                     |
| `SRS-WSI-003`  | Compile/link WSI backend configurability               | `IRenderOutput`        | Backend-interface decoupling.              |
| `SRS-SWP-001`  | Create static swapchain (3-image limit enforced)       | `SwapchainSc`          | Prevents violations of queue depth contraint.|

---

## Interface Definitions

| Interface             | API Details                       | Component              | Notes                                  |
|-----------------------|------------------------------------|------------------------|----------------------------------------|
| `VkscContext`         | `Init`, `Shutdown`, `Instance`    | Core                  | Encapsulates all Vulkan handles.      |
| `IAttitudeSource`     | `GetAttitude` -> AttitudeData     | Data/Telemetry        | Provides unlinked attitude interface. |
| `IRenderOutput`       | `Surface`, `Init`, `Shutdown`     | WSI                   | Unified access to WSI layers.         |
| `IFrameRenderer`      | `RecordFrame` -> CommandBuffer    | Rendering             | Encapsulates pipeline-level graphics. |

---

## Safety Architecture and ASIL Allocation

This architecture ensures full adherence to ASIL-D principles set out in ISO 26262. Key strategies utilized:
1. **ASIL-D Decomposition**: Components like `VkscContext`, `FrameSync`, and `PipelineCacheSc` hold safety-critical roles with no tolerance for error states. Less critical modules (`IRenderOutput`) achieve functional independence by using locked precompiled contracts without exceeding scheduling requirements.
2. **Freedom From Interference**: Enforced via strict data ownership. Rendering’s swapchain (`SwapchainSc`) cannot block telemetry methods (`IFrameReport`) as their responsibilities are fully partitioned within logical design nodes.
3. **Statistical Process Capability**: Reliability metrics evaluate Overall Failure Plan Mean (SPFM exceeds 95%) with Latent Failure Handling toward Self-Test-reports =+ logs in simulated versions.

---

## Refactoring Assessment

An architectural review of this iteration identifies several areas for long-term improvement:

1. **Abstract device selection** — `VkscContext` currently performs device selection inline. Extracting this into a dedicated `IDeviceSelector` interface would simplify unit testing and support multi-GPU configurations without engine changes.
2. **Resource reservation helper** — `VkscContextConfig::resources` is caller-filled today. A factory function `MakeReservationForRenderPipeline(imageCount, commandBuffers, ...)` would reduce integration errors and enforce the upfront-allocation contract.
3. **Default initialisation** — Default-constructed `VkCommandPoolMemoryReservationCreateInfo` fields are repeated across modules. A shared builder pattern would reduce duplication and ensure consistent reservation accounting.
4. **Introspection APIs** — `FrameSync` and `CommandPool` expose only handle accessors. Adding `IsInitialised()` to each component would allow uniform state queries and improve integration testability.

These refinements are non-breaking and can be introduced incrementally without impacting the current ASIL-D compliance posture.