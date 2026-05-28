/// @file pipeline_cache.hpp
/// @brief Generic VulkanSC pipeline cache lifecycle manager.
///
/// In VulkanSC, all pipelines must be pre-compiled offline.  At runtime
/// the application loads the binary pipeline cache and creates a
/// VkPipelineCache handle from it.  Pipeline creation then retrieves the
/// pre-compiled pipeline by matching the UUID stored in
/// VkPipelineOfflineCreateInfo.
///
/// @satisfies SWS_RENDER_020  PipelineCacheSc owns the VkPipelineCache handle.
/// @satisfies SWS_RENDER_021  Init validates that data is non-null and non-empty.
/// @satisfies SRS-PIPE-001    Pipeline cache is loaded from compile-time binary
///                            data; no filesystem I/O occurs at runtime.
/// @satisfies SRS-PIPE-002    Init rejects null data pointer or zero data size.

#ifndef VKSC_ENGINE_RENDERING_PIPELINE_CACHE_HPP
#define VKSC_ENGINE_RENDERING_PIPELINE_CACHE_HPP

#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>
#include <cstdint>

namespace engine {
namespace rendering {

/// @brief Wraps one VkPipelineCache handle.
///
/// Non-copyable, non-movable.  One instance per pre-compiled pipeline bundle.
class PipelineCacheSc
{
public:
    PipelineCacheSc()  noexcept = default;
    ~PipelineCacheSc() noexcept = default;

    PipelineCacheSc(const PipelineCacheSc&)            = delete;
    PipelineCacheSc& operator=(const PipelineCacheSc&) = delete;
    PipelineCacheSc(PipelineCacheSc&&)                 = delete;
    PipelineCacheSc& operator=(PipelineCacheSc&&)      = delete;

    /// @brief Create a VkPipelineCache from the supplied binary data.
    ///
    /// @param device    Logical device (must be valid).
    /// @param data      Pointer to the compiled pipeline cache binary.
    /// @param dataSize  Size of @p data in bytes.  Must be > 0.
    /// @returns Result::kOk on success.
    [[nodiscard]] Result Init(VkDevice       device,
                              const uint8_t* data,
                              uint32_t       dataSize) noexcept;

    /// @brief Destroy the pipeline cache.  Safe on partial init.
    void Shutdown(VkDevice device) noexcept;

    /// @returns The VkPipelineCache handle, or VK_NULL_HANDLE before Init.
    [[nodiscard]] VkPipelineCache Handle() const noexcept { return m_cache; }

private:
    VkPipelineCache m_cache{VK_NULL_HANDLE};
    bool            m_initialised{false};
};

} /* namespace rendering */
} /* namespace engine */

#endif /* VKSC_ENGINE_RENDERING_PIPELINE_CACHE_HPP */
