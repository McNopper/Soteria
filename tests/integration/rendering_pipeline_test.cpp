/// @file rendering_pipeline_test.cpp
/// @brief Integration tests for CommandPool, FrameSync, and PipelineCacheSc on a real VkDevice.
///
/// These tests verify that rendering components can be initialised using the VkDevice
/// obtained from VkscContext.  They require a VulkanSC-capable host and are
/// automatically skipped when none is available.
///
/// @satisfies SRS-CMD-001   CommandPool reserves command buffer memory upfront.
/// @satisfies SRS-SYNC-001  FrameSync creates semaphore and fence handles.
/// @satisfies SRS-PIPE-001  PipelineCacheSc loads compile-time binary cache.
/// @satisfies SRS-INIT-005  Ordered Shutdown releases all component resources.

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"
#include "engine/rendering/command_pool.hpp"
#include "engine/rendering/frame_sync.hpp"
#include "engine/rendering/pipeline_cache.hpp"
#include "app/rendering/pipeline_cache_data.hpp"

namespace engine {

/// Fixture: one VkscContext shared across rendering integration tests.
class RenderingPipelineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Declare the pre-compiled pipeline cache in the device reservation,
        // exactly as production (app/main.cpp) does.  Vulkan SC only accepts a
        // cache at vkCreatePipelineCache time if it was declared here, and the
        // embedded binary is built for the emulation driver — so prefer it.
        m_pcCI                 = VkPipelineCacheCreateInfo{};
        m_pcCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        m_pcCI.initialDataSize = sim::kPipelineCacheDataSize;
        m_pcCI.pInitialData    = sim::kPipelineCacheData;

        VkscContextConfig cfg{};
        // Reserve the objects we will create during these tests.
        cfg.resources.commandPools    = 1U;
        cfg.resources.commandBuffers  = 4U;
        cfg.resources.semaphores      = 2U;
        cfg.resources.fences          = 1U;
        cfg.resources.pipelineCaches  = 1U;
        cfg.pipelineCacheInfos        = &m_pcCI;
        cfg.pipelineCacheInfoCount    = 1U;
        cfg.preferredDriverId         = VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN;

        Result r = m_ctx.Init(cfg);
        if (!IsOk(r)) {
            GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
        }
    }

    void TearDown() override
    {
        m_pool.Shutdown(m_ctx.Device());
        m_sync.Shutdown(m_ctx.Device());
        m_cache.Shutdown(m_ctx.Device());
        m_ctx.Shutdown();
    }

    VkscContext                  m_ctx;
    VkPipelineCacheCreateInfo    m_pcCI{};
    rendering::CommandPool       m_pool;
    rendering::FrameSync         m_sync;
    rendering::PipelineCacheSc   m_cache;
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

/// @test IT-006 — CommandPool and FrameSync co-exist on the same VkDevice.
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

/// @test IT-007 — PipelineCacheSc initialises on a real VkDevice.
TEST_F(RenderingPipelineTest, PipelineCacheScInitOnRealDevice)
{
    // The embedded cache was declared in the device reservation during SetUp,
    // so the VulkanSC driver accepts this same binary here.
    Result r = m_cache.Init(m_ctx.Device(),
                            sim::kPipelineCacheData,
                            sim::kPipelineCacheDataSize);
    EXPECT_EQ(r, Result::kOk);
    EXPECT_NE(m_cache.Handle(), VK_NULL_HANDLE);
}

/// @test IT-008 — Ordered Shutdown: all component handles null after Shutdown.
TEST_F(RenderingPipelineTest, OrderedShutdownResetsAllHandles)
{
    ASSERT_EQ(m_pool.Init(m_ctx.Device(),
                          m_ctx.GraphicsQueueFamily(),
                          64U * 1024U, 4U),
              Result::kOk);
    ASSERT_EQ(m_sync.Init(m_ctx.Device()), Result::kOk);

    ASSERT_EQ(m_cache.Init(m_ctx.Device(),
                           sim::kPipelineCacheData,
                           sim::kPipelineCacheDataSize),
              Result::kOk);

    // Shutdown in reverse initialisation order.
    m_cache.Shutdown(m_ctx.Device());
    m_sync.Shutdown(m_ctx.Device());
    m_pool.Shutdown(m_ctx.Device());

    EXPECT_EQ(m_cache.Handle(),         VK_NULL_HANDLE);
    EXPECT_EQ(m_sync.ImageAvailable(),  VK_NULL_HANDLE);
    EXPECT_EQ(m_sync.InFlight(),        VK_NULL_HANDLE);
    EXPECT_EQ(m_pool.Handle(),          VK_NULL_HANDLE);
}

} /* namespace engine */