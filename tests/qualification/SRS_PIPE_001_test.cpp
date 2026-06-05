/// @file SRS_PIPE_001_test.cpp
/// @brief Qualification tests for SRS-PIPE-001 and SRS-PIPE-002.
///
/// SRS-PIPE-001: "The engine shall load a compile-time binary pipeline cache
/// and create a VkPipelineCache handle from it."
///
/// SRS-PIPE-002: "Init shall validate that the supplied cache data pointer is
/// non-null and the size is greater than zero; invalid arguments return
/// kInvalidArgument."
///
/// @satisfies SRS-PIPE-001
/// @satisfies SRS-PIPE-002

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"
#include "engine/rendering/pipeline_cache.hpp"
#include "app/rendering/pipeline_cache_data.hpp"

namespace engine {

// ---------------------------------------------------------------------------
// SRS-PIPE-002: argument validation — fully offline
// ---------------------------------------------------------------------------

TEST(Qualification_SRS_PIPE_002, NullDeviceReturnsInvalidArgument)
{
    rendering::PipelineCacheSc cache;
    uint8_t data[8]{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, data, sizeof(data)),
              Result::kInvalidArgument);
}

TEST(Qualification_SRS_PIPE_002, NullDataReturnsInvalidArgument)
{
    rendering::PipelineCacheSc cache;
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, nullptr, 8U),
              Result::kInvalidArgument);
}

TEST(Qualification_SRS_PIPE_002, ZeroSizeReturnsInvalidArgument)
{
    rendering::PipelineCacheSc cache;
    uint8_t data[4]{};
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, data, 0U),
              Result::kInvalidArgument);
}

// ---------------------------------------------------------------------------
// SRS-PIPE-001: success path — requires VulkanSC device
// ---------------------------------------------------------------------------

TEST(Qualification_SRS_PIPE_001, InitCreatesPipelineCacheHandle)
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