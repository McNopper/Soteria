/// @file command_pool.hpp
/// @brief Generic VulkanSC command pool and command buffer allocation.
///
/// In VulkanSC, VkCommandPoolMemoryReservationCreateInfo must be chained
/// into VkCommandPoolCreateInfo to declare the reserved command memory size.
/// Only primary command buffers are allocated; AllocateBuffers returns an
/// error on any allocation failure (never silently truncates).

#ifndef VKSC_ENGINE_RENDERING_COMMAND_POOL_HPP
#define VKSC_ENGINE_RENDERING_COMMAND_POOL_HPP

#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>
#include <cstdint>

namespace engine {
namespace rendering {

/// @brief Owns a VkCommandPool and can allocate primary command buffers.
///
/// Non-copyable, non-movable.
class CommandPool
{
public:
    CommandPool()  noexcept = default;
    ~CommandPool() noexcept = default;

    CommandPool(const CommandPool&)            = delete;
    CommandPool& operator=(const CommandPool&) = delete;
    CommandPool(CommandPool&&)                 = delete;
    CommandPool& operator=(CommandPool&&)      = delete;

    /// @brief Create the command pool.
    ///
    /// @param device             Logical device.
    /// @param queueFamilyIndex   Queue family for which commands are recorded.
    /// @param reservedSizeBytes  VkCommandPoolMemoryReservationCreateInfo.commandPoolReservedSize.
    ///                           Must match the total command-buffer reservation declared in
    ///                           VkDeviceObjectReservationCreateInfo.
    /// @param maxCommandBuffers  Maximum VkCommandBuffers this pool may allocate.
    /// @returns Result::kOk on success.
    [[nodiscard]] Result Init(VkDevice device,
                              uint32_t queueFamilyIndex,
                              VkDeviceSize reservedSizeBytes,
                              uint32_t     maxCommandBuffers) noexcept;

    /// @brief Destroy the pool (all command buffers allocated from it are freed).
    ///        Safe on partial init.
    void Shutdown(VkDevice device) noexcept;

    /// @brief Allocate @p count primary command buffers into @p outBuffers.
    ///
    /// @param device      Logical device.
    /// @param count       Number of buffers to allocate.
    /// @param outBuffers  Caller-provided array of at least @p count entries.
    /// @returns Result::kOk on success.
    [[nodiscard]] Result AllocateBuffers(VkDevice         device,
                                         uint32_t         count,
                                         VkCommandBuffer* outBuffers) const noexcept;

    /// @returns The VkCommandPool handle, or VK_NULL_HANDLE before Init.
    [[nodiscard]] VkCommandPool Handle() const noexcept { return m_pool; }

private:
    VkCommandPool m_pool{VK_NULL_HANDLE};
    bool          m_initialised{false};
};

} /* namespace rendering */
} /* namespace engine */

#endif /* VKSC_ENGINE_RENDERING_COMMAND_POOL_HPP */
