/// @file i_frame_renderer.hpp
/// @brief Generic interface for a single-pass frame renderer.
///
/// A frame renderer has one responsibility: given a target framebuffer index,
/// produce a recorded VkCommandBuffer ready for the caller to submit.
///
/// The renderer owns:
///   - Render pass  (defines attachment format / load-store ops)
///   - Pipelines    (pre-compiled, loaded from pipeline cache)
///   - Framebuffers (created from image views supplied at Init time)
///   - Command pool / command buffer
///   - Any geometry or constant data it writes
///
/// The renderer does NOT own or know about:
///   - Swapchain lifecycle (create / destroy / resize)
///   - Acquire / present operations
///   - Synchronisation primitives (semaphores, fences)
///   - Submission queues
///   - Any other engine's display pipeline
///
/// This separation means a IFrameRenderer implementation can be dropped into
/// any display engine that manages its own acquire/present/sync cycle.
/// The caller acquires an image, passes the index to RecordFrame, receives
/// a command buffer, submits with its own synchronisation, and presents.
///
/// @satisfies SWS_RENDER_100  IFrameRenderer decouples rendering from presentation.
/// @satisfies SWS_RENDER_101  RecordFrame must be called only after the caller has
///                            confirmed the GPU is not reading from the target framebuffer.

#ifndef VKSC_ENGINE_RENDERING_I_FRAME_RENDERER_HPP
#define VKSC_ENGINE_RENDERING_I_FRAME_RENDERER_HPP

#include "../data/i_attitude_source.hpp"
#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>
#include <cstdint>

namespace engine {
namespace rendering {

/// @brief Configuration passed to IFrameRenderer::Init.
struct FrameRendererConfig
{
    VkPhysicalDevice  physDevice{VK_NULL_HANDLE};   ///< For memory type queries.
    VkDevice          device{VK_NULL_HANDLE};        ///< Logical device.
    uint32_t          queueFamily{0U};               ///< Graphics queue family.
    VkFormat          colorFormat{VK_FORMAT_UNDEFINED}; ///< Must match image views.
    VkExtent2D        extent{};                      ///< Fixed display resolution.

    /// Image views to render into — one per swapchain image.
    /// Ownership is NOT transferred; views must remain valid for the renderer lifetime.
    const VkImageView* imageViews{nullptr};
    uint32_t           imageCount{0U};
};

/// @brief Abstract single-pass frame renderer.
///
/// Implementations are non-copyable and non-movable.
class IFrameRenderer
{
public:
    /// @brief Initialise all GPU objects needed to render frames.
    ///
    /// @param cfg  Renderer configuration.
    /// @returns Result::kOk on success; an error code on any failure.
    ///          On failure the implementation must release all GPU objects it
    ///          has already created before returning.
    [[nodiscard]] virtual Result Init(const FrameRendererConfig& cfg) noexcept = 0;

    /// @brief Destroy all owned GPU objects.  Safe to call in any state.
    virtual void Shutdown(VkDevice device) noexcept = 0;

    /// @brief Record draw commands for one frame.
    ///
    /// The caller MUST guarantee that the GPU has finished reading from
    /// framebuffer[imageIndex] before calling this function (i.e. the
    /// in-flight fence for that image has been signalled and reset).
    ///
    /// The returned VkCommandBuffer:
    ///   - Is already begun and ended.
    ///   - Is valid until the next call to RecordFrame with the same imageIndex
    ///     OR until Shutdown is called (whichever comes first).
    ///   - Must be submitted exactly once before the next RecordFrame call.
    ///
    /// On failure VK_NULL_HANDLE is returned; the caller must abort the frame
    /// and initiate a safe shutdown.
    ///
    /// @param imageIndex  Index into the image view array supplied at Init.
    /// @param attitude    Attitude data to render.  If attitude.valid == false
    ///                    the renderer must display a failure indication.
    /// @returns Ready-to-submit VkCommandBuffer, or VK_NULL_HANDLE on error.
    [[nodiscard]] virtual VkCommandBuffer RecordFrame(
        uint32_t                          imageIndex,
        const data::AttitudeData&         attitude) noexcept = 0;

    IFrameRenderer()                    noexcept = default;
    virtual ~IFrameRenderer()                     noexcept = default;
    IFrameRenderer(const IFrameRenderer&)                  = delete;
    IFrameRenderer& operator=(const IFrameRenderer&)       = delete;
    IFrameRenderer(IFrameRenderer&&)                       = delete;
    IFrameRenderer& operator=(IFrameRenderer&&)            = delete;
};

} /* namespace rendering */
} /* namespace engine */

#endif /* VKSC_ENGINE_RENDERING_I_FRAME_RENDERER_HPP */
