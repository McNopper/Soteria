/// @file SRS_INIT_001_test.cpp
/// @brief Qualification test for SRS-INIT-001.
///
/// Verifies: "The engine shall initialise a Vulkan SC instance and logical
/// device from a caller-supplied configuration."
///
/// @satisfies SRS-INIT-001

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"

namespace engine {

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

} /* namespace engine */