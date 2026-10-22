# Vulkan SC Safety-Critical Rendering Engine — Feature Research Report

**Project:** Soteria (Vulkan SC, ASIL-D / ISO 26262, MISRA C++:2023, avionics artificial-horizon demo)
**Method:** Online research against Khronos spec/registry/blogs, the Vulkan SC 1.0 spec HTML (downloaded and searched directly), VK-GL-CTS README, vendor product pages, and standards-industry literature. Every feature below carries source URL(s) and an applicability note: **ENGINE** (Soteria's layer), **DRIVER** (the Vulkan SC implementation / vendor stack), **APP** (the application using the engine), or **PLATFORM** (OS/RTOS/hardware).

---

## 1. Vulkan SC Specification Specifics (mandates / forbids vs. Vulkan)

Vulkan SC 1.0 is derived from Vulkan 1.2. Its changes are classified by the spec into three goals — **Deterministic Execution**, **Robustness**, **Simplification** — and every deviation is tagged with a change-justification ID (SCID-1…SCID-8: deterministic behavior, asynchronous-call parameter lifetime, no async callbacks, no garbage collection / avoid fragmentation, full testability, documented undefined behavior, unique runtime ID, code-complexity reduction). These SCIDs are the design grammar an engine layer should reuse when justifying its own API decisions.
Source: Vulkan SC 1.0 spec, §2.1 "Safety Critical Philosophy" — https://registry.khronos.org/VulkanSC/specs/1.0-extensions/html/vkspec.html
**Applies to:** ENGINE (mirror these justifications in Soteria's own design rationale).

### 1.1 API variant and version detection
Vulkan SC uses API *variant* 1 in the packed version number (`VKSC_API_VERSION_1_0`, `VK_API_VERSION_VARIANT()`), retroactively introduced in Vulkan 1.2.175. `vkEnumerateInstanceVersion` and `VkApplicationInfo::apiVersion` use it; a single implementation is *not* expected to support both Vulkan and Vulkan SC, enabling early detection of driver/application mismatch.
Source: https://www.khronos.org/blog/vulkan-sc-overview
**Applies to:** ENGINE — assert variant==1 and record driver vendor/device IDs + driver version at init (satisfies SCID-7 "unique runtime ID" and feeds certification configuration records).

### 1.2 Offline pipeline compilation (mandatory; no online compilation)
Vulkan SC does not support online pipeline compilation and has no `VkShaderModule` at all (vkCreateShaderModule/vkDestroyShaderModule removed, SCID-8). All pipelines are built offline by a vendor-specific **Pipeline Cache Compiler (PCC)** from (a) a JSON pipeline-state description (schema: https://github.com/KhronosGroup/VulkanSC-Docs/wiki/JSON-schema) and (b) SPIR-V modules, producing a pipeline-cache binary with a documented header (`VkPipelineCacheHeaderVersionSafetyCriticalOne`) and index entries (`VkPipelineCacheSafetyCriticalIndexEntry{pipelineIdentifier, pipelineMemorySize}`). At runtime, each pipeline create-info must chain `VkPipelineOfflineCreateInfo{pipelineIdentifier, poolEntrySize}`; failure yields `VK_ERROR_NO_PIPELINE_MATCH`. `VkPipelineCache` must be created with `VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT | VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT`, its contents must be identical to one of the caches declared at device creation, and pipeline state at create time must match the JSON given to the PCC (only shader-stage `module/pName/pSpecializationInfo` are irrelevant at runtime). `VkPipelineMatchControl` (e.g. `VK_PIPELINE_MATCH_CONTROL_APPLICATION_UUID_EXACT_MATCH`) controls matching strictness. Pipeline derivatives are removed (`basePipelineHandle` must be NULL, `basePipelineIndex` must be 0); `vkMergePipelineCaches`/`vkGetPipelineCacheData` are removed (SCID-1/8).
Sources: https://www.khronos.org/blog/vulkan-sc-overview ; Vulkan SC spec Appendix "Vulkan SC Deviations From Base Vulkan" ; https://docs.nvidia.com/jetson/archives/r35.1/DeveloperGuide/text/SD/GraphicsProgramming/VulkanSC.html
**Applies to:** ENGINE (consume/cache/validate binaries; match IDs; own the PCC input authoring workflow) + DRIVER (the PCC itself) + TOOLS (JSON generation layer can capture pipeline JSON from a running Vulkan prototype — see §5).

### 1.3 `VkDeviceObjectReservationCreateInfo` — static resource contract (mandatory)
`vkCreateDevice` **fails with `VK_ERROR_INITIALIZATION_FAILED`** unless the `pNext` chain includes both `VkDeviceObjectReservationCreateInfo` and `VkPhysicalDeviceVulkanSC10Features`. The reservation structure declares: the pipeline caches creatable on the device (`pPipelineCacheCreateInfos`), pipeline memory pool buckets (`pPipelinePoolSizes`), high-water-mark `*RequestCount` for ~30 object types (semaphores, command buffers, fences, memory, buffers, images, views, pipelines, descriptor sets/pools, framebuffers, render passes, swapchains, etc.), aggregate limits (`subpassDescriptionRequestCount`, `attachmentDescriptionRequestCount`, `descriptorSetLayoutBindingRequestCount/Limit`), and max property caps (`maxImageViewMipLevels`, `maxImageViewArrayLayers`, `maxOcclusionQueriesPerPool`, `maxImmutableSamplersPerDescriptorSetLayout`, …). Multiple instances may be chained (sums for counts, maxima for caps). Exceeding reservations is invalid usage (implementations *can* return `VK_ERROR_TOO_MANY_OBJECTS`). Incompatible cache data at device creation → `VK_ERROR_INVALID_PIPELINE_CACHE_DATA`.
Source: https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkDeviceObjectReservationCreateInfo.html
**Applies to:** ENGINE — this is Soteria's single most important contract: compute reservations from the declared scene/feature set at init; treat violations as design errors found in testing, never in production.

### 1.4 Pipeline pools and pool sizing
Pipeline memory is reserved at device creation as buckets of fixed-size entries (`VkPipelinePoolSize{poolEntrySize, poolEntryCount}`); a created pipeline occupies one entry of the bucket whose size ≥ its `pipelineMemorySize`; destroyed pipelines return their entry (only if `recyclePipelineMemory==VK_TRUE`, otherwise reuse may be deferred). The Khronos `pcinfo` tool (VulkanSC-pcutil) lists per-pipeline memory requirements and suggests bucket assignments. Fragmentation-free create/destroy cycles are possible inside reservations.
Sources: https://www.khronos.org/blog/vulkan-sc-overview ; https://github.com/KhronosGroup/VulkanSC-pcutil
**Applies to:** ENGINE — Soteria should derive `pPipelinePoolSizes` from `pcinfo` output as part of its build/CI tooling, and pin pipeline→bucket assignment.

### 1.5 Command pool memory reservation and deterministic command-buffer memory
Every command pool must be created with `VkCommandPoolMemoryReservationCreateInfo{commandPoolReservedSize, commandPoolMaxCommandBuffers}`; the buffer count is permanently charged against the device-wide `commandBufferRequestCount`. `vkFreeCommandBuffers` does **not** return memory to the pool — only `vkResetCommandPool` does. Command memory consumption is implementation-dependent but *deterministic* (same commands from same initial state → same consumption); `vkGetCommandPoolMemoryConsumption` measures it at development time or runtime, against `maxCommandBufferSize`/`maxCommandPoolCommandBuffers` limits. Capability properties gate: `commandPoolMultipleCommandBuffersRecording` (parallel recording from one pool), `commandPoolResetCommandBuffer` (vkResetCommandBuffer / begin from non-initial state), `commandBufferSimultaneousUse`. `vkDestroyCommandPool`, `vkTrimCommandPool`, and `VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT` are removed (SCID-4/8). Exhausting reserved pool memory raises `VK_FAULT_TYPE_COMMAND_BUFFER_FULL`.
Sources: Vulkan SC spec (command-buffer chapter + deviations) ; https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkPhysicalDeviceVulkanSC10Properties.html
**Applies to:** ENGINE — Soteria must size pools from measured consumption with margin, prefer pool-reset-per-frame patterns, and feature-test its code paths against property variations (see DevSim, §5).

### 1.6 Object lifetimes — what cannot be destroyed
Device memory (`vkFreeMemory` removed), swapchains (`vkDestroySwapchainKHR` removed), and pools (`vkDestroyCommandPool`/`vkDestroyDescriptorPool`/`vkDestroyQueryPool` removed) are never explicitly freed; their memory returns to the system at `vkDestroyDevice` only if `deviceDestroyFreesMemory==VK_TRUE`, else possibly not until process termination. Most other objects *can* be created/destroyed freely within reservation counts. `VkSwapchainCreateInfoKHR::oldSwapchain` must be NULL (no swapchain re-use chains). All `pAllocator` parameters must be NULL everywhere (application allocation callbacks removed, SCID-2/8). Sparse resources are entirely removed (all `vkQueueBindSparse`/sparse-residency features reported FALSE). Descriptor update templates removed. If `recycleDescriptorSetMemory==VK_FALSE`, freed descriptor-set memory is reusable only after `vkResetDescriptorPool`.
Source: Vulkan SC spec deviations appendix (verified verbatim) ; https://www.khronos.org/blog/vulkan-sc-overview
**Applies: to:** ENGINE — Soteria's "create everything at init, recycle via resets" pattern is exactly the intended idiom; deterministic shutdown means draining the GPU and destroying the instance/device, not chasing per-object deleters.

### 1.7 Fault handling (core Vulkan SC — *not* an extension)
Note on terminology: there is **no `VK_EXT_fault_handling`**; fault handling is core Vulkan SC 1.0 (`VKSC_VERSION_1_0`). Mechanism: register `VkFaultCallbackInfo` + `PFN_vkFaultCallbackFunction` at device creation (max `maxCallbackFaultCount` faults per callback), and/or poll `vkGetFaultData(device, VK_FAULT_QUERY_BEHAVIOR_GET_AND_CLEAR_ALL_FAULTS, &unrecorded, &count, VkFaultData[])` — internally synchronized (thread-safe), bounded by `maxQueryFaultCount`, with a `pUnrecordedFaults` overflow flag. `VkFaultData{faultLevel, faultType}`: levels `UNASSIGNED/WARNING/RECOVERABLE/CRITICAL`; types `INVALID/UNASSIGNED/IMPLEMENTATION/SYSTEM/PHYSICAL_DEVICE/COMMAND_BUFFER_FULL/INVALID_API_USAGE`; `pNext` allows vendor-specific fault detail (vendors are *encouraged* to extend and to document in their safety manuals). New result `VK_ERROR_VALIDATION_FAILED` may be returned by any command with a return value. Robustness in the Vulkan sense (`robustBufferAccess`) is unchanged from Vulkan 1.2 features; Vulkan SC's robustness additions are the fault model + deterministic reservations + `deviceNoDynamicHostAllocations` (if TRUE, `VK_ERROR_OUT_OF_HOST_MEMORY` may only be returned where explicitly allowed).
Sources: https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/vkGetFaultData.html ; …/VkFaultType.html ; https://www.khronos.org/blog/vulkan-sc-overview
**Applies to:** ENGINE — Soteria should register the callback *and* poll once per frame (defense in depth), map fault records onto its telemetry, and define per-type reactions (§7).

### 1.8 SEU robustness: `VK_MEMORY_HEAP_SEU_SAFE_BIT` and `VK_KHR_object_refresh`
Safety-critical platforms must contend with single-event upsets (bit flips). Vulkan SC adds a heap flag marking memory robust against SEUs, plus the only *added* Khronos extension, `VK_KHR_object_refresh` (optional device extension, ratified): `vkGetPhysicalDeviceRefreshableObjectTypesKHR` reports which object types hold implicit device-resident data, and `vkCmdRefreshObjectsKHR` re-loads that data from the application's SEU-safe copy (synchronized as a transfer write). The extension refreshes rather than validates, by design (reduces app error-handling complexity).
Sources: https://registry.khronos.org/VulkanSC/specs/1.0-extensions/html/vkspec.html#VK_KHR_object_refresh ; https://www.khronos.org/blog/vulkan-sc-overview
**Applies to:** ENGINE — own the refresh policy (periodic refresh commands in the frame schedule when SEU-safe heaps are absent); DRIVER provides the mechanism.

### 1.9 Device-lost semantics
`VK_ERROR_DEVICE_LOST` remains in Vulkan SC: a lost logical device cannot be reset to a non-lost state; loss may be logical-device-scoped or physical-device-scoped (physical loss typically indicates an implementation/host-connection problem). Fence/semaphore waits with a timeout must return in finite time even on a lost device; `vkQueueSubmit2KHR` must return `VK_ERROR_DEVICE_LOST` if it cannot guarantee its execution contract. The Vulkan SC Emulation ICD maps detected `VK_ERROR_DEVICE_LOST` to `VK_FAULT_TYPE_PHYSICAL_DEVICE` faults.
Sources: Vulkan SC spec ("Lost Device") ; https://www.rastergrid.com/blog/gpu-tech/2025/02/the-vulkan-sc-emulation-driver-stack/
**Applies to:** ENGINE — Soteria's existing GPU-hang timeout aligns with the spec's finite-time guarantee; the *policy* on device loss (safe state, degraded mode, restart escalation to the system health monitor) is an engine/app responsibility — the driver cannot recover for you.

### 1.10 WSI / display output
Desktop window-system extensions (Win32/X11 surfaces, etc.) are not part of Vulkan SC; the portable on-screen path is **`VK_KHR_display`** direct-to-display (used by NVIDIA's desktop Vulkan SC driver and emulated as Win32/X11 windows by the Emulation ICD, with `VKSC_EMULATION_DISPLAYS` / JSON display config). NVIDIA DRIVE/Jetson instead present via **OpenWFD through NvSci interop** (`VK_NV_external_memory_sci_buf`, `VK_NV_external_sci_sync2`). Swapchain creation is allowed; swapchain destruction is not (§1.6).
Sources: https://github.com/KhronosGroup/VulkanSC-Tools/blob/sc_main/cube-vksc/README.md ; https://developer.nvidia.com/vulkan-sc ; https://docs.nvidia.com/jetson/archives/r35.1/DeveloperGuide/text/SD/GraphicsProgramming/VulkanSC.html
**Applies to:** ENGINE (Soteria's VK_KHR_display path is the reference-blessed choice) + PLATFORM (real targets may substitute OpenWFD/NvSci — keep a presentation abstraction).

### 1.11 Feature deltas vs Vulkan 1.2
Made **optional**: shader atomic instructions, multiview, timeline semaphores, depth/stencil resolve (`supported*ResolveModes` may be NONE). Made **mandatory**: the Vulkan Memory Model (well-defined memory semantics). New SC-specific limits: `maxRenderPassSubpasses`, `maxRenderPassDependencies`, `maxSubpassInputAttachments`, `maxSubpassPreserveAttachments`, `maxFramebufferAttachments`, `maxDescriptorSetLayoutBindings`, `maxQueryFaultCount`, `maxCallbackFaultCount`, `maxCommandPoolCommandBuffers`, `maxCommandBufferSize`. Removed legacy aliases/deprecated enums; extensions promoted to core in 1.1/1.2 exist only in core form.
Source: https://www.khronos.org/blog/vulkan-sc-overview ; spec deviations appendix
**Applies to:** ENGINE — do not assume timeline semaphores; Soteria's fence+binary-semaphore design is the portable baseline. Query every optional feature before use.

### 1.12 Invariance / repeatability (Appendix H)
Vulkan is not pixel-exact across implementations, but per implementation the spec mandates repeatability: identical state + identical command sequence ⇒ identical result (Invariance Rules 1–7; relaxed in the presence of shader side effects such as image stores/atomics, which is also why shader atomics were made optional in SC). Rule 4: identical pipelines (same SPIR-V + same state) produce identical results.
Source: Vulkan SC spec, Appendix H "Invariance"
**Applies to:** ENGINE — this is the spec-level foundation for Soteria's deterministic-rendering claims and for golden-image regression testing; avoid shader side effects in safety-rated pipelines.

### 1.13 Validation philosophy
Vulkan SC keeps Vulkan's "limited runtime error checking" philosophy: Valid Usage defines the contract; *who* checks (application, driver, formal proof) is a system-level certification decision documented in the vendor's safety manual. Validation layers are expected in development, not in deployment; Khronos explicitly has **no plans to provide a safety-certified loader or validation layers**.
Source: https://www.khronos.org/blog/vulkan-sc-overview
**Applies to:** ENGINE — Soteria must not rely on layers at runtime; it should add its own input validation at its API boundary (defensive design consistent with ASIL-D) while treating driver VUs as preconditions proven by test + static analysis.

### 1.14 "Fresh-ID objects" — terminology check
The Vulkan SC 1.0 spec contains **no "fresh ID"/handle-freshness requirement** (verified by full-text search of the spec HTML). The adjacent real concepts are: **SCID-7** (implementation must expose a runtime-unique identifier — name/version/build — interrogable at any time, which an engine should capture into telemetry) and the **high-water-mark reservation model** (§1.3), under which most object handles may be recycled through create/destroy while pools/memory/swapchains may not be freed. If "fresh-id" was noted from a vendor safety manual, treat it as vendor-specific, not core-spec.
Source: Vulkan SC 1.0 spec (§2.1.1 Change Justification Table; deviations appendix)
**Applies to:** ENGINE — no engine obligation beyond ID/telemetry capture; do not design around a non-existent mandate.

### 1.15 MISRA alignment of the API header itself
The spec ships an appendix of **MISRA C:2012 deviations for `vulkan_sc_core.h`** (advisory/required notes on typedefs, unused tags/macros, identifier distinctness, unions, external definitions) with per-rule rationale — a template for how Soteria should document its own MISRA C++:2023 deviations.
Source: Vulkan SC spec, "MISRA C:2012 Deviations" appendix
**Applies to:** ENGINE (deviation-record format) — copy this pattern.

---

## 2. Features Expected of Safety-Critical Display Engines (ISO 26262 ASIL-D / DO-178C / ARINC 653 contexts)

### 2.1 Telltale / safety-content monitoring (per-frame signature check)
ISO 26262-compliant clusters must guarantee that warning lights, telltales, and gear indicators (PRNDL) are correctly displayed; the canonical mechanism is a **signature/CRC computed over the safety-critical content of every displayed frame** and compared against expectation, with driver notification (warning message / safe icon) on mismatch. Cypress/Infineon clusters pair a CRC "signature unit" in the display path with software such as Altia's ISO 26262 Safety Monitor (an ASIL-B SEooC that "checks signatures of safety-critical content in each frame of the display's output"); Altia's Safety Monitor received UL ISO 26262 tool qualification in 2026. Epson's automotive display controllers similarly provide a warning-lamp overlay layer, strict layer management, and rendering-cycle monitoring with CRC.
Sources: https://embeddedcomputing.com/technology/security/iec-iso-other-standards/designing-iso-26262-compliant-hmi-graphics-and-storage-for-automotive-clusters ; https://altia.com/2026/02/03/altia-announces-iso-26262-tool-qualification-from-ul-solutions-strengthening-safety-critical-automotive-development/ ; https://altia.com/2017/08/15/altia-announces-iso-26262-software-and-services-for-automotive-embedded-graphics/
**Applies to:** ENGINE (tag regions/objects as safety-critical, compute/verify per-frame signatures where hardware does not, expose pass/fail to telemetry) + PLATFORM (the actual display-path CRC unit) + APP (declares which content is safety-related).

### 2.2 Watchdog / heartbeat and deadline monitoring
Automotive safety MCUs integrate watchdog, clock supervisor, low-voltage detection, CRC engine, timing-protection and peripheral-protection units; HMI software is expected to be supervised by them (kick only when the frame loop is healthy). In avionics, ARINC 653 mandates a **Health Monitor** that detects partition-level failures (deadline overruns, process death) and applies configured recovery actions, preventing cascading failures.
Sources: https://embeddedcomputing.com/technology/security/iec-iso-other-standards/designing-iso-26262-compliant-hmi-graphics-and-storage-for-automotive-clusters ; https://www.sysgo.com/blog/article/arinc-and-arinc-653-in-embedded-software
**Applies to:** ENGINE (emit heartbeat per completed frame; detect frame-deadline misses itself; never kick the hardware watchdog directly — that's APP/PLATFORM, but the engine must supply the liveness evidence).

### 2.3 Deterministic frame pacing within partition schedules
ARINC 653 systems run partitions on fixed time windows inside a repeating major frame with cyclic scheduling; a display engine in such a system must complete its GPU submission and present inside its partition window — reinforcing bounded WCET per frame, no unbounded waits, and a schedule that degrades predictably (skip/dup frame) rather than overrun.
Source: https://www.sysgo.com/blog/article/arinc-and-arinc-653-in-embedded-software ; https://www.windriver.com/resource/safety-critical-software-development-for-integrated-modular-avionics
**Applies to:** ENGINE (frame scheduler design: fixed cadence, bounded work, defined overrun behavior) + PLATFORM (the actual partition scheduler).

### 2.4 GPU fault detection, health monitoring and overutilization control
Commercial stacks pair the SC driver with a **GPU safety monitor**: CoreAVI TrueCore provides "real-time verification of graphics and compute pipelines" and — with VkCore SC on Mali-G78AE — lets a monitor "detect GPU overutilization … [for] graphics applications with different levels of safety sharing a non-pre-emptive GPU"; Lynx describes TrueCore as "continuous health validation" of the GPU. NASA's X-59 XVS uses VkCore SC + TrueCore 2.0 "to ensure the health and integrity of the system's GPU … to the highest DAL levels."
Sources: https://www.lynx.com/solutions/safety-critical-graphics-certification ; https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/FuSa%20Partners/CoreAVI_ProductBrief_VkCoreSC.pdf ; https://www.mobilityengineeringtech.com/component/content/article/43753-sae-ma-03064
**Applies to:** DRIVER/PLATFORM (TrueCore is driver-stack IP) — but the ENGINE must surface equivalent app-visible health (fault counts, frame-time overruns, present failures) and consume driver health signals when present.

### 2.5 Safe state indication and degraded-mode operation
On unrecoverable display failure the driver must be *informed* (warning message, fallback icon, or dedicated safe display). Cluster literature requires the system to detect corruption and "notify the driver with a warning message"; the mechanism implies a pre-provisioned minimal rendering path that cannot be corrupted by the same fault (simple pipeline, static assets).
Source: https://embeddedcomputing.com/technology/security/iec-iso-other-standards/designing-iso-26262-compliant-hmi-graphics-and-storage-for-automotive-clusters
**Applies to:** ENGINE (provide the pre-registered "safe frame" pipeline + escalation policy); APP decides when to enter/exit degraded mode with system-level agreement.

### 2.6 Freeze-frame detection
A display that shows a plausible-but-stale image is a classic cluster hazard; the standard countermeasure is liveness proof of the render loop (frame counters/sequence numbers composited or signed per frame, monitored by a safety monitor) — a specialization of §2.1/§2.2 (signature check catches *corruption*, sequence/freshness checks catch *stasis*). Input-side freshness (attitude data age) is the dual problem on the data path.
Sources: derived from the signature-monitor and ARINC 653 health-monitor sources above; Altia ASM "ensures that every pixel displays correctly" per-frame.
**Applies to:** ENGINE (frame sequence counter embedded in output + telemetry), APP/PLATFORM (independent monitor compares).

### 2.7 Memory/asset integrity (ECC/EDC, safe boot, instant-on)
Cluster solutions require fail-safe storage for graphics assets (ECC NOR flash with corruption reporting and recovery), safe boot, and "instant-on" display of correct data at power-on/reset — i.e., time-to-first-valid-frame is a *safety-relevant* metric.
Source: https://embeddedcomputing.com/technology/security/iec-iso-other-standards/designing-iso-26262-compliant-hmi-graphics-and-storage-for-automotive-clusters
**Applies to:** PLATFORM (storage ECC) + ENGINE (integrity-check loaded assets — CRC the pipeline cache and any resource blobs at init; minimize and measure boot-to-first-frame).

### 2.8 Dual-channel / lockstep comparison
ASIL-D decomposition commonly uses two dissimilar channels with comparison (e.g., QM renderer + ASIL monitor channel, or duplicated computation with voter). None of the surveyed GPU stacks implements pixel-dual-rendering *inside* the graphics driver; comparison happens at the safety-monitor/display-controller level (signature of channel output) or system level (redundant ECUs).
Sources: inferred from §2.1/§2.4 architectures (Altia ASM SEooC; TrueCore; Cypress signature unit).
**Applies to:** Mostly SYSTEM/PLATFORM; ENGINE contribution: deterministic, inspectable output (Invariance §1.12) and a clean separation between the safety channel and rich/QM content (freedom from interference, §6).

### 2.9 Dithering / image-quality determinism
Not a certification mandate per se, but automotive/avionics display practice requires banding-free gradients (attitude horizon sky/ground) on 6–8-bit panels; any dithering used must itself be *deterministic* (fixed ordered pattern), consistent with SCID-1 and Invariance Rule 1 — stochastic temporal dither would violate repeatability and complicate golden-image testing.
Sources: derived from spec Invariance appendix (§1.12) — no vendor source mandates dithering; listed here because the prompt asked.
**Applies to:** ENGINE.

### 2.10 Data-path verification end-to-end ("display data path verification")
The surveyed automotive solution verifies the whole chain: asset storage (ECC flash) → GPU/HMI render (safety monitor signature per frame) → display controller (CRC engine, layer management, rendering-cycle monitoring) → panel. The lesson for an engine: *its own* responsibility is the middle links, and it should expose the hooks (region tags, signatures, telemetry) the other links need.
Sources: https://embeddedcomputing.com/technology/security/iec-iso-other-standards/designing-iso-26262-compliant-hmi-graphics-and-storage-for-automotive-clusters ; Epson functional-safety HUD page (search-result snippet, site blocks fetch).
**Applies to:** ENGINE + PLATFORM cooperatively.

---

## 3. What Commercial Safety-Critical GPU/Display Stacks Provide

| Vendor / Product | What it is | Feature checklist (from product pages/docs) | Relevance to Soteria |
|---|---|---|---|
| **CoreAVI / Lynx CoreSuite 2.0 — VkCore® SC** | Vulkan SC graphics+compute driver, certification-ready | DO-178C DAL A & ISO 26262 ASIL D certification evidence; RTOS portability (VxWorks, Deos, PikeOS, QNX, LynxOS-178…); OpenGL SC1/SC2 layered libraries (VkCoreGL SC2); ComputeCore algorithm libs; EncodeCore/DecodeCore low-latency video synchronized with graphics; deployed on NASA X-59 XVS | The dominant certified *driver* layer; Soteria would sit on top of such a driver |
| **CoreAVI TrueCore™ 2.0** | GPU safety monitor | Real-time verification of graphics/compute pipelines; GPU overutilization detection for mixed-criticality sharing of a non-preemptive GPU; "continuous health validation" | Model for Soteria's telemetry/health interface (what a monitor expects to see) |
| **Lynx ProfileCore** | GPU performance profiler | "Optimize application execution and validate Worst-Case Execution Time (WCET)" | Validates §6 temporal-determinism requirement; Soteria needs WCET-measurable frames |
| **DiSTI GL Studio® Safety-Critical** | HMI design tool + code generator + runtime | SC Code Generator qualified to **DO-330 TQL-1**; SC runtime certifiable to **DO-178C DAL A**; ARINC 661 deployment in segmented certifiable environments; coverage-tested runtime (Boeing T-7A, Boom XB-1); Mixed-Criticality™ variant; GL Studio Micro for MCUs | The canonical *engine/authoring* layer above the driver; shows code-gen + TQL-1 is the industry pattern (cf. Soteria's offline tooling) |
| **Ansys SCADE (Suite KCG / Display / ARINC 661 Solutions)** | Model-based display dev + qualified code generation | KCG C/Ada code generator qualified **DO-178C/DO-330 TQL-1**; SCADE Display for certified cockpit graphics; ARINC 661 DF Generator with updated DO-178C qualification; VectorCast test-harness generation | Same pattern as DiSTI: certified output comes from *qualified generators + certified runtime* |
| **Basemark Rocksolid (Graphics Engine / Core)** | Automotive graphics engine + toolchain + cluster reference | First ISO 26262 **ASIL-B**-certifiable OpenGL SC 2.0 graphics solution running on CoreAVI VkCore SC (NXP i.MX 8) and Imagination drivers (Renesas R-Car H3); identical cluster app on both platforms (HW/SW separation); Display Safety Framework for clusters/safety-critical HMI | Closest public analogue to Soteria's positioning (engine above SC driver); note ASIL-B is typical for clusters — ASIL-D scope is a differentiator |
| **Altia (Safety Monitor / ASM)** | ISO 26262 SEooC monitor + HMI tools | Per-frame signature checking of safety-critical content; UL ISO 26262 tool qualification (ISO 26262-8 TCL evaluation); monitors correctness "when commanded" | Pattern for the independent *monitor channel* Soteria should feed |
| **NVIDIA Vulkan SC (GeForce/RTX, Jetson, DRIVE)** | Vulkan SC implementations | Desktop/Jetson = development (not safety-certified); DriveOS QNX Safety = ASPICE + ISO 21434 certified, QM level ISO 26262; presentation via OpenWFD/NvSci on DRIVE/Jetson, VK_KHR_display on desktop; PCC + JSON-generation layer + vulkanscinfo tooling | Likely Soteria dev target; be explicit about which platforms carry certification evidence |
| **Siemens (Verification Horizons)** | Tool-qualification guidance (not a display product) | Clarifies ISO 26262-8 clause 11.4.6 four qualification methods (see §6) | Source for Soteria's tool-qualification plan |
| **EnSilica** | (No public safety-critical display-engine product found in this research; automotive/imaging chip IP house) | — | Excluded from checklist; revisit if a specific product is meant |

Key URLs: https://www.lynx.com/solutions/safety-critical-graphics-certification ; https://www.mobilityengineeringtech.com/component/content/article/43753-sae-ma-03064 ; https://www.disti.com/gl-studio/safety-critical-avionics ; https://ansys.synopsys.com/products/embedded-software/solutions-for-arinc-661 ; https://www.basemark.com/news/basemark-launches-iso-26262-compliant-graphics-solution/ ; https://developer.nvidia.com/vulkan-sc ; https://blogs.sw.siemens.com/verificationhorizons/2022/04/13/clearing-the-fog-of-iso-26262-tool-qualification/

**Cross-vendor lessons for an ENGINE layer:** (1) certified artifacts (evidence packages, safety manuals) are as much the product as code; (2) code generation/offline tools get *tool-qualified* (DO-330 TQL-1 / ISO 26262-8 TCL), runtimes get *certified*; (3) a separate monitor component (TrueCore/ASM) supervises the render path; (4) hardware independence is a selling point (Basemark's identical cluster on two SoCs); (5) mixed-criticality support (QM + safety content sharing one GPU/display) is the hard, differentiating feature.

---

## 4. Vulkan SC Conformance & the Khronos CTS

The Vulkan SC CTS is part of the open-source VK-GL-CTS (dEQP framework). Facts a conformant implementation — and therefore any engine on top — must live with:

1. **Dedicated module & mustpass:** `deqp-vksc` executable; mustpass list `external/vulkancts/mustpass/main/vksc-default.txt`; fraction-mandatory list `vksc-fraction-mandatory-tests.txt`.
2. **Two-pass test execution:** every test runs twice — a main-process pass that collects used pipelines, object counts, etc. (mirroring the SC static-reservation workflow), then a subprocess that performs the real test; the pair communicates over a local TCP connection. This validates that the implementation works under the offline/reservation regime an engine must also follow.
3. **Offline pipeline compiler integration:** `--deqp-pipeline-compiler/-dir/-args/...` hook in the vendor PCC; a reference `vksc-pipeline-compiler` (built on the Vulkan API) demonstrates input/output contract.
4. **Filesystem-less targets:** `vksc-server` runs on a host to do file I/O, pipeline compilation and logging for embedded targets (`--deqp-server-address`).
5. **Sizing knobs:** `--deqp-command-pool-min-size`, `--deqp-command-buffer-min-size`, `--deqp-command-default-size` (256 B), `--deqp-pipeline-default-size` (16 KiB) — mirrors the app-side need to estimate command/pipeline memory (§1.5).
6. **Vendor-tweakable surface (allowed in submissions):** `vkscpc/`, `vkscserver/`, and `modules/vulkan/sc/vktApplicationParametersTests.cpp` (vendor-specific application parameters, fed via `--deqp-app-params-input-file`).
7. **Pass criteria & submission:** allowed result codes Pass/NotSupported/QualityWarning/CompatibilityWarning/Waiver; submission = full QPA logs + git status/log + patches + STATEMENT; waivers via XML with issue references; verified by `verify_submission.py` (VK-GL-CTS-Tools).
8. **Adopter program:** passing CTS + adopter agreement is required to use the Vulkan SC name/logo; conformant products today include CoreAVI VkCore SC and NVIDIA (DRIVE, Jetson, desktop).

Sources: https://github.com/KhronosGroup/VK-GL-CTS/blob/main/external/vulkancts/README.md ; https://www.khronos.org/vulkansc/
**Applies to:** DRIVER (the CTS certifies implementations) — ENGINE implications: (a) rely only on behavior the CTS can actually verify; (b) treat validation layers + CTS-style two-pass testing as the model for Soteria's own test harness (a "reservation discovery" pass followed by a "production-mode" pass is a genuinely useful app-level pattern); (c) the engine's init sequence must tolerate conformant-but-different property sets (test via DevSim, §5).

---

## 5. Open-Source Vulkan SC Projects / SDKs and What Their Samples Demonstrate

1. **Vulkan SC SDK (Khronos, vksc1.0.21, May 2026)** — one installer bundling: SC Loader, **Emulation driver stack**, **Validation Layers**, **Device Simulation Layer (devsim)**, **pipeline-cache utilities** (pcutil: `pcinfo`, PCC discovery), **cube sample**, `vulkanscinfo`, and CMake/PCC integration. https://www.khronos.org/blog/vulkan-sc-sdk-released-an-integrated-toolchain-for-vulkan-safety-critical-development ; https://github.com/KhronosGroup/VulkanSC-SDK
2. **cube-vksc (`vksccube`)** — the canonical sample. Demonstrates the "expected" engine-feature baseline: VK_KHR_display presentation, a PCC-produced pipeline-cache binary *embedded in the executable* (with `--pipeline-cache` override for other ICDs), device creation with reservation info, and the SDK's automatic PCC discovery (`REBUILD_PIPELINE_CACHE`, `VKSC_PIPELINE_CACHE_FLAGS`). https://github.com/KhronosGroup/VulkanSC-Tools/blob/sc_main/cube-vksc/README.md
3. **VulkanSC-Emulation (vksconvk ICD + pcconvk PCC)** — runs SC apps on any Vulkan 1.2+memory-model device. Notably demonstrates **fault-path testing**: emits `VK_FAULT_TYPE_COMMAND_BUFFER_FULL` on reserved-pool exhaustion, `VK_FAULT_TYPE_PHYSICAL_DEVICE` on underlying `VK_ERROR_DEVICE_LOST`, `VK_FAULT_TYPE_INVALID_API_USAGE` on detected misuse; emulates command-pool memory consumption; emulates `VK_KHR_display` (configurable display count/geometry); PCC packages SPIR-V+JSON ("debug info") making caches portable. https://www.rastergrid.com/blog/gpu-tech/2025/02/the-vulkan-sc-emulation-driver-stack/ ; https://github.com/KhronosGroup/VulkanSC-Emulation
4. **VulkanSC-ValidationLayers** — SC-specific state tracking/validation classes over upstream VVL; parses pipeline-cache *debug info* (embedded JSON + SPIR-V) to restore full SPIR-V-dependent validation; VUID filtering where SC rules diverge from Vulkan. Validation-layer tests run against a Mock ICD. https://www.rastergrid.com/blog/projects/2024/07/behind-the-scenes-the-vulkan-sc-ecosystem/ ; https://github.com/KhronosGroup/VulkanSC-ValidationLayers
5. **Device Simulation Layer (devsim)** — reports a configurable JSON "profile" of `VkPhysicalDeviceVulkanSC10Properties` instead of the real ones; used to test apps/layers against capability variations (e.g., no `commandPoolResetCommandBuffer`, no `commandBufferSimultaneousUse`). https://github.com/KhronosGroup/VulkanSC-Tools/blob/sc_main/devsim/README.md
6. **VulkanSC-pcutil / PccDiscovery** — parse SC pipeline-cache binaries (header, index entries, debug info); `pcinfo` lists pipeline memory requirements and suggests pool bucket sizes; CMake-based PCC discovery used by the SDK. https://github.com/KhronosGroup/VulkanSC-pcutil
7. **NVIDIA Jetson/DRIVE samples & JSON generation layer** — `vulkanscinfo`; a Vulkan layer that captures a running *Vulkan* app's pipelines into PCC input JSON (then `pcc --path … --chip ga10b --out cache.bin`, `--hex` to embed as C array) — i.e., the sanctioned "author in Vulkan, ship in Vulkan SC" workflow. https://docs.nvidia.com/jetson/archives/r35.1/DeveloperGuide/text/SD/GraphicsProgramming/VulkanSC.html

**What the ecosystem implies an SC engine "should" demonstrate:** embedded/overridable pipeline-cache binaries; display-path abstraction over VK_KHR_display (and vendor WSI equivalents); deliberate fault-handling test coverage (the emulation ICD exists largely *because* native drivers make fault paths hard to exercise); capability-matrix testing via devsim; pool sizing derived from measured data (pcinfo / vkGetCommandPoolMemoryConsumption); and dev-only validation + release builds with layers/logging stripped — all of which map directly onto Soteria's stated design.
**Applies to:** ENGINE (all of the above are engine-level patterns; the components themselves are dev tools, not deployable certified parts).

---

## 6. ISO 26262 Software Aspects Relevant to a Rendering Engine

### 6.1 Software tool qualification (ISO 26262-8 clause 11) — applies to Soteria's *offline* toolchain
Tools are classified by Tool impact (TI1/TI2) × Tool error Detection (TD1–TD3) into TCL1/2/3; TCL1 needs no qualification, while TCL2/3 require qualification by one of four methods (clause 11.4.6): **increased confidence from use, evaluation of the tool development process, validation of the software tool, or development in compliance with a standard**. A rendering engine's *runtime* is a work product developed to ASIL-D directly, but its **offline tools (PCC invocation scripts, pipeline-JSON generators, any code generation, build/config tooling)** are classic TCL2/TCL3 candidates — the industry pattern is to qualify generators (DiSTI: DO-330 TQL-1; SCADE KCG: TQL-1; Altia: UL ISO 26262 tool qualification) and/or validate their outputs (Soteria already loads only pre-validated, integrity-checked pipeline caches — that output-validation step is itself a TD-improving measure).
Sources: https://blogs.sw.siemens.com/verificationhorizons/2022/04/13/clearing-the-fog-of-iso-26262-tool-qualification/ ; https://www.btc-embedded.com/when-and-how-to-qualify-tools-according-to-iso-26262 ; https://www.embitel.com/blog/embedded-blog/why-is-software-tool-qualification-indispensable-in-iso-26262-based-software-development
**Applies to:** ENGINE-TOOLS (qualify or validate), not the deployed runtime.

### 6.2 Freedom from interference (FFI)
For mixed-criticality displays (safety telltales + QM infotainment on one GPU/panel), ISO 26262 requires evidence that lower-ASIL elements cannot cascade-fail into ASIL-D ones (dependent-failure analysis, ISO 26262-9; coexistence criteria). For an engine this means: strict memory partitioning (SC's static reservations are the GPU-side enabler), bounded GPU time (CoreAVI TrueCore's overutilization detection exists precisely for "graphics applications with different levels of safety sharing a non-pre-emptive GPU"), no shared mutable state with QM components, and temporal isolation consistent with the platform's partition schedule (ARINC 653 time/space partitioning is the avionics analog).
Sources: https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/FuSa%20Partners/CoreAVI_ProductBrief_VkCoreSC.pdf ; https://www.sysgo.com/blog/article/arinc-and-arinc-653-in-embedded-software ; https://www.rapitasystems.com/iso26262
**Applies to:** ENGINE (design for spatial/temporal isolation; document interference channels) + PLATFORM (MMU/partition OS/display controller layering enforce it).

### 6.3 Temporal determinism / WCET of a frame
ASIL-D scheduling analysis needs a bounded WCET per frame. Vulkan SC was explicitly designed for this: offline pipelines remove compile hitches; static reservations remove runtime allocation; SCID-3 forbids async callbacks; SCID-4 forbids garbage collection and fine-grained freeing; command-buffer memory consumption is deterministic and measurable; the Invariance appendix makes output repeatable. Lynx sells ProfileCore specifically to "validate Worst-Case Execution Time (WCET)" of GPU work. The engine's remaining duties: bounded loop iterations, no unbounded waits (Soteria's hang timeout ✔), measured worst-case submit/present paths, and a defined policy when a frame misses its deadline.
Sources: spec §2.1.1 + Invariance appendix; https://www.lynx.com/solutions/safety-critical-graphics-certification ; https://developer.nvidia.com/vulkan-sc ("eliminates GPU processing stalls due to pipeline creation and memory allocations during runtime")
**Applies to:** ENGINE (primary), with DRIVER properties (`deviceNoDynamicHostAllocations`, command determinism) as the foundation.

### 6.4 Memory partition guarantees
SC's device-level reservation + no-`pAllocator` + no-`vkFreeMemory` model gives a static host/device memory map at init; `deviceNoDynamicHostAllocations==VK_TRUE` strengthens it to "no driver host allocations under valid usage." Engine-level duties: allocate every Vulkan object and every engine-side structure at init (Soteria ✔), place safety-critical images/buffers in SEU-safe heaps where available, and prove (via review + analysis) that no code path allocates post-init — a freedom-from-interference and WCET precondition.
Sources: spec deviations appendix (`deviceNoDynamicHostAllocations`, `pAllocator must be NULL`) ; https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkPhysicalDeviceVulkanSC10Properties.html
**Applies to:** ENGINE + DRIVER.

### 6.5 MISRA C++:2023 as the coding standard
MISRA C++:2023 (179 guidelines) targets **C++17**, merges the AUTOSAR C++14 guidelines into MISRA's framework, promotes the "Rule of Zero" for special member functions, and is notably stricter on implicit conversions than MISRA C's essential-type model; static-analysis compliance modules exist (Helix QAC etc.). For Soteria: pin the C++17 subset, wire a compliance checker into CI, and maintain a deviation register modeled on the Vulkan SC spec's own MISRA C appendix (§1.15). No-exceptions/no-RTTI (Soteria ✔) is consistent with typical automotive profiles of these rules.
Sources: https://www.perforce.com/blog/qac/misra-cpp-2023-intro ; https://www.parasoft.com/blog/misra-cpp-2023-guide/ ; https://arxiv.org/html/2605.23490v1 (guideline count)
**Applies to:** ENGINE (process, not a runtime feature).

### 6.6 SEooC / certification packaging
The market pattern (Altia Safety Monitor as "ISO 26262-compliant SEooC"; DiSTI runtime with "certification artifacts"; CoreAVI "certification data kits") shows engines are delivered as **Safety Elements out of Context** with: safety manual (assumptions of use, confirmed/potential faults), requirements traceability, coverage-tested code, and configuration/version identification (SCID-7). Soteria should budget for the evidence package from day one — it shapes API design (e.g., every safety-relevant behavior must be documented, testable, and configurable per §SCID-5).
Sources: https://altia.com/2017/08/15/altia-announces-iso-26262-software-and-services-for-automotive-embedded-graphics/ ; https://www.disti.com/gl-studio/safety-critical-avionics
**Applies to:** ENGINE (deliverable shape).

---

## 7. Prioritized Feature Checklist for the Soteria MVP

Legend: ✅ = already present per project description; ◐ = partially present; ☐ = absent.

### P0 — Must-have (safety-case backbone; MVP is not credible without these)

| # | Feature | Why / source | State |
|---|---------|--------------|-------|
| P0-1 | **Complete static resource contract**: full `VkDeviceObjectReservationCreateInfo` (all request counts + aggregate limits + pipeline pool buckets derived from `pcinfo`) + mandatory `VkPhysicalDeviceVulkanSC10Features` | Spec §1.3/1.4; device creation fails without it | ◐ |
| P0-2 | **Pipeline-cache integrity & compatibility validation** at init: header version, vendor/device compatibility, per-pipeline identifier presence (`VK_ERROR_INVALID_PIPELINE_CACHE_DATA`/`VK_ERROR_NO_PIPELINE_MATCH` paths), CRC of the binary | Spec §1.2; asset-integrity §2.7 | ◐ (loading exists; validation rigor TBD) |
| P0-3 | **Fault handling**: `VkFaultCallbackInfo` at device creation + per-frame `vkGetFaultData` poll; classify by level/type; defined per-type reaction table; feed telemetry | Spec §1.7 | ☐ |
| P0-4 | **Device-lost / hang policy**: bounded fence waits (✔), `VK_ERROR_DEVICE_LOST` classification, escalation to safe-state rendering and/or system health monitor; never block indefinitely | Spec §1.9 | ◐ |
| P0-5 | **Frame deadline monitoring + heartbeat**: measure frame time vs budget, count misses, emit heartbeat for external watchdog/ARINC 653-style HM; defined overrun behavior (skip frame, hold last good) | §2.2/2.3 | ◐ (telemetry exists; deadline logic TBD) |
| P0-6 | **Safe-state indication**: pre-provisioned minimal "safe frame" (static assets, simplest pipeline) renderable after unrecoverable faults; deterministic shutdown (✔) | §2.5 | ☐ |
| P0-7 | **Zero dynamic allocation after init** (✔) verified by analysis/test; `pAllocator==NULL` everywhere; prefer `deviceNoDynamicHostAllocations` devices | Spec §1.6, §6.4 | ✅ |
| P0-8 | **Input data integrity on attitude source**: range/plausibility checks, freshness/sequence counters, stale-data flag propagated to rendered output (no silently frozen horizon) | §2.2/2.6 (data-path analog) | ◐ |
| P0-9 | **Version/variant & identity capture**: assert API variant 1; record vendorID/deviceID/driverVersion/conformance into telemetry (SCID-7) | §1.1, §1.14 | ☐ |
| P0-10 | **Command-pool memory budgeting**: measure with `vkGetCommandPoolMemoryConsumption` at init; reserve with margin; runtime check in dev builds; fault-tested via emulation ICD `COMMAND_BUFFER_FULL` injection | Spec §1.5; §5(3) | ◐ |
| P0-11 | **Dev/test hygiene**: SC Validation Layers in dev (✔ logging stripped in release), MISRA C++:2023 checker in CI, no exceptions/RTTI (✔) | §1.13, §6.5 | ◐ |
| P0-12 | **Health/telemetry report** (✔) extended: fault counts, deadline misses, device-lost events, safe-state status, data-staleness flags — the interface a TrueCore/ASM-style monitor consumes | §2.4 | ◐ |

### P1 — Should-have (expected of a credible ASIL-D display engine)

| # | Feature | Why / source |
|---|---------|--------------|
| P1-1 | **Telltale/safety-region tagging + per-frame signature/CRC verification hook** (compute in engine or interface to display-controller signature unit); mismatch → warning + safe state | §2.1 (ISO 26262 cluster practice: Altia ASM, Cypress) |
| P1-2 | **Freeze-frame detection**: frame sequence counter in output + monitor-visible freshness; distinct from corruption detection | §2.6 |
| P1-3 | **Degraded mode**: pre-compiled reduced pipeline set & reduced feature path entered on RECOVERABLE faults; formal entry/exit conditions | §2.5 |
| P1-4 | **SEU mitigations**: prefer `VK_MEMORY_HEAP_SEU_SAFE_BIT` heaps; periodic `vkCmdRefreshObjectsKHR` for refreshable object types when unavailable | Spec §1.8 |
| P1-5 | **Deterministic frame pacing**: fixed cadence aligned to platform schedule (ARINC 653 partition window), bounded present logic, no drift accumulation | §2.3 |
| P1-6 | **Capability-matrix testing** with devsim across property sets (`commandPoolResetCommandBuffer`, `commandBufferSimultaneousUse`, `recycle*`, fault counts) | §4, §5(5) |
| P1-7 | **Golden-image/invariance regression tests** per pipeline (repeatability rules; no shader side effects in safety pipelines) | Spec §1.12 |
| P1-8 | **Two-pass self-check init** (CTS-inspired): reservation-discovery pass in dev builds verifies declared reservations ≥ actual usage | §4(2) |
| P1-9 | **Tool-qualification plan**: TCL assessment of pipeline-JSON/PCC/build tooling; output validation (cache CRC) as TD improvement; deviation register (MISRA appendix format) | §6.1, §1.15 |
| P1-10 | **WCET/frame-time characterization** in dev (timestamps; optionally `VK_KHR_performance_query` with `VkPerformanceQueryReservationInfoKHR`); documented frame WCET claim | §6.3 |
| P1-11 | **Deterministic ordered dither** (if needed for horizon gradients on target panels) — fixed pattern, no temporal noise | §2.9 |
| P1-12 | **Boot-to-first-valid-frame metric** measured and minimized (instant-on expectation) | §2.7 |

### P2 — Nice-to-have (differentiators / later phases)

| # | Feature | Why / source |
|---|---------|--------------|
| P2-1 | **Dual-channel comparison support**: engine output structured so an independent monitor channel (or second render path) can compare | §2.8 (mostly system-level) |
| P2-2 | **ARINC 661-style widget/UA separation API** for avionics CDS architectures | §2 sources (ARINC 661, SCADE/DiSTI) |
| P2-3 | **Multi-display / display-group failover** (configurable displays as in emulation ICD JSON configs) | §1.10, §5(3) |
| P2-4 | **OpenWFD/NvSci presentation backend** behind the existing WSI abstraction (DRIVE/Jetson targets) | §1.10 |
| P2-5 | **Fault-injection test suite** on the emulation ICD in CI (COMMAND_BUFFER_FULL, DEVICE_LOST→PHYSICAL_DEVICE, INVALID_API_USAGE) | §5(3) |
| P2-6 | **SPIR-V+JSON debug-info caches in dev builds** (enables full validation-layer SPIR-V checks), stripped for production | §5(4), spec §1.2 |
| P2-7 | **Dev ring-buffer trace** for certification debug sessions (compiled out in release — policy already set) | §1.13 |
| P2-8 | **Compute pipeline support** (deferred; SC supports compute but Soteria's 2D/3D scope may not need it in MVP) | spec (compute pipelines exist in SC) |

### Explicitly *not* engine-scope (assign elsewhere)
- Display-path CRC hardware, panel-level overlay of warning lamps, ECC flash, safe boot → **PLATFORM/HW** (§2.1/2.7).
- Partition scheduling, MMU enforcement, hardware watchdog kicking → **RTOS/PLATFORM** (§2.2/2.3, §6.2).
- Vulkan SC driver, PCC binary correctness, TrueCore-class GPU monitoring → **DRIVER vendor** (§3); Soteria consumes their safety manuals as environmental assumptions.
- The decision of *which* displayed items are safety-related, and system-level hazard analysis → **APP/OEM**; Soteria provides the tagging/monitoring mechanisms.

---

## Appendix A — Primary Sources
1. Vulkan SC 1.0 Specification (HTML, searched directly incl. deviations & MISRA appendices): https://registry.khronos.org/VulkanSC/specs/1.0-extensions/html/vkspec.html
2. Khronos blog — Vulkan SC Overview & differences: https://www.khronos.org/blog/vulkan-sc-overview
3. Khronos Vulkan SC landing page: https://www.khronos.org/vulkansc/
4. Vulkan SC man pages: VkDeviceObjectReservationCreateInfo, VkPhysicalDeviceVulkanSC10Properties, vkGetFaultData, VkFaultData, VkFaultType — under https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/
5. RasterGrid — Behind the scenes: Vulkan SC Ecosystem: https://www.rastergrid.com/blog/projects/2024/07/behind-the-scenes-the-vulkan-sc-ecosystem/
6. RasterGrid — Vulkan SC Emulation driver stack: https://www.rastergrid.com/blog/gpu-tech/2025/02/the-vulkan-sc-emulation-driver-stack/
7. Khronos blog — Vulkan SC SDK released (vksc1.0.21): https://www.khronos.org/blog/vulkan-sc-sdk-released-an-integrated-toolchain-for-vulkan-safety-critical-development
8. VK-GL-CTS README (Vulkan SC CTS section): https://github.com/KhronosGroup/VK-GL-CTS/blob/main/external/vulkancts/README.md
9. cube-vksc README: https://github.com/KhronosGroup/VulkanSC-Tools/blob/sc_main/cube-vksc/README.md
10. VulkanSC-pcutil: https://github.com/KhronosGroup/VulkanSC-pcutil
11. NVIDIA Vulkan SC: https://developer.nvidia.com/vulkan-sc ; Jetson Vulkan SC guide: https://docs.nvidia.com/jetson/archives/r35.1/DeveloperGuide/text/SD/GraphicsProgramming/VulkanSC.html
12. Lynx/CoreAVI safety-critical graphics (CoreSuite 2.0: VkCoreSC/TrueCore/ProfileCore): https://www.lynx.com/solutions/safety-critical-graphics-certification ; CoreAVI VkCore SC brief (Arm): https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/FuSa%20Partners/CoreAVI_ProductBrief_VkCoreSC.pdf
13. NASA X-59 / CoreAVI: https://www.mobilityengineeringtech.com/component/content/article/43753-sae-ma-03064
14. DiSTI GL Studio Safety-Critical Avionics: https://www.disti.com/gl-studio/safety-critical-avionics
15. Ansys SCADE ARINC 661: https://ansys.synopsys.com/products/embedded-software/solutions-for-arinc-661
16. Basemark ISO 26262 graphics launch: https://www.basemark.com/news/basemark-launches-iso-26262-compliant-graphics-solution/
17. Altia ISO 26262 (SEooC monitor; UL tool qualification): https://altia.com/2017/08/15/altia-announces-iso-26262-software-and-services-for-automotive-embedded-graphics/ ; https://altia.com/2026/02/03/altia-announces-iso-26262-tool-qualification-from-ul-solutions-strengthening-safety-critical-automotive-development/
18. Embedded Computing Design — ISO 26262 HMI graphics for clusters: https://embeddedcomputing.com/technology/security/iec-iso-other-standards/designing-iso-26262-compliant-hmi-graphics-and-storage-for-automotive-clusters
19. SYSGO — ARINC 653: https://www.sysgo.com/blog/article/arinc-and-arinc-653-in-embedded-software ; Wind River IMA: https://www.windriver.com/resource/safety-critical-software-development-for-integrated-modular-avionics ; ARINC 661: https://en.wikipedia.org/wiki/ARINC_661
20. ISO 26262 tool qualification: https://blogs.sw.siemens.com/verificationhorizons/2022/04/13/clearing-the-fog-of-iso-26262-tool-qualification/ ; https://www.btc-embedded.com/when-and-how-to-qualify-tools-according-to-iso-26262
21. MISRA C++:2023: https://www.perforce.com/blog/qac/misra-cpp-2023-intro ; https://www.parasoft.com/blog/misra-cpp-2023-guide/
