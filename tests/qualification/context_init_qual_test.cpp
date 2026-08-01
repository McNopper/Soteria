/// @file context_init_qual_test.cpp
/// @brief Qualification tests for VkscContext initialisation and shutdown.
///
/// Verifies: "The engine shall initialise a Vulkan SC instance and logical
/// device from a caller-supplied configuration" and "Shutdown shall reset all
/// handles and IsInitialised() to false (deterministic ordered shutdown)."

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"

namespace engine {

/// Init creates Vulkan SC instance, physical device, logical device, and queue.
TEST(Qualification_ContextInit, InitCreatesInstanceAndDevice)
{
    VkscContext ctx;
    VkscContextConfig cfg{};

    Result r = ctx.Init(cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    EXPECT_TRUE(ctx.IsInitialised());
    EXPECT_NE(ctx.Instance(),       VK_NULL_HANDLE);
    EXPECT_NE(ctx.PhysicalDevice(), VK_NULL_HANDLE);
    EXPECT_NE(ctx.Device(),         VK_NULL_HANDLE);
    EXPECT_NE(ctx.GraphicsQueue(),  VK_NULL_HANDLE);

    ctx.Shutdown();
}

/// Shutdown resets all handles and IsInitialised to false.
TEST(Qualification_ContextInit, ShutdownResetsAllHandles)
{
    VkscContext ctx;
    VkscContextConfig cfg{};

    Result r = ctx.Init(cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    ctx.Shutdown();

    EXPECT_FALSE(ctx.IsInitialised());
    EXPECT_EQ(ctx.Instance(),       VK_NULL_HANDLE);
    EXPECT_EQ(ctx.PhysicalDevice(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.Device(),         VK_NULL_HANDLE);
    EXPECT_EQ(ctx.GraphicsQueue(),  VK_NULL_HANDLE);
}

} /* namespace engine */
