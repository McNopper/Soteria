/// @file pipeline_cache.cpp
/// @brief VulkanSC pipeline cache lifecycle implementation.

#include "pipeline_cache.hpp"
#include "../core/log.hpp"

#include <cstdint>

namespace engine {
namespace rendering {

Result PipelineCacheSc::Init(VkDevice device,
                              const uint8_t* data,
                              uint32_t dataSize) noexcept
{
    if (m_initialised)
    {
        log::Error("PipelineCacheSc::Init called on already-initialised object.");
        return Result::kAlreadyInitialised;
    }
    if ((device == VK_NULL_HANDLE) || (data == nullptr) || (dataSize == 0U))
    {
        log::Error("PipelineCacheSc::Init: null device, data, or zero size.");
        return Result::kInvalidArgument;
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
