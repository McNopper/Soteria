/// @file vertex_buffer.hpp
/// @brief Generic host-visible, persistently-mapped vertex buffer.
///
/// Allocates a VkBuffer with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
/// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT and keeps it persistently mapped.
/// Callers write vertices by memcpy-ing directly into MappedPtr().
///
/// No dynamic memory allocation.
///
/// @satisfies SWS_RENDER_030  VertexBuffer owns buffer and memory handles.
/// @satisfies SWS_RENDER_031  Persistent mapping eliminates per-frame map/unmap.

#ifndef VKSC_ENGINE_RENDERING_VERTEX_BUFFER_HPP
#define VKSC_ENGINE_RENDERING_VERTEX_BUFFER_HPP

#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>
#include <cstdint>

namespace engine {
namespace rendering {

/// @brief Host-visible vertex buffer with persistent mapping.
///
/// Non-copyable, non-movable.
class VertexBuffer
{
public:
    VertexBuffer()  noexcept = default;
    ~VertexBuffer() noexcept = default;

    VertexBuffer(const VertexBuffer&)            = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&&)                 = delete;
    VertexBuffer& operator=(VertexBuffer&&)      = delete;

    /// @brief Allocate a host-coherent vertex buffer of @p sizeBytes.
    ///
    /// @param device      Logical device.
    /// @param physDevice  Physical device (used for memory type query).
    /// @param sizeBytes   Buffer size in bytes.  Must be > 0.
    /// @returns Result::kOk on success.
    [[nodiscard]] Result Init(VkDevice         device,
                              VkPhysicalDevice physDevice,
                              uint32_t         sizeBytes) noexcept;

    /// @brief Unmap and free buffer + memory.  Safe on partial init.
    void Shutdown(VkDevice device) noexcept;

    /// @returns VkBuffer handle.
    [[nodiscard]] VkBuffer Buffer() const noexcept { return m_buffer; }

    /// @returns Persistently-mapped write pointer.  Valid between Init and Shutdown.
    [[nodiscard]] void* MappedPtr() const noexcept { return m_mapped; }

    /// @returns Persistently-mapped write pointer as a byte pointer.
    /// Use with std::copy for type-safe byte-level writes to the buffer.
    /// Valid between Init and Shutdown.
    [[nodiscard]] uint8_t* MappedBytes() const noexcept
    {
        return static_cast<uint8_t*>(m_mapped);
    }

    /// @returns Allocated buffer size in bytes.
    [[nodiscard]] uint32_t SizeBytes() const noexcept { return m_sizeBytes; }

private:
    VkBuffer       m_buffer{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    void*          m_mapped{nullptr};
    uint32_t       m_sizeBytes{0U};
    bool           m_initialised{false};
};

} /* namespace rendering */
} /* namespace engine */

#endif /* VKSC_ENGINE_RENDERING_VERTEX_BUFFER_HPP */
