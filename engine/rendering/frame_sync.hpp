/// @file frame_sync.hpp
/// @brief Generic per-frame synchronisation: imageAvailable + renderComplete
///        semaphores and one in-flight fence.
///
/// The fence is created in the signalled state so the first WaitAndReset()
/// returns immediately.  Fence waits time out after 5 seconds; a timeout
/// returns Result::kVkscTimeoutFailed (possible GPU hang).

#ifndef VKSC_ENGINE_RENDERING_FRAME_SYNC_HPP
#define VKSC_ENGINE_RENDERING_FRAME_SYNC_HPP

#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>
#include <cstdint>

namespace engine {
namespace rendering {

/// @brief Per-frame synchronisation primitives.
///
/// Lifecycle: Init -> (WaitAndReset, then use semaphore/fence handles) -> Shutdown.
/// Non-copyable, non-movable.
class FrameSync
{
public:
    FrameSync()  noexcept = default;
    ~FrameSync() noexcept = default;

    FrameSync(const FrameSync&)            = delete;
    FrameSync& operator=(const FrameSync&) = delete;
    FrameSync(FrameSync&&)                 = delete;
    FrameSync& operator=(FrameSync&&)      = delete;

    /// @brief Create the two semaphores and one fence (pre-signalled).
    ///
    /// The fence is created in the signalled state so the first WaitAndReset()
    /// call returns immediately.
    ///
    /// @param device  Logical device.
    /// @returns Result::kOk on success.
    [[nodiscard]] Result Init(VkDevice device) noexcept;

    /// @brief Destroy all sync objects.  Safe on partial init.
    void Shutdown(VkDevice device) noexcept;

    /// @brief Wait for the in-flight fence, then reset it for the next frame.
    ///
    /// @param device   Logical device.
    /// @returns Result::kOk, kVkscTimeoutFailed, or kError on Vulkan error.
    [[nodiscard]] Result WaitAndReset(VkDevice device) noexcept;

    [[nodiscard]] VkSemaphore ImageAvailable() const noexcept { return m_imageAvailable; }
    [[nodiscard]] VkSemaphore RenderComplete() const noexcept { return m_renderComplete; }
    [[nodiscard]] VkFence     InFlight()       const noexcept { return m_inFlight; }

private:
    static constexpr uint64_t kFenceTimeoutNs{5'000'000'000ULL};  ///< 5-second GPU timeout.

    VkSemaphore m_imageAvailable{VK_NULL_HANDLE};
    VkSemaphore m_renderComplete{VK_NULL_HANDLE};
    VkFence     m_inFlight{VK_NULL_HANDLE};
    bool        m_initialised{false};
};

} /* namespace rendering */
} /* namespace engine */

#endif /* VKSC_ENGINE_RENDERING_FRAME_SYNC_HPP */
