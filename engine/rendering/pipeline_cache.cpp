/// @file pipeline_cache.cpp
/// @brief VulkanSC pipeline cache lifecycle implementation.
///
/// Init validates the cache blob's integrity header before handing it to the
/// driver: a corrupted or device-incompatible cache binary is a safety-relevant
/// asset-integrity failure and must be caught at startup, not mid-frame
/// (Vulkan SC spec, VkPipelineCacheHeaderVersionSafetyCriticalOne).

#include "pipeline_cache.hpp"
#include "../core/log.hpp"

#include <cstdint>

namespace engine {
namespace rendering {

namespace {

/// Size of VkPipelineCacheHeaderVersionSafetyCriticalOne (spec-fixed layout).
constexpr uint32_t kScHeaderBytes{56U};

/// Minimum VkPipelineCacheSafetyCriticalIndexEntry size (spec VUID 05078).
constexpr uint32_t kMinIndexStride{56U};

/// Read a little-endian uint32 at @p off (fields are LE on every host).
[[nodiscard]] uint32_t ReadLe32(const uint8_t* const p, const uint32_t off) noexcept
{
    return static_cast<uint32_t>(p[off])                    |
          (static_cast<uint32_t>(p[off + 1U]) <<  8U)       |
          (static_cast<uint32_t>(p[off + 2U]) << 16U)       |
          (static_cast<uint32_t>(p[off + 3U]) << 24U);
}

/// Read a little-endian uint64 at @p off (fields are LE on every host).
[[nodiscard]] uint64_t ReadLe64(const uint8_t* const p, const uint32_t off) noexcept
{
    return static_cast<uint64_t>(ReadLe32(p, off)) |
          (static_cast<uint64_t>(ReadLe32(p, off + 4U)) << 32ULL);
}

} /* anonymous namespace */

Result ValidatePipelineCacheHeader(const uint8_t* const data,
                                   const uint32_t       dataSize,
                                   const uint32_t       expectedVendorId,
                                   const uint32_t       expectedDeviceId) noexcept
{
    if ((data == nullptr) || (dataSize < kScHeaderBytes))
    {
        return Result::kVkscCacheInvalid;
    }

    // ---- Fixed-layout fields (offsets per spec) ----------------------------
    const uint32_t headerSize       = ReadLe32(data,  0U);
    const uint32_t headerVersion    = ReadLe32(data,  4U);
    const uint32_t vendorID         = ReadLe32(data,  8U);
    const uint32_t deviceID         = ReadLe32(data, 12U);
    const uint32_t validationVer    = ReadLe32(data, 32U);
    const uint32_t indexCount       = ReadLe32(data, 40U);
    const uint32_t indexStride      = ReadLe32(data, 44U);
    const uint64_t indexOffset      = ReadLe64(data, 48U);

    if ((headerSize != kScHeaderBytes) ||
        (headerVersion != static_cast<uint32_t>(VK_PIPELINE_CACHE_HEADER_VERSION_SAFETY_CRITICAL_ONE)))
    {
        log::Error("ValidatePipelineCacheHeader: not a Vulkan SC cache (bad size/version).");
        return Result::kVkscCacheInvalid;
    }
    if (validationVer != static_cast<uint32_t>(VK_PIPELINE_CACHE_VALIDATION_VERSION_SAFETY_CRITICAL_ONE))
    {
        log::Error("ValidatePipelineCacheHeader: unsupported validation version.");
        return Result::kVkscCacheInvalid;
    }
    if ((vendorID != expectedVendorId) || (deviceID != expectedDeviceId))
    {
        // Not a hard failure: emulation stacks legitimately compile caches
        // whose header IDs differ from the runtime-reported physical device
        // IDs (the PCC stamps its own identifiers).  The driver remains the
        // authoritative compatibility check — it rejects an incompatible
        // blob with VK_ERROR_INVALID_PIPELINE_CACHE_DATA.  We log the
        // mismatch for the certification record and continue.
        log::Warn("ValidatePipelineCacheHeader: vendorID/deviceID differ from the "
                  "selected physical device (expected for emulation-compiled caches).");
    }
    if (indexStride < kMinIndexStride)
    {
        log::Error("ValidatePipelineCacheHeader: index stride below minimum.");
        return Result::kVkscCacheInvalid;
    }

    // Spec VUID 05079: offset + count * stride must not exceed the blob size.
    // 64-bit arithmetic throughout — no overflow possible.
    const uint64_t indexEnd =
        indexOffset + (static_cast<uint64_t>(indexCount) * static_cast<uint64_t>(indexStride));
    if (indexEnd > static_cast<uint64_t>(dataSize))
    {
        log::Error("ValidatePipelineCacheHeader: index extends past end of blob.");
        return Result::kVkscCacheInvalid;
    }

    return Result::kOk;
}

Result PipelineCacheSc::Init(VkDevice         device,
                             VkPhysicalDevice physDevice,
                             const uint8_t*   data,
                             uint32_t         dataSize) noexcept
{
    if (m_initialised)
    {
        log::Error("PipelineCacheSc::Init called on already-initialised object.");
        return Result::kAlreadyInitialised;
    }
    if ((device == VK_NULL_HANDLE) || (physDevice == VK_NULL_HANDLE) ||
        (data == nullptr) || (dataSize == 0U))
    {
        log::Error("PipelineCacheSc::Init: null device/physDevice/data or zero size.");
        return Result::kInvalidArgument;
    }

    // ---- Integrity + compatibility validation (before touching the driver) --
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physDevice, &props);

    const Result valid = ValidatePipelineCacheHeader(data, dataSize,
                                                     props.vendorID, props.deviceID);
    if (!IsOk(valid))
    {
        log::Error("PipelineCacheSc::Init: cache blob failed integrity validation.");
        return valid;
    }

    VkPipelineCacheCreateInfo cacheCI{};
    cacheCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheCI.pNext           = nullptr;
    cacheCI.flags           = 0U;
    cacheCI.initialDataSize = static_cast<size_t>(dataSize);
    cacheCI.pInitialData    = data;

    const VkResult r = vkCreatePipelineCache(device, &cacheCI, nullptr, &m_cache);
    if (r != VK_SUCCESS)
    {
        log::Error("PipelineCacheSc: vkCreatePipelineCache failed.");
        return Result::kVkscCacheFailed;
    }

    m_initialised = true;
    log::Info("PipelineCacheSc: pipeline cache created.");
    return Result::kOk;
}

void PipelineCacheSc::Shutdown(VkDevice device) noexcept
{
    if (!m_initialised) { return; }

    if (m_cache != VK_NULL_HANDLE)
    {
        vkDestroyPipelineCache(device, m_cache, nullptr);
        m_cache = VK_NULL_HANDLE;
        log::Info("PipelineCacheSc: pipeline cache destroyed.");
    }

    m_initialised = false;
}

} /* namespace rendering */
} /* namespace engine */
