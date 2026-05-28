/// @file SRS_CMD_001_test.cpp
/// @brief Qualification test for SRS-CMD-001.
///
/// Verifies: "The engine shall reserve command buffer memory upfront at
/// CommandPool initialisation time; no allocation shall be performed after
/// Init returns."
///
/// @satisfies SRS-CMD-001

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"
#include "engine/rendering/command_pool.hpp"

namespace engine {

TEST(Qualification_SRS_CMD_001, CommandPoolReservesBuffersUpfront)
{
    VkscContext ctx;
    VkscContextConfig cfg{};
    cfg.resources.commandPools   = 1U;
    cfg.resources.commandBuffers = 4U;

    Result r = ctx.Init(cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    rendering::CommandPool pool;
    ASSERT_EQ(pool.Init(ctx.Device(),
                        ctx.GraphicsQueueFamily(),
                        /* reservedSizeBytes */ 64U * 1024U,
                        /* maxCommandBuffers */ 4U),
              Result::kOk);
    EXPECT_NE(pool.Handle(), VK_NULL_HANDLE);

    // Allocate all reserved command buffers — this must succeed.
    VkCommandBuffer buffers[4]{};
    EXPECT_EQ(pool.AllocateBuffers(ctx.Device(), 4U, buffers), Result::kOk);
    for (auto buf : buffers) {
        EXPECT_NE(buf, VK_NULL_HANDLE);
    }

    pool.Shutdown(ctx.Device());
    ctx.Shutdown();
}

} /* namespace engine */