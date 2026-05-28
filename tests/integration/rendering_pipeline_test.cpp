/// @file rendering_pipeline_test.cpp
/// @brief Integration tests for CommandPool and FrameSync on a real VkDevice.
///
/// These tests verify that CommandPool and FrameSync can be initialised using
/// the VkDevice obtained from VkscContext.  They require a VulkanSC-capable
/// host and are automatically skipped when none is available.
///
/// @satisfies SRS-CMD-001   CommandPool reserves command buffer memory upfront.
/// @satisfies SRS-SYNC-001  FrameSync creates semaphore and fence handles.

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"
#include "engine/rendering/command_pool.hpp"
#include "engine/rendering/frame_sync.hpp"

namespace engine {

/// Fixture: one VkscContext shared across rendering integration tests.
class RenderingPipelineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        VkscContextConfig cfg{};
        // Reserve the objects we will create during these tests.
        cfg.resources.commandPools    = 1U;
        cfg.resources.commandBuffers  = 4U;
        cfg.resources.semaphores      = 2U;
        cfg.resources.fences          = 1U;

        Result r = m_ctx.Init(cfg);
        if (!IsOk(r)) {
            GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
        }
    }

    void TearDown() override
    {
        m_pool.Shutdown(m_ctx.Device());
        m_sync.Shutdown(m_ctx.Device());
        m_ctx.Shutdown();
    }

    VkscContext             m_ctx;
    rendering::CommandPool  m_pool;
    rendering::FrameSync    m_sync;
};

/// @test IT-004 — CommandPool initialises on a real VkDevice.
TEST_F(RenderingPipelineTest, CommandPoolInitOnRealDevice)
{
    Result r = m_pool.Init(m_ctx.Device(),
                           m_ctx.GraphicsQueueFamily(),
                           /* reservedSizeBytes */ 64U * 1024U,
                           /* maxCommandBuffers */ 4U);
    EXPECT_EQ(r, Result::kOk);
    EXPECT_NE(m_pool.Handle(), VK_NULL_HANDLE);
}

/// @test IT-005 — FrameSync initialises on a real VkDevice.
TEST_F(RenderingPipelineTest, FrameSyncInitOnRealDevice)
{
    Result r = m_sync.Init(m_ctx.Device());
    EXPECT_EQ(r, Result::kOk);
    EXPECT_NE(m_sync.ImageAvailable(), VK_NULL_HANDLE);
    EXPECT_NE(m_sync.RenderComplete(), VK_NULL_HANDLE);
    EXPECT_NE(m_sync.InFlight(),       VK_NULL_HANDLE);
}

/// @test IT-006 — CommandPool + FrameSync co-exist on the same VkDevice.
TEST_F(RenderingPipelineTest, CommandPoolAndFrameSyncCoexist)
{
    ASSERT_EQ(m_pool.Init(m_ctx.Device(),
                          m_ctx.GraphicsQueueFamily(),
                          64U * 1024U, 4U),
              Result::kOk);
    ASSERT_EQ(m_sync.Init(m_ctx.Device()), Result::kOk);

    // Both should expose valid handles simultaneously.
    EXPECT_NE(m_pool.Handle(),         VK_NULL_HANDLE);
    EXPECT_NE(m_sync.ImageAvailable(), VK_NULL_HANDLE);
    EXPECT_NE(m_sync.InFlight(),       VK_NULL_HANDLE);
}

} /* namespace engine */