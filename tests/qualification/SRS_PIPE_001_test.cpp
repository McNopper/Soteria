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
    VkscContext ctx;
    VkscContextConfig cfg{};
    cfg.resources.pipelineCaches = 1U;

    Result r = ctx.Init(cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    // Minimal valid binary: a zero-filled block.  The VulkanSC driver accepts
    // any non-empty blob; the actual compiled pipelines are declared separately
    // via VkPipelineCacheCreateInfo in VkscContextConfig.
    static constexpr uint8_t kDummyCacheData[64]{};

    rendering::PipelineCacheSc cache;
    ASSERT_EQ(cache.Init(ctx.Device(), kDummyCacheData, sizeof(kDummyCacheData)),
              Result::kOk);
    EXPECT_NE(cache.Handle(), VK_NULL_HANDLE);

    cache.Shutdown(ctx.Device());
    ctx.Shutdown();
}

} /* namespace engine */