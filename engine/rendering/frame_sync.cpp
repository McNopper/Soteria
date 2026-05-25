/// @file frame_sync.cpp
/// @brief Per-frame synchronisation implementation.

#include "frame_sync.hpp"
#include "../core/log.hpp"

#include <cstdint>

namespace engine {
namespace rendering {

Result FrameSync::Init(VkDevice device) noexcept
{
    if (m_initialised)
    {
        log::Error("FrameSync::Init called on already-initialised object.");
        return Result::kAlreadyInitialised;
    }
    if (device == VK_NULL_HANDLE)
    {
        log::Error("FrameSync::Init: null device.");
        return Result::kInvalidArgument;
    }

    const VkSemaphoreCreateInfo semCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0U};

    VkResult r = vkCreateSemaphore(device, &semCI, nullptr, &m_imageAvailable);
    if (r != VK_SUCCESS)
    {
        log::Error("FrameSync: vkCreateSemaphore (imageAvailable) failed.");
        return Result::kVkscSyncFailed;
    }

    r = vkCreateSemaphore(device, &semCI, nullptr, &m_renderComplete);
    if (r != VK_SUCCESS)
    {
        log::Error("FrameSync: vkCreateSemaphore (renderComplete) failed.");
        vkDestroySemaphore(device, m_imageAvailable, nullptr);
        m_imageAvailable = VK_NULL_HANDLE;
        return Result::kVkscSyncFailed;
    }

    // Create fence in the signalled state so the first WaitAndReset succeeds.
    const VkFenceCreateInfo fenceCI{
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};

    r = vkCreateFence(device, &fenceCI, nullptr, &m_inFlight);
    if (r != VK_SUCCESS)
    {
        log::Error("FrameSync: vkCreateFence failed.");
        vkDestroySemaphore(device, m_renderComplete,  nullptr);
        vkDestroySemaphore(device, m_imageAvailable, nullptr);
        m_renderComplete  = VK_NULL_HANDLE;
        m_imageAvailable = VK_NULL_HANDLE;
        return Result::kVkscSyncFailed;
    }

    m_initialised = true;
    log::Info("FrameSync: sync objects created.");
    return Result::kOk;
}

void FrameSync::Shutdown(VkDevice device) noexcept
{
    if (!m_initialised) { return; }

    if (m_inFlight != VK_NULL_HANDLE)
    {
        vkDestroyFence(device, m_inFlight, nullptr);
        m_inFlight = VK_NULL_HANDLE;
    }
    if (m_renderComplete != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(device, m_renderComplete, nullptr);
        m_renderComplete = VK_NULL_HANDLE;
    }
    if (m_imageAvailable != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(device, m_imageAvailable, nullptr);
        m_imageAvailable = VK_NULL_HANDLE;
    }

    m_initialised = false;
    log::Info("FrameSync: sync objects destroyed.");
}

Result FrameSync::WaitAndReset(VkDevice device) noexcept
{
    const VkResult r = vkWaitForFences(device, 1U, &m_inFlight,
                                       VK_TRUE, kFenceTimeoutNs);
    if (r == VK_TIMEOUT)
    {
        log::Error("FrameSync: vkWaitForFences timed out (possible GPU hang).");
        return Result::kVkscTimeoutFailed;
    }
    if (r != VK_SUCCESS)
    {
        log::Error("FrameSync: vkWaitForFences failed.");
        return Result::kError;
    }

    const VkResult rr = vkResetFences(device, 1U, &m_inFlight);
    if (rr != VK_SUCCESS)
    {
        log::Error("FrameSync: vkResetFences failed.");
        return Result::kError;
    }

    return Result::kOk;
}

} /* namespace rendering */
} /* namespace engine */
