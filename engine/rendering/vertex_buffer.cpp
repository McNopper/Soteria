/// @file vertex_buffer.cpp
/// @brief Host-visible vertex buffer lifecycle implementation.

#include "vertex_buffer.hpp"
#include "../core/log.hpp"
#include "../core/safety_macros.hpp"

#include <algorithm>
#include <cstdint>

namespace engine {
namespace rendering {

// ---- Init -------------------------------------------------------------------

Result VertexBuffer::Init(VkDevice device, VkPhysicalDevice physDevice,
                          uint32_t sizeBytes) noexcept
{
    if (m_initialised)
    {
        log::Error("VertexBuffer::Init called on already-initialised object.");
        return Result::kAlreadyInitialised;
    }
    if ((device == VK_NULL_HANDLE) || (physDevice == VK_NULL_HANDLE) || (sizeBytes == 0U))
    {
        log::Error("VertexBuffer::Init: null device/physDevice or zero size.");
        return Result::kInvalidArgument;
    }

    // ---- Create buffer ------------------------------------------------------
    VkBufferCreateInfo bufferCI{};
    bufferCI.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.pNext                 = nullptr;
    bufferCI.flags                 = 0U;
    bufferCI.size                  = static_cast<VkDeviceSize>(sizeBytes);
    bufferCI.usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferCI.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    bufferCI.queueFamilyIndexCount = 0U;
    bufferCI.pQueueFamilyIndices   = nullptr;

    VkResult r = vkCreateBuffer(device, &bufferCI, nullptr, &m_buffer);
    if (r != VK_SUCCESS)
    {
        log::Error("VertexBuffer: vkCreateBuffer failed.");
        return Result::kVkscBufferFailed;
    }

    // ---- Query memory requirements -----------------------------------------
    VkMemoryRequirements memReqs{};
    vkGetBufferMemoryRequirements(device, m_buffer, &memReqs);

    // ---- Find host-visible, host-coherent memory type ----------------------
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

    static constexpr uint32_t kRequiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // VulkanSC spec guarantees memoryTypeCount <= VK_MAX_MEMORY_TYPES (== 32).
    // Therefore mt < 32 always holds, and (1U << mt) never exceeds uint32_t width.
    static_assert(VK_MAX_MEMORY_TYPES <= 32U,
                  "memoryTypeBits shift would exceed uint32_t width — "
                  "review if VK_MAX_MEMORY_TYPES ever grows beyond 32");

    uint32_t memTypeIdx{0xFFFFFFFFU};
    for (uint32_t mt{0U}; mt < memProps.memoryTypeCount; ++mt)
    {
        const bool typeBitSet =
            ((memReqs.memoryTypeBits & (1U << mt)) != 0U);
        const bool flagsMatch =
            ((memProps.memoryTypes[mt].propertyFlags & kRequiredFlags) == kRequiredFlags);

        if (typeBitSet && flagsMatch)
        {
            memTypeIdx = mt;
            break;
        }
    }

    if (memTypeIdx == 0xFFFFFFFFU)
    {
        log::Error("VertexBuffer: no suitable host-visible memory type found.");
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return Result::kVkscBufferFailed;
    }

    // ---- Allocate memory ---------------------------------------------------
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext           = nullptr;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIdx;

    r = vkAllocateMemory(device, &allocInfo, nullptr, &m_memory);
    if (r != VK_SUCCESS)
    {
        log::Error("VertexBuffer: vkAllocateMemory failed.");
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return Result::kVkscBufferFailed;
    }

    r = vkBindBufferMemory(device, m_buffer, m_memory, 0ULL);
    if (r != VK_SUCCESS)
    {
        log::Error("VertexBuffer: vkBindBufferMemory failed.");
        // In VulkanSC, vkFreeMemory does not exist; memory is freed by vkDestroyDevice.
        m_memory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return Result::kVkscBufferFailed;
    }

    // ---- Persistent map -----------------------------------------------------
    r = vkMapMemory(device, m_memory, 0ULL,
                    static_cast<VkDeviceSize>(sizeBytes), 0U, &m_mapped);
    if (r != VK_SUCCESS)
    {
        log::Error("VertexBuffer: vkMapMemory failed.");
        // In VulkanSC, vkFreeMemory does not exist; memory is freed by vkDestroyDevice.
        m_memory = VK_NULL_HANDLE;
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        return Result::kVkscBufferFailed;
    }

    m_sizeBytes   = sizeBytes;
    m_initialised = true;
    log::Info("VertexBuffer: created.");
    return Result::kOk;
}

// ---- Write ------------------------------------------------------------------

Result VertexBuffer::Write(const void* const data, const uint32_t bytes) const noexcept
{
    if (!m_initialised)
    {
        log::Error("VertexBuffer::Write called before Init.");
        return Result::kError;
    }
    if ((data == nullptr) || (bytes == 0U) || (bytes > m_sizeBytes))
    {
        log::Error("VertexBuffer::Write: null data, zero bytes, or size overflow.");
        return Result::kInvalidArgument;
    }

    // vkMapMemory returns void*; casting it to a byte pointer to write into the
    // mapped region is unavoidable with the Vulkan SC API.  This is the single
    // deviation point — callers use Write() and need no cast of their own.
    MISRA_DEVIATION("Rule 8.2.6",
                    "Cast from pointer-to-void to pointer-to-uint8_t is required "
                    "to write into Vulkan SC mapped memory; vkMapMemory only "
                    "yields void*. Bounds are checked above.");
    const auto* const src = static_cast<const uint8_t*>(data);
    auto* const       dst = static_cast<uint8_t*>(m_mapped);

    std::copy(src, src + bytes, dst);
    return Result::kOk;
}

// ---- Shutdown ---------------------------------------------------------------

void VertexBuffer::Shutdown(VkDevice device) noexcept
{
    if (!m_initialised) { return; }

    if ((m_memory != VK_NULL_HANDLE) && (m_mapped != nullptr))
    {
        vkUnmapMemory(device, m_memory);
        m_mapped = nullptr;
    }
    // In VulkanSC, vkFreeMemory does not exist.
    // Device memory is implicitly freed when vkDestroyDevice is called.
    m_memory = VK_NULL_HANDLE;
    if (m_buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }

    m_sizeBytes   = 0U;
    m_initialised = false;
    log::Info("VertexBuffer: destroyed.");
}

} /* namespace rendering */
} /* namespace engine */
