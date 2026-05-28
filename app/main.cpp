/// @file main.cpp
/// @brief Simulation orchestrator — avionics artificial horizon demo.
///
/// Architecture
/// ============
/// This file is the ONLY place that owns:
///   - Swapchain lifecycle (create / destroy)
///   - Acquire / present operations (vkAcquireNextImageKHR, vkQueuePresentKHR)
///   - Synchronisation objects (semaphores, fences)
///   - The frame loop
///
/// The renderer (HorizonRenderer) is given image views and, each frame,
/// is asked to record draw commands into a command buffer.  It returns
/// that command buffer ready for submission.  It never sees the swapchain
/// handle, never waits on a fence, and never calls vkQueueSubmit.
///
/// Frame loop sequence (per frame):
///   1. WaitAndReset(inFlightFence)   — guarantee GPU is done with the buffer
///   2. vkAcquireNextImageKHR         — obtain the next image index
///   3. renderer.RecordFrame(index)   — record draw commands
///   4. vkQueueSubmit                 — submit: wait imageAvailable, signal renderComplete
///   5. vkQueuePresentKHR             — present: wait renderComplete
///   6. frameReport.OnFrameComplete() — telemetry/logging
///
/// Init sequence (strict order):
///   VkscContext → DisplayOutput (surface) → SwapchainSc → HorizonRenderer → FrameSync
///
/// Shutdown sequence:
///   vkDeviceWaitIdle → FrameSync → HorizonRenderer → SwapchainSc →
///   DisplayOutput → VkscContext
///
/// Resource reservation
/// ====================
/// VulkanSC requires all object counts to be declared at vkCreateDevice time.
/// Numbers below must exactly match what the engine + renderer will create:
///
///   semaphores               = 2  (imageAvailable + renderComplete in FrameSync)
///   fences                   = 1  (inFlight in FrameSync)
///   commandPools             = 1  (HorizonRenderer::m_commandPool)
///   commandBuffers           = 1  (one pre-allocated primary buffer in the pool)
///   renderPasses             = 1  (HorizonRenderer::m_renderPass)
///   pipelineCaches           = 1  (HorizonRenderer::m_pipelineCache)
///   pipelineLayouts          = 1  (HorizonRenderer::m_pipelineLayout)
///   graphicsPipelines        = 2  (BG + LINE)
///   framebuffers             = 3  (one per swapchain image, max SwapchainSc::kMaxImages)
///   swapchains               = 1
///   surfaces                 = 1
///   buffers                  = 1  (HorizonRenderer::m_vertexBuffer)
///   deviceMemoryAllocations  = 1
///   imageViews               = 3  (one per swapchain image — created by SwapchainSc)
///   subpassDescriptions      = 1
///   attachmentDescriptions   = 1
///
/// PipelinePoolSizes (declared in reservation):
///   Pipelines compiled without poolEntrySize → pipelinePoolSizeCount = 0.
///   (poolEntrySize = 0 in VkPipelineOfflineCreateInfo at runtime)

#include "../engine/core/vksc_context.hpp"
#include "../engine/core/log.hpp"
#include "../engine/core/result.hpp"
#include "../engine/wsi/display_output.hpp"
#include "../engine/rendering/swapchain.hpp"
#include "../engine/rendering/frame_sync.hpp"

#include "rendering/horizon_renderer.hpp"
#include "rendering/pipeline_cache_data.hpp"

#include "demo/demo_attitude_source.hpp"
#include "demo/console_frame_report.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>   // GetTickCount64

#include <cstdlib>
#include <cstdint>
#include <cstdio>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Number of frames to render before a clean shutdown.
/// Set to 0 for infinite (not recommended during first validation runs).
static constexpr uint64_t kMaxFrames{600U};  // ~10 s at 60 fps

// ---------------------------------------------------------------------------
// ShutdownAll — unconditional, safe to call at any point in the init sequence.
// ---------------------------------------------------------------------------

static void ShutdownAll(
    engine::VkscContext&              ctx,
    engine::wsi::DisplayOutput&       display,
    engine::rendering::SwapchainSc&   swapchain,
    sim::rendering::HorizonRenderer&  renderer,
    engine::rendering::FrameSync&     sync) noexcept
{
    const VkDevice dev = ctx.Device();

    // Wait for all GPU work to finish before destroying any objects.
    if (dev != VK_NULL_HANDLE)
    {
        const VkResult waitResult = vkDeviceWaitIdle(dev);
        if (waitResult != VK_SUCCESS)
        {
            engine::log::Warn("main: vkDeviceWaitIdle failed during shutdown — proceeding.");
        }
    }

    sync.Shutdown(dev);
    renderer.Shutdown(dev);
    swapchain.Shutdown(dev);
    display.Shutdown();
    ctx.Shutdown();
}

// ---------------------------------------------------------------------------
// BuildContextConfig
// ---------------------------------------------------------------------------

static engine::VkscContextConfig BuildContextConfig(
    const VkPipelineCacheCreateInfo& pcCI,
    const VkPipelinePoolSize&        poolSizes) noexcept
{
    engine::VkscContextConfig cfg{};
    cfg.appName    = "soteria-horizon";
    cfg.engineName = "soteria";

    // Instance extensions required for VK_KHR_display WSI.
    cfg.instanceExtensions[0]      = VK_KHR_SURFACE_EXTENSION_NAME;
    cfg.instanceExtensions[1]      = VK_KHR_DISPLAY_EXTENSION_NAME;
    cfg.instanceExtensionCount     = 2U;

    // Device extensions required for swapchain presentation.
    cfg.deviceExtensions[0]    = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    cfg.deviceExtensionCount   = 1U;

    // Static object reservation (must match frame-loop resource usage exactly).
    cfg.resources.semaphores               = 2U;   // imageAvailable + renderComplete
    cfg.resources.fences                   = 1U;   // inFlight
    cfg.resources.commandPools             = 1U;
    cfg.resources.commandBuffers           = 1U;
    cfg.resources.renderPasses             = 1U;
    cfg.resources.pipelineCaches           = 1U;
    cfg.resources.pipelineLayouts          = 1U;
    cfg.resources.graphicsPipelines        = 2U;   // BG + LINE
    cfg.resources.framebuffers             = engine::rendering::SwapchainSc::kMaxImages;
    cfg.resources.swapchains               = 1U;
    cfg.resources.surfaces                 = 1U;
    cfg.resources.buffers                  = 1U;   // vertex buffer
    cfg.resources.deviceMemoryAllocations  = 1U;
    cfg.resources.imageViews               = engine::rendering::SwapchainSc::kMaxImages;
    cfg.resources.subpassDescriptions      = 1U;
    cfg.resources.attachmentDescriptions   = 1U;

    // Pipeline cache reservation — the emulation layer needs to know the
    // total amount of pre-compiled pipeline data before device creation.
    cfg.pipelineCacheInfos      = &pcCI;
    cfg.pipelineCacheInfoCount  = 1U;

    // Pipeline pool sizes — one entry covers both BG and LINE pipelines.
    // poolEntrySize must match HorizonRenderer::kPoolEntrySize exactly.
    // poolEntryCount = kPipelineCount (2: BG + LINE).
    cfg.pipelinePoolSizes      = &poolSizes;
    cfg.pipelinePoolSizeCount  = 1U;

    // On development systems the VulkanSC emulation ICD may coexist with a
    // native hardware VulkanSC ICD (e.g. NVIDIA nvvkscv64.dll).  Prefer the
    // emulation ICD so the pre-compiled pipeline cache binary (built with
    // VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN parameters) is accepted.
    cfg.preferredDriverId = VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN;

    return cfg;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    engine::log::Info("soteria: avionics horizon demo starting.");

    // -----------------------------------------------------------------------
    // 1. Prepare pipeline cache create-info (points to embedded binary).
    //    This is passed to VkscContext so the emulation ICD can reserve
    //    memory for the pre-compiled pipelines before vkCreateDevice.
    // -----------------------------------------------------------------------
    VkPipelineCacheCreateInfo pcCI{};
    pcCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pcCI.pNext           = nullptr;
    pcCI.flags           = 0U;
    pcCI.initialDataSize = sim::kPipelineCacheDataSize;
    pcCI.pInitialData    = sim::kPipelineCacheData;

    // Pipelines were compiled without an explicit poolEntrySize (= 0).
    // VulkanSC spec requires pipelinePoolSizeCount = 0 in that case.
    // Pool size entry for all horizon pipelines.  poolEntrySize must match
    // HorizonRenderer::kPoolEntrySize (both must be updated together).
    VkPipelinePoolSize poolSizes{};
    poolSizes.sType          = VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE;
    poolSizes.pNext          = nullptr;
    poolSizes.poolEntrySize  = sim::rendering::HorizonRenderer::kPoolEntrySize;
    poolSizes.poolEntryCount = sim::rendering::HorizonRenderer::kPipelineCount;

    const engine::VkscContextConfig ctxCfg = BuildContextConfig(pcCI, poolSizes);

    // -----------------------------------------------------------------------
    // 2. Init VulkanSC context (instance + device).
    // -----------------------------------------------------------------------
    engine::VkscContext              ctx{};
    engine::wsi::DisplayOutput       display{};
    engine::rendering::SwapchainSc   swapchain{};
    sim::rendering::HorizonRenderer  renderer{};
    engine::rendering::FrameSync     sync{};

    {
        const engine::Result r = ctx.Init(ctxCfg);
        if (!engine::IsOk(r))
        {
            engine::log::Error("main: VkscContext::Init failed.");
            engine::log::Error(engine::ResultToString(r));
            return EXIT_FAILURE;
        }
    }
    engine::log::Info("main: VulkanSC context ready.");

    // -----------------------------------------------------------------------
    // 3. Create display surface (VK_KHR_display — only WSI on the emulation ICD).
    // -----------------------------------------------------------------------
    {
        const engine::Result r = display.Init(ctx.Instance(), ctx.PhysicalDevice());
        if (!engine::IsOk(r))
        {
            engine::log::Error("main: DisplayOutput::Init failed.");
            engine::log::Error(engine::ResultToString(r));
            ShutdownAll(ctx, display, swapchain, renderer, sync);
            return EXIT_FAILURE;
        }
    }
    engine::log::Info("main: display surface created.");

    // -----------------------------------------------------------------------
    // 4. Create swapchain.
    // -----------------------------------------------------------------------
    {
        engine::rendering::SwapchainSc::Config swCfg{};
        swCfg.physDevice        = ctx.PhysicalDevice();
        swCfg.device            = ctx.Device();
        swCfg.surface           = display.Surface();
        swCfg.queueFamilyIndex  = ctx.GraphicsQueueFamily();
        swCfg.requiredFormat    = display.ColorFormat();
        swCfg.preferredWidth    = display.Width();
        swCfg.preferredHeight   = display.Height();

        const engine::Result r = swapchain.Init(swCfg);
        if (!engine::IsOk(r))
        {
            engine::log::Error("main: SwapchainSc::Init failed.");
            engine::log::Error(engine::ResultToString(r));
            ShutdownAll(ctx, display, swapchain, renderer, sync);
            return EXIT_FAILURE;
        }
    }
    engine::log::Info("main: swapchain created.");

    // -----------------------------------------------------------------------
    // 5. Build image-view array and pass to renderer.
    //    HorizonRenderer does not own the swapchain; it only holds pointers to
    //    the image views for the duration of its lifetime.
    // -----------------------------------------------------------------------
    const uint32_t imageCount = swapchain.ImageCount();
    VkImageView imageViews[engine::rendering::SwapchainSc::kMaxImages]{};
    for (uint32_t i = 0U; i < imageCount; ++i)
    {
        imageViews[i] = swapchain.ImageView(i);
    }

    {
        engine::rendering::FrameRendererConfig rendCfg{};
        rendCfg.physDevice   = ctx.PhysicalDevice();
        rendCfg.device       = ctx.Device();
        rendCfg.queueFamily  = ctx.GraphicsQueueFamily();
        rendCfg.colorFormat  = swapchain.Format();
        rendCfg.extent       = swapchain.Extent();
        rendCfg.imageViews   = imageViews;
        rendCfg.imageCount   = imageCount;

        const engine::Result r = renderer.Init(rendCfg);
        if (!engine::IsOk(r))
        {
            engine::log::Error("main: HorizonRenderer::Init failed.");
            engine::log::Error(engine::ResultToString(r));
            ShutdownAll(ctx, display, swapchain, renderer, sync);
            return EXIT_FAILURE;
        }
    }
    engine::log::Info("main: renderer ready.");

    // -----------------------------------------------------------------------
    // 6. Create synchronisation objects.
    //    FrameSync owns: imageAvailable, renderComplete (semaphores) + inFlight (fence).
    // -----------------------------------------------------------------------
    {
        const engine::Result r = sync.Init(ctx.Device());
        if (!engine::IsOk(r))
        {
            engine::log::Error("main: FrameSync::Init failed.");
            engine::log::Error(engine::ResultToString(r));
            ShutdownAll(ctx, display, swapchain, renderer, sync);
            return EXIT_FAILURE;
        }
    }
    engine::log::Info("main: synchronisation objects created.");

    // -----------------------------------------------------------------------
    // 7. Data sources and telemetry.
    // -----------------------------------------------------------------------
    sim::demo::DemoAttitudeSource  attitudeSource{};
    sim::demo::ConsoleFrameReport  frameReport{};

    // -----------------------------------------------------------------------
    // 8. Frame loop.
    // -----------------------------------------------------------------------
    engine::log::Info("main: entering frame loop.");

    // Query performance frequency once — it does not change at runtime.
    LARGE_INTEGER perfFreq{};
    QueryPerformanceFrequency(&perfFreq);

    bool loopRunning = true;
    uint64_t frameNumber = 0U;

    while (loopRunning)
    {
        // --- 8.1 Wait for the in-flight fence, then reset it. ---
        {
            const engine::Result r = sync.WaitAndReset(ctx.Device());
            if (!engine::IsOk(r))
            {
                engine::log::Error("main: WaitAndReset failed — aborting.");
                engine::log::Error(engine::ResultToString(r));
                loopRunning = false;
                break;
            }
        }

        // --- 8.2 Sample attitude data. ---
        const engine::data::AttitudeData attitude = attitudeSource.GetAttitude();

        // --- 8.3 Acquire next swapchain image. ---
        uint32_t imageIndex = 0U;
        {
            const VkResult vr = vkAcquireNextImageKHR(
                ctx.Device(),
                swapchain.Handle(),
                UINT64_MAX,
                sync.ImageAvailable(),
                VK_NULL_HANDLE,
                &imageIndex);

            if ((vr == VK_ERROR_OUT_OF_DATE_KHR) || (vr == VK_SUBOPTIMAL_KHR))
            {
                // Display resize or mode change — not expected in our fixed-mode setup.
                engine::log::Error("main: swapchain out of date — aborting.");
                loopRunning = false;
                break;
            }
            if (vr != VK_SUCCESS)
            {
                engine::log::Error("main: vkAcquireNextImageKHR failed — aborting.");
                loopRunning = false;
                break;
            }
        }

        // --- 8.4 Ask the renderer to record the frame. ---
        LARGE_INTEGER recordStart{};
        QueryPerformanceCounter(&recordStart);

        const VkCommandBuffer cmdBuf = renderer.RecordFrame(imageIndex, attitude);
        if (cmdBuf == VK_NULL_HANDLE)
        {
            engine::log::Error("main: RecordFrame returned null — aborting.");
            loopRunning = false;
            break;
        }

        LARGE_INTEGER recordEnd{};
        QueryPerformanceCounter(&recordEnd);

        // --- 8.5 Submit command buffer. ---
        //
        // Wait:   imageAvailable (image ready to write)
        // Signal: renderComplete (rendering done)
        // Signal: inFlight fence (CPU can reuse this frame's resources)
        //
        const VkPipelineStageFlags waitStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        // Store semaphore handles in locals: VkSubmitInfo takes pointers,
        // and accessor return values cannot have their address taken.
        const VkSemaphore imgAvail = sync.ImageAvailable();
        const VkSemaphore rendDone = sync.RenderComplete();

        VkSubmitInfo submitInfo{};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                = nullptr;
        submitInfo.waitSemaphoreCount   = 1U;
        submitInfo.pWaitSemaphores      = &imgAvail;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1U;
        submitInfo.pCommandBuffers      = &cmdBuf;
        submitInfo.signalSemaphoreCount = 1U;
        submitInfo.pSignalSemaphores    = &rendDone;

        {
            const VkResult vr = vkQueueSubmit(
                ctx.GraphicsQueue(), 1U, &submitInfo, sync.InFlight());
            if (vr != VK_SUCCESS)
            {
                engine::log::Error("main: vkQueueSubmit failed — aborting.");
                loopRunning = false;
                break;
            }
        }

        // --- 8.6 Present. ---
        const VkSwapchainKHR swapHandle = swapchain.Handle();
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext              = nullptr;
        presentInfo.waitSemaphoreCount = 1U;
        presentInfo.pWaitSemaphores    = &rendDone;
        presentInfo.swapchainCount     = 1U;
        presentInfo.pSwapchains        = &swapHandle;
        presentInfo.pImageIndices      = &imageIndex;
        presentInfo.pResults           = nullptr;

        bool framePresented = true;
        {
            const VkResult vr = vkQueuePresentKHR(ctx.GraphicsQueue(), &presentInfo);
            if ((vr != VK_SUCCESS) && (vr != VK_SUBOPTIMAL_KHR))
            {
                engine::log::Error("main: vkQueuePresentKHR failed — aborting.");
                framePresented = false;
                loopRunning = false;
            }
        }

        // --- 8.7 Telemetry. ---
        {
            const float renderMs =
                static_cast<float>(recordEnd.QuadPart - recordStart.QuadPart) *
                1000.0F / static_cast<float>(perfFreq.QuadPart);

            engine::data::FrameMetrics metrics{};
            metrics.frameNumber       = frameNumber;
            metrics.renderTimeMs      = renderMs;
            metrics.presentTimeMs     = 0.0F;  // not separately timed in this build
            metrics.displayedRollDeg  = attitude.valid ? attitude.rollDeg  : 0.0F;
            metrics.displayedPitchDeg = attitude.valid ? attitude.pitchDeg : 0.0F;
            metrics.framePresented    = framePresented;

            frameReport.OnFrameComplete(metrics);
        }

        ++frameNumber;

        // --- 8.8 Termination check. ---
        if ((kMaxFrames > 0U) && (frameNumber >= kMaxFrames))
        {
            engine::log::Info("main: frame limit reached, exiting cleanly.");
            loopRunning = false;
        }
    }

    engine::log::Info("main: frame loop complete.");

    // -----------------------------------------------------------------------
    // 9. Clean shutdown (reverse init order).
    // -----------------------------------------------------------------------
    ShutdownAll(ctx, display, swapchain, renderer, sync);
    engine::log::Info("main: shutdown complete.");

    return EXIT_SUCCESS;
}
