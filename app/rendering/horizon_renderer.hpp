/// @file horizon_renderer.hpp
/// @brief Artificial-horizon VulkanSC renderer — simulation example.
///
/// HorizonRenderer implements IFrameRenderer.  Its only job is:
///
///   Given an image index and attitude data,
///   record VulkanSC commands and return a ready-to-submit command buffer.
///
/// It knows nothing about swapchain lifecycle, acquire/present operations,
/// synchronisation primitives, or submission queues.  The simulation main.cpp
/// orchestrator owns all of those concerns.  This separation means the same
/// renderer could be driven by any display engine that exposes a
/// "here is an image view, draw into it" contract.
///
/// Objects owned by HorizonRenderer
/// ---------------------------------
///   VkRenderPass        — defines the single colour attachment
///   VkPipelineLayout    — push constant range (32 bytes)
///   PipelineCacheSc     — loaded from the embedded binary
///   VkPipeline[2]       — BG (TRIANGLE_LIST) and LINE (LINE_LIST)
///   VkFramebuffer[N]    — one per image view supplied at Init
///   CommandPool         — VulkanSC pool with memory reservation
///   VkCommandBuffer     — pre-allocated, re-recorded each frame
///   VertexBuffer        — host-coherent, persistently mapped, 94 × 8 B
///
/// Push constant layout (32 bytes, both pipelines):
/// @code
///   offset  0  float cosR        cos(rollRad)
///   offset  4  float sinR        sin(rollRad)
///   offset  8  float pitchNdc    pitchDeg / 30.0
///   offset 12  float pad         (unused, alignment)
///   offset 16  float r           line colour R
///   offset 20  float g           line colour G
///   offset 24  float b           line colour B
///   offset 28  float a           line colour A
/// @endcode
///
/// @satisfies SWS_HORIZON_020  HorizonRenderer owns all VulkanSC handles.
/// @satisfies SWS_HORIZON_021  Init cleans up all handles on any failure path.
/// @satisfies SWS_HORIZON_022  RecordFrame returns VK_NULL_HANDLE on any error.
/// @satisfies SWS_HORIZON_023  HorizonRenderer has no knowledge of presentation.

#ifndef VKSC_SIM_RENDERING_HORIZON_RENDERER_HPP
#define VKSC_SIM_RENDERING_HORIZON_RENDERER_HPP

#include "../../engine/rendering/i_frame_renderer.hpp"
#include "../../engine/rendering/pipeline_cache.hpp"
#include "../../engine/rendering/vertex_buffer.hpp"
#include "../../engine/rendering/command_pool.hpp"

#include "horizon_geometry.hpp"

#include <vulkan/vulkan_sc.h>
#include <cstdint>

namespace sim {
namespace rendering {

/// @brief Push constant data written before each draw call.
///
/// Must be exactly 32 bytes — matches the push constant range declared in
/// both pipeline JSONs and in the runtime VkPipelineLayout.
struct HorizonPushConstants
{
    float cosR{1.0F};      ///< cos(rollRad)
    float sinR{0.0F};      ///< sin(rollRad)
    float pitchNdc{0.0F};  ///< pitchDeg / 30.0
    float pad{0.0F};       ///< unused
    float r{1.0F};         ///< colour red
    float g{1.0F};         ///< colour green
    float b{1.0F};         ///< colour blue
    float a{1.0F};         ///< colour alpha
};
static_assert(sizeof(HorizonPushConstants) == 32U,
              "HorizonPushConstants must be exactly 32 bytes");

/// @brief VulkanSC frame renderer for the avionics artificial horizon.
///
/// Implements engine::rendering::IFrameRenderer.
/// Non-copyable, non-movable.
class HorizonRenderer final : public engine::rendering::IFrameRenderer
{
public:
    HorizonRenderer()  noexcept = default;
    ~HorizonRenderer() noexcept override = default;

    /// Pool entry size that must be passed in VkPipelineOfflineCreateInfo::poolEntrySize
    /// for every pipeline created by this renderer, AND declared as a matching
    /// VkPipelinePoolSize entry in VkDeviceObjectReservationCreateInfo at device creation.
    ///
    /// Value: 8 KiB — well above the largest compiled pipeline (BG = 3656 B, LINE = 1792 B).
    static constexpr VkDeviceSize kPoolEntrySize{8192U};

    /// Number of pipelines that share kPoolEntrySize (BG + LINE).
    static constexpr uint32_t kPipelineCount{2U};

    /// Maximum swapchain images this renderer supports.
    /// Must be >= engine::rendering::SwapchainSc::kMaxImages — enforced by
    /// static_assert in horizon_renderer.cpp.
    static constexpr uint32_t kMaxFramebuffers{3U};

    HorizonRenderer(const HorizonRenderer&)            = delete;
    HorizonRenderer& operator=(const HorizonRenderer&) = delete;
    HorizonRenderer(HorizonRenderer&&)                 = delete;
    HorizonRenderer& operator=(HorizonRenderer&&)      = delete;

    /// @brief Initialise all VulkanSC objects.
    ///
    /// Creates (in order): render pass → pipeline layout → pipeline cache →
    /// BG pipeline → LINE pipeline → framebuffers → command pool →
    /// command buffer → vertex buffer.
    ///
    /// On any failure every object already created is destroyed before returning.
    ///
    /// @param cfg  See FrameRendererConfig.  cfg.imageViews and cfg.imageCount
    ///             must point to the swapchain's image views for the duration of
    ///             the renderer's lifetime.
    /// @returns Result::kOk on success.
    [[nodiscard]] engine::Result Init(
        const engine::rendering::FrameRendererConfig& cfg) noexcept override;

    /// @brief Destroy all owned VulkanSC handles.  Safe on partial init.
    void Shutdown(VkDevice device) noexcept override;

    /// @brief Record draw commands for one artificial-horizon frame.
    ///
    /// Caller guarantees:
    ///   - The GPU has finished reading framebuffer[imageIndex]
    ///     (fence waited and reset before this call).
    ///   - This function is not called concurrently.
    ///
    /// Returns the internally-owned command buffer, already begun and ended,
    /// ready for the caller to submit.  Returns VK_NULL_HANDLE on any error.
    ///
    /// @param imageIndex  Swapchain image index (from vkAcquireNextImageKHR).
    /// @param attitude    Current attitude.  If valid == false a red failure
    ///                    indication is rendered instead of the normal display.
    [[nodiscard]] VkCommandBuffer RecordFrame(
        uint32_t                          imageIndex,
        const engine::data::AttitudeData& attitude) noexcept override;

private:
    /// Pipeline UUIDs — must match PipelineUUID in the pipeline JSON files.
    static constexpr uint8_t kBgUuid[VK_UUID_SIZE] =
        {161U, 162U, 163U, 164U, 165U, 166U, 167U, 168U,
           0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U};
    static constexpr uint8_t kLineUuid[VK_UUID_SIZE] =
        {177U, 178U, 179U, 180U, 181U, 182U, 183U, 184U,
           0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U};

    /// Command buffer memory reservation (128 KiB; generous for 2 draw calls).
    static constexpr VkDeviceSize kCmdReservedBytes{131072U};

    /// Vertex buffer size: 94 vertices × sizeof(Vertex2D) = 94 × 8 B.
    static constexpr uint32_t kVBOBytes{
        kHorizonVertexCount * static_cast<uint32_t>(sizeof(Vertex2D))};

    // --- Engine primitives (generic, reusable) ---
    engine::rendering::PipelineCacheSc m_pipelineCache{};
    engine::rendering::VertexBuffer    m_vertexBuffer{};
    engine::rendering::CommandPool     m_commandPool{};

    // --- Horizon-specific VulkanSC handles ---
    VkRenderPass     m_renderPass{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline       m_bgPipeline{VK_NULL_HANDLE};
    VkPipeline       m_linePipeline{VK_NULL_HANDLE};
    VkCommandBuffer  m_cmdBuffer{VK_NULL_HANDLE};

    /// Must be >= SwapchainSc::kMaxImages (currently 3). Static assertion in .cpp.
    // kMaxFramebuffers is now public — see above.
    VkFramebuffer    m_framebuffers[kMaxFramebuffers]{};
    uint32_t         m_framebufferCount{0U};

    /// Stored at Init for use in RecordFrame.
    VkExtent2D       m_extent{};

    bool m_initialised{false};

    // --- Private helpers ---
    [[nodiscard]] engine::Result CreateRenderPass(VkDevice device,
                                                   VkFormat colorFormat) noexcept;

    [[nodiscard]] engine::Result CreatePipelineLayout(VkDevice device) noexcept;

    /// @param hasVertexInput  true for the LINE pipeline (binding 0, vec2 stride 8).
    [[nodiscard]] engine::Result CreatePipeline(VkDevice       device,
                                                 const uint8_t  uuid[VK_UUID_SIZE],
                                                 bool           hasVertexInput,
                                                 VkPipeline&    outPipeline) noexcept;

    [[nodiscard]] engine::Result CreateFramebuffers(VkDevice           device,
                                                     const VkImageView* imageViews,
                                                     uint32_t           imageCount) noexcept;

    /// @brief Destroy everything in reverse creation order.  Idempotent.
    void DestroyAll(VkDevice device) noexcept;
};

} /* namespace rendering */
} /* namespace sim */

#endif /* VKSC_SIM_RENDERING_HORIZON_RENDERER_HPP */
