/// @file vksc_context_test.cpp
/// @brief Unit tests for engine::VkscContext.
///
/// Offline tests (no VulkanSC device required) are guarded by nothing —
/// they always run.  Tests that call Init() and expect kOk require a
/// VulkanSC driver; they use GTEST_SKIP() when no driver is present.

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"

namespace engine {

// ---------------------------------------------------------------------------
// Offline invariants — no device required
// ---------------------------------------------------------------------------

TEST(VkscContextTest, IsNotInitialisedByDefault)
{
    VkscContext ctx;
    EXPECT_FALSE(ctx.IsInitialised());
    EXPECT_EQ(ctx.Device(),         VK_NULL_HANDLE);
    EXPECT_EQ(ctx.Instance(),       VK_NULL_HANDLE);
    EXPECT_EQ(ctx.PhysicalDevice(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.GraphicsQueue(),  VK_NULL_HANDLE);
}

TEST(VkscContextTest, ShutdownOnUninitIsNop)
{
    VkscContext ctx;
    ctx.Shutdown();
    EXPECT_FALSE(ctx.IsInitialised());
    EXPECT_EQ(ctx.Device(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// Device-dependent tests — skipped when VulkanSC is not available
// ---------------------------------------------------------------------------

TEST(VkscContextTest, InitSucceedsAndSetsInitialisedFlag)
{
    VkscContext ctx;
    VkscContextConfig config{};
    Result r = ctx.Init(config);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ") — skipping";
    }
    EXPECT_TRUE(ctx.IsInitialised());
    EXPECT_NE(ctx.Device(),         VK_NULL_HANDLE);
    EXPECT_NE(ctx.Instance(),       VK_NULL_HANDLE);
    EXPECT_NE(ctx.PhysicalDevice(), VK_NULL_HANDLE);
    ctx.Shutdown();
}

TEST(VkscContextTest, SecondInitReturnsAlreadyInitialised)
{
    VkscContext ctx;
    VkscContextConfig config{};
    Result r = ctx.Init(config);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ") — skipping";
    }
    EXPECT_EQ(ctx.Init(config), Result::kAlreadyInitialised);
    ctx.Shutdown();
}

TEST(VkscContextTest, ShutdownResetsInitialisedFlag)
{
    VkscContext ctx;
    VkscContextConfig config{};
    Result r = ctx.Init(config);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ") — skipping";
    }
    EXPECT_TRUE(ctx.IsInitialised());
    ctx.Shutdown();
    EXPECT_FALSE(ctx.IsInitialised());
    EXPECT_EQ(ctx.Device(), VK_NULL_HANDLE);
}

} /* namespace engine */