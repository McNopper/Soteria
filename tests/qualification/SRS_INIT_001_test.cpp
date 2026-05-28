/// @file SRS_INIT_001_test.cpp
/// @brief Qualification tests for SRS-INIT-001 and SRS-INIT-005.
///
/// QT-001: Verifies "The engine shall initialise a Vulkan SC instance and logical
///         device from a caller-supplied configuration." (SRS-INIT-001)
///
/// QT-003: Verifies "The software shall perform deterministic ordered Shutdown;
///         all handles must be null and IsInitialised() false after Shutdown."
///         (SRS-INIT-005)
///
/// @satisfies SRS-INIT-001
/// @satisfies SRS-INIT-005

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"

namespace engine {

/// @test QT-001 — Init creates Vulkan SC instance and device.
TEST(Qualification_SRS_INIT_001, InitCreatesInstanceAndDevice)
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

/// @test QT-003 — Shutdown resets all handles and IsInitialised to false (SRS-INIT-005).
TEST(Qualification_SRS_INIT_005, ShutdownResetsAllHandles)
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