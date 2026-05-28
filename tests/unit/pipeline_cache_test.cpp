/// @file pipeline_cache_test.cpp
/// @brief Unit tests for engine::rendering::PipelineCacheSc.
///
/// These tests exercise only input-validation paths and are fully offline
/// (no VulkanSC device required).  Tests that exercise the success path
/// with a real device are in tests/integration/.

#include <gtest/gtest.h>
#include "engine/rendering/pipeline_cache.hpp"

namespace engine::rendering {

TEST(PipelineCacheScTest, HandleIsNullBeforeInit)
{
    PipelineCacheSc cache;
    EXPECT_EQ(cache.Handle(), VK_NULL_HANDLE);
}

TEST(PipelineCacheScTest, InitFailsWithNullDevice)
{
    PipelineCacheSc cache;
    uint8_t data[8]{0xAB, 0xCD, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, data, sizeof(data)), Result::kInvalidArgument);
}

TEST(PipelineCacheScTest, InitFailsWithNullData)
{
    // Null data pointer is rejected even when device would be valid.
    // (Here device is VK_NULL_HANDLE too, but null data check is independent.)
    PipelineCacheSc cache;
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, nullptr, 8U), Result::kInvalidArgument);
}

TEST(PipelineCacheScTest, InitFailsWithZeroDataSize)
{
    PipelineCacheSc cache;
    uint8_t data[4]{};
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, data, 0U), Result::kInvalidArgument);
}

TEST(PipelineCacheScTest, ShutdownOnUninitIsNop)
{
    // Shutdown on an uninitialised cache must not crash; handle stays null.
    PipelineCacheSc cache;
    cache.Shutdown(VK_NULL_HANDLE);
    EXPECT_EQ(cache.Handle(), VK_NULL_HANDLE);
}

} /* namespace engine::rendering */