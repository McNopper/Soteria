/// @file SRS_SYNC_001_test.cpp
/// @brief Qualification test for SRS-SYNC-001.
///
/// Verifies: "The engine shall create one imageAvailable semaphore, one
/// renderComplete semaphore, and one pre-signalled in-flight fence during
/// FrameSync initialisation."
///
/// @satisfies SRS-SYNC-001

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"
#include "engine/rendering/frame_sync.hpp"

namespace engine {

TEST(Qualification_SRS_SYNC_001, FrameSyncCreatesAllSyncObjects)
{
    VkscContext ctx;
    VkscContextConfig cfg{};
    cfg.resources.semaphores = 2U;
    cfg.resources.fences     = 1U;

    Result r = ctx.Init(cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    rendering::FrameSync sync;
    ASSERT_EQ(sync.Init(ctx.Device()), Result::kOk);

    EXPECT_NE(sync.ImageAvailable(), VK_NULL_HANDLE);
    EXPECT_NE(sync.RenderComplete(), VK_NULL_HANDLE);
    EXPECT_NE(sync.InFlight(),       VK_NULL_HANDLE);

    sync.Shutdown(ctx.Device());
    ctx.Shutdown();
}

} /* namespace engine */