/// @file horizon_renderer.cpp
/// @brief Artificial-horizon VulkanSC renderer — implementation.
///
/// This file contains only rendering logic: command recording, pipeline
/// creation, render pass setup, and geometry upload.
/// It has no knowledge of swapchain lifecycle, semaphores, fences, or queues.

#include "horizon_renderer.hpp"
#include "pipeline_cache_data.hpp"

#include "../../engine/core/log.hpp"
#include "../../engine/rendering/swapchain.hpp"

#include <algorithm>    // std::copy
#include <cmath>

namespace sim {
namespace rendering {

// kMaxFramebuffers must accommodate the maximum swapchain image count.
static_assert(HorizonRenderer::kMaxFramebuffers >= engine::rendering::SwapchainSc::kMaxImages,
              "HorizonRenderer::kMaxFramebuffers must be >= SwapchainSc::kMaxImages");

// ---------------------------------------------------------------------------
// Public: Init
// ---------------------------------------------------------------------------

engine::Result HorizonRenderer::Init(
    const engine::rendering::FrameRendererConfig& cfg) noexcept
{
    if (m_initialised)
    {
        engine::log::Error("HorizonRenderer::Init: already initialised.");
        return engine::Result::kAlreadyInitialised;
    }
    if ((cfg.device == VK_NULL_HANDLE) || (cfg.physDevice == VK_NULL_HANDLE))
    {
        engine::log::Error("HorizonRenderer::Init: null device or physDevice.");
        return engine::Result::kInvalidArgument;
    }
    if ((cfg.imageViews == nullptr) || (cfg.imageCount == 0U))
    {
        engine::log::Error("HorizonRenderer::Init: no image views supplied.");
        return engine::Result::kInvalidArgument;
    }
    if (cfg.colorFormat == VK_FORMAT_UNDEFINED)
    {
        engine::log::Error("HorizonRenderer::Init: undefined colour format.");
        return engine::Result::kInvalidArgument;
    }

    m_extent = cfg.extent;

    engine::Result r = CreateRenderPass(cfg.device, cfg.colorFormat);
    if (!engine::IsOk(r)) { return r; }

    r = CreatePipelineLayout(cfg.device);
    if (!engine::IsOk(r)) { DestroyAll(cfg.device); return r; }

    r = m_pipelineCache.Init(cfg.device, sim::kPipelineCacheData, sim::kPipelineCacheDataSize);
    if (!engine::IsOk(r))
    {
        engine::log::Error("HorizonRenderer: PipelineCacheSc::Init failed.");
        DestroyAll(cfg.device);
        return r;
    }

    r = CreatePipeline(cfg.device, kBgUuid,   false, m_bgPipeline);
    if (!engine::IsOk(r)) { DestroyAll(cfg.device); return r; }

    r = CreatePipeline(cfg.device, kLineUuid, true,  m_linePipeline);
    if (!engine::IsOk(r)) { DestroyAll(cfg.device); return r; }

    r = CreateFramebuffers(cfg.device, cfg.imageViews, cfg.imageCount);
    if (!engine::IsOk(r)) { DestroyAll(cfg.device); return r; }

    r = m_commandPool.Init(cfg.device, cfg.queueFamily, kCmdReservedBytes, 1U);
    if (!engine::IsOk(r))
    {
        engine::log::Error("HorizonRenderer: CommandPool::Init failed.");
        DestroyAll(cfg.device);
        return r;
    }

    r = m_commandPool.AllocateBuffers(cfg.device, 1U, &m_cmdBuffer);
    if (!engine::IsOk(r))
    {
        engine::log::Error("HorizonRenderer: AllocateBuffers failed.");
        DestroyAll(cfg.device);
        return r;
    }

    r = m_vertexBuffer.Init(cfg.device, cfg.physDevice, kVBOBytes);
    if (!engine::IsOk(r))
    {
        engine::log::Error("HorizonRenderer: VertexBuffer::Init failed.");
        DestroyAll(cfg.device);
        return r;
    }

    m_initialised = true;
    engine::log::Info("HorizonRenderer: ready.");
    return engine::Result::kOk;
}

// ---------------------------------------------------------------------------
// Public: Shutdown
// ---------------------------------------------------------------------------

void HorizonRenderer::Shutdown(VkDevice device) noexcept
{
    DestroyAll(device);
}

// ---------------------------------------------------------------------------
// Public: RecordFrame
// ---------------------------------------------------------------------------

VkCommandBuffer HorizonRenderer::RecordFrame(
    uint32_t                          imageIndex,
    const engine::data::AttitudeData& attitude) noexcept
{
    if (!m_initialised)
    {
        engine::log::Error("HorizonRenderer::RecordFrame called before Init.");
        return VK_NULL_HANDLE;
    }
    if (imageIndex >= m_framebufferCount)
    {
        engine::log::Error("HorizonRenderer::RecordFrame: imageIndex out of range.");
        return VK_NULL_HANDLE;
    }

    // Use level-horizon fallback when data is not valid.
    const float rollDeg  = attitude.valid ? attitude.rollDeg  : 0.0F;
    const float pitchDeg = attitude.valid ? attitude.pitchDeg : 0.0F;

    // Update vertex buffer — host-coherent, no explicit flush needed.
    const float aspectRatio =
        (m_extent.height > 0U)
        ? (static_cast<float>(m_extent.width) / static_cast<float>(m_extent.height))
        : 1.0F;

    Vertex2D verts[kHorizonVertexCount]{};
    ComputeHorizonVertices(rollDeg, pitchDeg, aspectRatio, verts);
    const auto* const srcBytes =
        static_cast<const uint8_t*>(static_cast<const void*>(verts));
    std::copy(srcBytes, srcBytes + kVBOBytes, m_vertexBuffer.MappedBytes());

    // Begin command buffer.
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext            = nullptr;
    beginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(m_cmdBuffer, &beginInfo) != VK_SUCCESS)
    {
        engine::log::Error("HorizonRenderer: vkBeginCommandBuffer failed.");
        return VK_NULL_HANDLE;
    }

    // Set dynamic viewport and scissor.
    VkViewport viewport{};
    viewport.x        = 0.0F;
    viewport.y        = 0.0F;
    viewport.width    = static_cast<float>(m_extent.width);
    viewport.height   = static_cast<float>(m_extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(m_cmdBuffer, 0U, 1U, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_extent;
    vkCmdSetScissor(m_cmdBuffer, 0U, 1U, &scissor);

    // Begin render pass — clear to dark charcoal.
    VkClearValue clearVal{};
    clearVal.color.float32[0] = 0.05F;
    clearVal.color.float32[1] = 0.05F;
    clearVal.color.float32[2] = 0.05F;
    clearVal.color.float32[3] = 1.0F;

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.pNext           = nullptr;
    rpBegin.renderPass      = m_renderPass;
    rpBegin.framebuffer     = m_framebuffers[imageIndex];
    rpBegin.renderArea      = scissor;
    rpBegin.clearValueCount = 1U;
    rpBegin.pClearValues    = &clearVal;

    vkCmdBeginRenderPass(m_cmdBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Pre-compute shared attitude values.
    static constexpr float kDegToRad{0.01745329251994329576F};
    const float rollRad  = rollDeg * kDegToRad;
    const float cosR     = std::cos(rollRad);
    const float sinR     = std::sin(rollRad);
    const float pitchNdc = pitchDeg / 30.0F;

    // --- Draw 1: BG pipeline — fullscreen sky/earth split ---
    //
    // The vertex shader generates a fullscreen oversized triangle from gl_VertexIndex.
    // No vertex buffer is bound.  The fragment shader classifies each pixel as
    // sky (blue) or earth (brown) using the roll-unrotated Y coordinate.
    //
    // Colour fields in push constants are not used by the BG fragment shader.
    {
        HorizonPushConstants pc{};
        pc.cosR     = cosR;
        pc.sinR     = sinR;
        pc.pitchNdc = pitchNdc;
        pc.pad      = 0.0F;
        pc.r        = 0.0F;
        pc.g        = 0.0F;
        pc.b        = 0.0F;
        pc.a        = 0.0F;

        vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bgPipeline);
        vkCmdPushConstants(m_cmdBuffer, m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0U, static_cast<uint32_t>(sizeof(HorizonPushConstants)), &pc);
        vkCmdDraw(m_cmdBuffer, 3U, 1U, 0U, 0U);
    }

    // --- Draw 2: LINE pipeline — horizon symbology ---
    //
    // 94 NDC-space vertices from the vertex buffer.
    // White when data is valid; red when the attitude source has declared a failure.
    {
        HorizonPushConstants pc{};
        pc.cosR     = cosR;
        pc.sinR     = sinR;
        pc.pitchNdc = pitchNdc;
        pc.pad      = 0.0F;

        if (attitude.valid)
        {
            pc.r = 1.0F;  // white
            pc.g = 1.0F;
            pc.b = 1.0F;
            pc.a = 1.0F;
        }
        else
        {
            pc.r = 1.0F;  // red — sensor failure indication
            pc.g = 0.0F;
            pc.b = 0.0F;
            pc.a = 1.0F;
        }

        const VkBuffer vbo = m_vertexBuffer.Buffer();
        const VkDeviceSize vbOffset{0U};
        vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
        vkCmdBindVertexBuffers(m_cmdBuffer, 0U, 1U, &vbo, &vbOffset);
        vkCmdPushConstants(m_cmdBuffer, m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0U, static_cast<uint32_t>(sizeof(HorizonPushConstants)), &pc);
        vkCmdDraw(m_cmdBuffer, kHorizonVertexCount, 1U, 0U, 0U);
    }

    vkCmdEndRenderPass(m_cmdBuffer);

    if (vkEndCommandBuffer(m_cmdBuffer) != VK_SUCCESS)
    {
        engine::log::Error("HorizonRenderer: vkEndCommandBuffer failed.");
        return VK_NULL_HANDLE;
    }

    return m_cmdBuffer;
}

// ---------------------------------------------------------------------------
// Private: CreateRenderPass
// ---------------------------------------------------------------------------

engine::Result HorizonRenderer::CreateRenderPass(VkDevice device,
                                                   VkFormat colorFormat) noexcept
{
    VkAttachmentDescription att{};
    att.flags          = 0U;
    att.format         = colorFormat;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    const VkAttachmentReference colorRef{0U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.flags                   = 0U;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0U;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = 1U;
    subpass.pColorAttachments       = &colorRef;
    subpass.pResolveAttachments     = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0U;
    subpass.pPreserveAttachments    = nullptr;

    VkSubpassDependency dep{};
    dep.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass      = 0U;
    dep.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask   = 0U;
    dep.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dep.dependencyFlags = 0U;

    VkRenderPassCreateInfo rpCI{};
    rpCI.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.pNext           = nullptr;
    rpCI.flags           = 0U;
    rpCI.attachmentCount = 1U;
    rpCI.pAttachments    = &att;
    rpCI.subpassCount    = 1U;
    rpCI.pSubpasses      = &subpass;
    rpCI.dependencyCount = 1U;
    rpCI.pDependencies   = &dep;

    if (vkCreateRenderPass(device, &rpCI, nullptr, &m_renderPass) != VK_SUCCESS)
    {
        engine::log::Error("HorizonRenderer: vkCreateRenderPass failed.");
        return engine::Result::kVkscRenderPassFailed;
    }
    return engine::Result::kOk;
}

// ---------------------------------------------------------------------------
// Private: CreatePipelineLayout
// ---------------------------------------------------------------------------

engine::Result HorizonRenderer::CreatePipelineLayout(VkDevice device) noexcept
{
    const VkPushConstantRange pcRange{
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0U,
        static_cast<uint32_t>(sizeof(HorizonPushConstants))
    };

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.pNext                  = nullptr;
    layoutCI.flags                  = 0U;
    layoutCI.setLayoutCount         = 0U;
    layoutCI.pSetLayouts            = nullptr;
    layoutCI.pushConstantRangeCount = 1U;
    layoutCI.pPushConstantRanges    = &pcRange;

    if (vkCreatePipelineLayout(device, &layoutCI, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        engine::log::Error("HorizonRenderer: vkCreatePipelineLayout failed.");
        return engine::Result::kVkscPipelineLayoutFailed;
    }
    return engine::Result::kOk;
}

// ---------------------------------------------------------------------------
// Private: CreatePipeline
// ---------------------------------------------------------------------------

engine::Result HorizonRenderer::CreatePipeline(
    VkDevice       device,
    const uint8_t  uuid[VK_UUID_SIZE],
    bool           hasVertexInput,
    VkPipeline&    outPipeline) noexcept
{
    // In VulkanSC shaders live in the pipeline cache; module must be VK_NULL_HANDLE.
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = VK_NULL_HANDLE;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = VK_NULL_HANDLE;
    stages[1].pName  = "main";

    // Vertex input — used only by the LINE pipeline (binding 0, vec2, stride 8).
    const VkVertexInputBindingDescription binding{
        0U, static_cast<uint32_t>(sizeof(Vertex2D)), VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription attrib{
        0U, 0U, VK_FORMAT_R32G32_SFLOAT, 0U};

    VkPipelineVertexInputStateCreateInfo viCI{};
    viCI.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viCI.vertexBindingDescriptionCount   = hasVertexInput ? 1U : 0U;
    viCI.pVertexBindingDescriptions      = hasVertexInput ? &binding : nullptr;
    viCI.vertexAttributeDescriptionCount = hasVertexInput ? 1U : 0U;
    viCI.pVertexAttributeDescriptions    = hasVertexInput ? &attrib : nullptr;

    VkPipelineInputAssemblyStateCreateInfo iaCI{};
    iaCI.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaCI.topology = hasVertexInput
                    ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                    : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaCI.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor are dynamic; only counts are needed here.
    VkPipelineViewportStateCreateInfo vpCI{};
    vpCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpCI.viewportCount = 1U;
    vpCI.pViewports    = nullptr;
    vpCI.scissorCount  = 1U;
    vpCI.pScissors     = nullptr;

    VkPipelineRasterizationStateCreateInfo rastCI{};
    rastCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastCI.polygonMode = VK_POLYGON_MODE_FILL;
    rastCI.cullMode    = VK_CULL_MODE_NONE;
    rastCI.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rastCI.lineWidth   = 1.0F;

    VkPipelineMultisampleStateCreateInfo msCI{};
    msCI.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable    = VK_FALSE;
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbCI{};
    cbCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbCI.attachmentCount = 1U;
    cbCI.pAttachments    = &blendAtt;

    static constexpr VkDynamicState kDynStates[2U]{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynCI{};
    dynCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynCI.dynamicStateCount = 2U;
    dynCI.pDynamicStates    = kDynStates;

    // VkPipelineOfflineCreateInfo — matches pre-compiled pipeline by UUID.
    VkPipelineOfflineCreateInfo offlineCI{};
    offlineCI.sType        = VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO;
    offlineCI.pNext        = nullptr;
    std::copy(uuid, uuid + VK_UUID_SIZE, offlineCI.pipelineIdentifier);
    offlineCI.matchControl  = VK_PIPELINE_MATCH_CONTROL_APPLICATION_UUID_EXACT_MATCH;
    offlineCI.poolEntrySize = kPoolEntrySize;

    VkGraphicsPipelineCreateInfo pipeCI{};
    pipeCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeCI.pNext               = &offlineCI;   // VulkanSC mandatory chain
    pipeCI.stageCount          = 2U;
    pipeCI.pStages             = stages;
    pipeCI.pVertexInputState   = &viCI;
    pipeCI.pInputAssemblyState = &iaCI;
    pipeCI.pTessellationState  = nullptr;
    pipeCI.pViewportState      = &vpCI;
    pipeCI.pRasterizationState = &rastCI;
    pipeCI.pMultisampleState   = &msCI;
    pipeCI.pDepthStencilState  = nullptr;
    pipeCI.pColorBlendState    = &cbCI;
    pipeCI.pDynamicState       = &dynCI;
    pipeCI.layout              = m_pipelineLayout;
    pipeCI.renderPass          = m_renderPass;
    pipeCI.subpass             = 0U;
    pipeCI.basePipelineHandle  = VK_NULL_HANDLE;
    pipeCI.basePipelineIndex   = -1;

    if (vkCreateGraphicsPipelines(device, m_pipelineCache.Handle(),
                                  1U, &pipeCI, nullptr, &outPipeline) != VK_SUCCESS)
    {
        engine::log::Error("HorizonRenderer: vkCreateGraphicsPipelines failed.");
        return engine::Result::kVkscPipelineFailed;
    }
    return engine::Result::kOk;
}

// ---------------------------------------------------------------------------
// Private: CreateFramebuffers
// ---------------------------------------------------------------------------

engine::Result HorizonRenderer::CreateFramebuffers(VkDevice           device,
                                                     const VkImageView* imageViews,
                                                     uint32_t           imageCount) noexcept
{
    for (uint32_t i = 0U; i < imageCount; ++i)
    {
        VkFramebufferCreateInfo fbCI{};
        fbCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCI.renderPass      = m_renderPass;
        fbCI.attachmentCount = 1U;
        fbCI.pAttachments    = &imageViews[i];
        fbCI.width           = m_extent.width;
        fbCI.height          = m_extent.height;
        fbCI.layers          = 1U;

        if (vkCreateFramebuffer(device, &fbCI, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
        {
            engine::log::Error("HorizonRenderer: vkCreateFramebuffer failed.");
            // Clean up framebuffers already created.
            for (uint32_t j = 0U; j < i; ++j)
            {
                vkDestroyFramebuffer(device, m_framebuffers[j], nullptr);
                m_framebuffers[j] = VK_NULL_HANDLE;
            }
            return engine::Result::kVkscFramebufferFailed;
        }
    }

    m_framebufferCount = imageCount;
    return engine::Result::kOk;
}

// ---------------------------------------------------------------------------
// Private: DestroyAll
// ---------------------------------------------------------------------------

void HorizonRenderer::DestroyAll(VkDevice device) noexcept
{
    if (device == VK_NULL_HANDLE) { return; }

    m_vertexBuffer.Shutdown(device);

    // Command buffers are freed implicitly when the command pool is destroyed.
    m_cmdBuffer = VK_NULL_HANDLE;
    m_commandPool.Shutdown(device);

    for (uint32_t i = 0U; i < m_framebufferCount; ++i)
    {
        if (m_framebuffers[i] != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, m_framebuffers[i], nullptr);
            m_framebuffers[i] = VK_NULL_HANDLE;
        }
    }
    m_framebufferCount = 0U;

    if (m_linePipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_linePipeline, nullptr);
        m_linePipeline = VK_NULL_HANDLE;
    }
    if (m_bgPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_bgPipeline, nullptr);
        m_bgPipeline = VK_NULL_HANDLE;
    }

    m_pipelineCache.Shutdown(device);

    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    m_initialised = false;
    engine::log::Info("HorizonRenderer: destroyed.");
}

} /* namespace rendering */
} /* namespace sim */
