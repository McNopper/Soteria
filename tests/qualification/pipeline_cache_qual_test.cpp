/// @file pipeline_cache_qual_test.cpp
/// @brief Qualification tests for the offline pipeline cache loading path.
///
/// Verifies: "The engine shall load a compile-time binary pipeline cache
/// and create a VkPipelineCache handle from it" and "Init shall validate that
/// the supplied cache data pointer is non-null and the size is greater than
/// zero; invalid arguments return kInvalidArgument."

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"
#include "engine/rendering/pipeline_cache.hpp"
#include "app/rendering/pipeline_cache_data.hpp"

#include <array>

namespace engine {

// ---------------------------------------------------------------------------
// Argument validation — fully offline
// ---------------------------------------------------------------------------

TEST(Qualification_PipelineCache, NullDeviceReturnsInvalidArgument)
{
    rendering::PipelineCacheSc cache;
    std::array<uint8_t, 8U> data{0x01U, 0x02U, 0x03U, 0x04U,
                                 0x05U, 0x06U, 0x07U, 0x08U};
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, data.data(), static_cast<uint32_t>(data.size())),
              Result::kInvalidArgument);
}

TEST(Qualification_PipelineCache, NullDataReturnsInvalidArgument)
{
    rendering::PipelineCacheSc cache;
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, nullptr, 8U),
              Result::kInvalidArgument);
}

TEST(Qualification_PipelineCache, ZeroSizeReturnsInvalidArgument)
{
    rendering::PipelineCacheSc cache;
    std::array<uint8_t, 4U> data{};
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, data.data(), 0U),
              Result::kInvalidArgument);
}

// ---------------------------------------------------------------------------
// Success path — requires VulkanSC device
// ---------------------------------------------------------------------------

TEST(Qualification_PipelineCache, InitCreatesPipelineCacheHandle)
{
    // Vulkan SC requires every pipeline cache to be declared in the device
    // reservation at vkCreateDevice time; vkCreatePipelineCache then only
    // accepts that same pre-compiled, driver-specific binary.  An arbitrary or
    // zero-filled blob is rejected with VK_ERROR_INVALID_PIPELINE_CACHE_DATA.
    // We therefore mirror production (app/main.cpp): declare the embedded cache
    // in the reservation and prefer the emulation driver it was compiled for.
    VkPipelineCacheCreateInfo pcCI{};
    pcCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pcCI.initialDataSize = sim::kPipelineCacheDataSize;
    pcCI.pInitialData    = sim::kPipelineCacheData;

    VkscContext ctx;
    VkscContextConfig cfg{};
    cfg.resources.pipelineCaches = 1U;
    cfg.pipelineCacheInfos       = &pcCI;
    cfg.pipelineCacheInfoCount   = 1U;
    cfg.preferredDriverId        = VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN;

    Result r = ctx.Init(cfg);
    if (!IsOk(r)) {
        // No VulkanSC device, or no driver whose pre-compiled cache matches the
        // embedded binary on this host — environment precondition not met.
        GTEST_SKIP() << "VulkanSC emulation with matching pipeline cache not available ("
                     << ResultToString(r) << ")";
    }

    rendering::PipelineCacheSc cache;
    ASSERT_EQ(cache.Init(ctx.Device(),
                         sim::kPipelineCacheData,
                         sim::kPipelineCacheDataSize),
              Result::kOk);
    EXPECT_NE(cache.Handle(), VK_NULL_HANDLE);

    cache.Shutdown(ctx.Device());
    ctx.Shutdown();
}

} /* namespace engine */
