/// @file vertex_buffer.hpp
/// @brief Generic host-visible, persistently-mapped vertex buffer.
///
/// Allocates a VkBuffer with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
/// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT and keeps it persistently mapped.
/// Callers upload vertex data through the bounds-checked Write() method;
/// direct pointer access is available via MappedPtr() for special cases.
///
/// No dynamic memory allocation.

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

    /// @brief Copy @p bytes from @p data into the mapped buffer at offset 0.
    ///
    /// Bounds-checked: fails with Result::kInvalidArgument when @p data is
    /// null, @p bytes is zero, or @p bytes exceeds the buffer size.
    ///
    /// The memory is host-coherent — no explicit flush is required.
    ///
    /// @param data   Source bytes.  Must point to at least @p bytes readable
    ///               bytes for the duration of the call.
    /// @param bytes  Number of bytes to copy.
    /// @returns Result::kOk on success.
    [[nodiscard]] Result Write(const void* data, uint32_t bytes) const noexcept;

    /// @returns VkBuffer handle.
    [[nodiscard]] VkBuffer Buffer() const noexcept { return m_buffer; }

    /// @returns Persistently-mapped write pointer.  Valid between Init and
    ///          Shutdown.  Prefer Write() — casting this pointer to a typed
    ///          pointer requires a MISRA C++:2023 Rule 8.2.6 deviation at the
    ///          call site.
    [[nodiscard]] void* MappedPtr() const noexcept { return m_mapped; }

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
