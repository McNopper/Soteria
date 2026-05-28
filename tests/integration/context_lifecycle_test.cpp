/// @file context_lifecycle_test.cpp
/// @brief Integration tests for VkscContext full lifecycle.
///
/// These tests verify the VkscContext Init → use → Shutdown sequence
/// against a real (or emulated) VulkanSC driver.  They are automatically
/// skipped when no suitable driver is present.
///
/// @satisfies SRS-INIT-001  VkscContext initialises the Vulkan SC stack.
/// @satisfies SRS-INIT-002  Second Init() returns kAlreadyInitialised.
/// @satisfies SRS-INIT-003  Shutdown() resets all handles.

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"

namespace engine {

class ContextLifecycleTest : public ::testing::Test
{
protected:
    VkscContext      m_ctx;
    VkscContextConfig m_cfg{};
};

/// @test IT-001 — Full lifecycle: Init, verify handles, Shutdown.
TEST_F(ContextLifecycleTest, FullInitShutdownLifecycle)
{
    Result r = m_ctx.Init(m_cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    EXPECT_TRUE(m_ctx.IsInitialised());
    EXPECT_NE(m_ctx.Instance(),       VK_NULL_HANDLE);
    EXPECT_NE(m_ctx.PhysicalDevice(), VK_NULL_HANDLE);
    EXPECT_NE(m_ctx.Device(),         VK_NULL_HANDLE);
    EXPECT_NE(m_ctx.GraphicsQueue(),  VK_NULL_HANDLE);

    m_ctx.Shutdown();

    EXPECT_FALSE(m_ctx.IsInitialised());
    EXPECT_EQ(m_ctx.Device(),   VK_NULL_HANDLE);
    EXPECT_EQ(m_ctx.Instance(), VK_NULL_HANDLE);
}

/// @test IT-002 — Double-init guard.
TEST_F(ContextLifecycleTest, SecondInitReturnsAlreadyInitialised)
{
    Result r = m_ctx.Init(m_cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    EXPECT_EQ(m_ctx.Init(m_cfg), Result::kAlreadyInitialised);

    m_ctx.Shutdown();
}

/// @test IT-003 — Shutdown is idempotent (safe to call twice).
TEST_F(ContextLifecycleTest, ShutdownIsIdempotent)
{
    Result r = m_ctx.Init(m_cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    m_ctx.Shutdown();
    EXPECT_EQ(m_ctx.Device(), VK_NULL_HANDLE);

    // Second Shutdown must not crash.
    m_ctx.Shutdown();
    EXPECT_EQ(m_ctx.Device(), VK_NULL_HANDLE);
}

} /* namespace engine */