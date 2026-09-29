/// @file pipeline_cache.hpp
/// @brief Generic VulkanSC pipeline cache lifecycle manager.
///
/// In VulkanSC, all pipelines must be pre-compiled offline.  At runtime
/// the application loads the binary pipeline cache and creates a
/// VkPipelineCache handle from it.  Pipeline creation then retrieves the
/// pre-compiled pipeline by matching the UUID stored in
/// VkPipelineOfflineCreateInfo.
///
/// The cache is loaded from compile-time binary data; no filesystem I/O
/// occurs at runtime.  Init rejects a null data pointer or zero data size.

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

    /// @brief Validate the blob, then create a VkPipelineCache from it.
    ///
    /// @param device      Logical device (must be valid).
    /// @param physDevice  Physical device; the blob's vendorID/deviceID header
    ///                    fields must match it (cache binaries are device-specific).
    /// @param data        Pointer to the compiled pipeline cache binary.
    /// @param dataSize    Size of @p data in bytes.  Must be > 0.
    /// @returns Result::kOk on success; kInvalidArgument on null/zero inputs;
    ///          kVkscCacheInvalid when the blob fails integrity validation;
    ///          kVkscCacheFailed when vkCreatePipelineCache rejects it.
    [[nodiscard]] Result Init(VkDevice         device,
                              VkPhysicalDevice physDevice,
                              const uint8_t*   data,
                              uint32_t         dataSize) noexcept;

    /// @brief Destroy the pipeline cache.  Safe on partial init.
    void Shutdown(VkDevice device) noexcept;

    /// @returns The VkPipelineCache handle, or VK_NULL_HANDLE before Init.
    [[nodiscard]] VkPipelineCache Handle() const noexcept { return m_cache; }

private:
    VkPipelineCache m_cache{VK_NULL_HANDLE};
    bool            m_initialised{false};
};

/// @brief Validate the 56-byte Vulkan SC pipeline cache header.
///
/// Checks (spec, "VkPipelineCacheHeaderVersionSafetyCriticalOne"):
///   * dataSize >= header size (56 bytes);
///   * headerSize field == 56 and headerVersion is SAFETY_CRITICAL_ONE;
///   * validationVersion is SAFETY_CRITICAL_ONE;
///   * pipelineIndexStride >= 56 and the index fits inside dataSize.
///
/// vendorID / deviceID mismatches against the physical device are logged as
/// warnings but do NOT fail validation: emulation stacks legitimately
/// compile caches whose header IDs differ from the runtime-reported IDs.
/// Device compatibility is the driver's authoritative check
/// (VK_ERROR_INVALID_PIPELINE_CACHE_DATA at vkCreatePipelineCache).
///
/// All header fields are little-endian regardless of host byte order; they
/// are parsed byte-wise (no casts, no alignment assumptions).
///
/// Pure arithmetic — no Vulkan calls, offline-testable.
///
/// @param data              Blob to validate (must be non-null).
/// @param dataSize          Blob size in bytes.
/// @param expectedVendorId  VkPhysicalDeviceProperties::vendorID.
/// @param expectedDeviceId  VkPhysicalDeviceProperties::deviceID.
/// @returns Result::kOk when the header is well-formed.
[[nodiscard]] Result ValidatePipelineCacheHeader(const uint8_t* data,
                                                 uint32_t       dataSize,
                                                 uint32_t       expectedVendorId,
                                                 uint32_t       expectedDeviceId) noexcept;

} /* namespace rendering */
} /* namespace engine */

#endif /* VKSC_ENGINE_RENDERING_PIPELINE_CACHE_HPP */
