/// @file command_pool.cpp
/// @brief VulkanSC command pool lifecycle implementation.

#include "command_pool.hpp"
#include "../core/log.hpp"

#include <cstdint>

namespace engine {
namespace rendering {

Result CommandPool::Init(VkDevice device,
                          uint32_t queueFamilyIndex,
                          VkDeviceSize reservedSizeBytes,
                          uint32_t     maxCommandBuffers) noexcept
{
    if (m_initialised)
    {
        log::Error("CommandPool::Init called on already-initialised object.");
        return Result::kAlreadyInitialised;
    }
    if (device == VK_NULL_HANDLE)
    {
        log::Error("CommandPool::Init: null device.");
        return Result::kInvalidArgument;
    }

    // VulkanSC requires VkCommandPoolMemoryReservationCreateInfo to declare
    // the reserved command-buffer memory size upfront.
    VkCommandPoolMemoryReservationCreateInfo memResCI{};
    memResCI.sType                    = VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_RESERVATION_CREATE_INFO;
    memResCI.pNext                    = nullptr;
    memResCI.commandPoolReservedSize  = reservedSizeBytes;
    memResCI.commandPoolMaxCommandBuffers = maxCommandBuffers;

    VkCommandPoolCreateInfo poolCI{};
    poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCI.pNext            = &memResCI;
    poolCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCI.queueFamilyIndex = queueFamilyIndex;

    const VkResult r = vkCreateCommandPool(device, &poolCI, nullptr, &m_pool);
    if (r != VK_SUCCESS)
    {
        log::Error("CommandPool: vkCreateCommandPool failed.");
        return Result::kVkscCommandPoolFailed;
    }

    m_initialised = true;
    log::Info("CommandPool: created.");
    return Result::kOk;
}

void CommandPool::Shutdown(VkDevice device) noexcept
{
    if (!m_initialised) { return; }

    if (m_pool != VK_NULL_HANDLE)
    {
        // VulkanSC does not have vkDestroyCommandPool.
        // Reset the pool to release command buffer memory back to the reserved pool.
        (void)vkResetCommandPool(device, m_pool, 0U);
        m_pool = VK_NULL_HANDLE;
        log::Info("CommandPool: reset.");
    }

    m_initialised = false;
}

Result CommandPool::AllocateBuffers(VkDevice device, uint32_t count,
                                     VkCommandBuffer* outBuffers) const noexcept
{
    if ((device == VK_NULL_HANDLE) || (count == 0U) || (outBuffers == nullptr))
    {
        log::Error("CommandPool::AllocateBuffers: invalid arguments.");
        return Result::kInvalidArgument;
    }
    if (!m_initialised)
    {
        log::Error("CommandPool::AllocateBuffers called before Init.");
        return Result::kError;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext              = nullptr;
    allocInfo.commandPool        = m_pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    const VkResult r = vkAllocateCommandBuffers(device, &allocInfo, outBuffers);
    if (r != VK_SUCCESS)
    {
        log::Error("CommandPool: vkAllocateCommandBuffers failed.");
        return Result::kVkscCommandPoolFailed;
    }

    return Result::kOk;
}

} /* namespace rendering */
} /* namespace engine */
