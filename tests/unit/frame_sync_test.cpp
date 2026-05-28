/// @file frame_sync_test.cpp
/// @brief Unit tests for engine::rendering::FrameSync.
///
/// These tests exercise only input-validation and offline invariants —
/// no VulkanSC device is required.  Tests that need a real device are
/// covered in tests/integration/.

#include <gtest/gtest.h>
#include "engine/rendering/frame_sync.hpp"

namespace engine::rendering {

TEST(FrameSyncTest, HandlesAreNullBeforeInit)
{
    FrameSync sync;
    EXPECT_EQ(sync.ImageAvailable(), VK_NULL_HANDLE);
    EXPECT_EQ(sync.RenderComplete(), VK_NULL_HANDLE);
    EXPECT_EQ(sync.InFlight(),       VK_NULL_HANDLE);
}

TEST(FrameSyncTest, InitFailsWithNullDevice)
{
    FrameSync sync;
    EXPECT_EQ(sync.Init(VK_NULL_HANDLE), Result::kInvalidArgument);
}

TEST(FrameSyncTest, HandlesRemainNullAfterFailedInit)
{
    FrameSync sync;
    sync.Init(VK_NULL_HANDLE);
    EXPECT_EQ(sync.ImageAvailable(), VK_NULL_HANDLE);
    EXPECT_EQ(sync.RenderComplete(), VK_NULL_HANDLE);
    EXPECT_EQ(sync.InFlight(),       VK_NULL_HANDLE);
}

TEST(FrameSyncTest, ShutdownOnUninitIsNop)
{
    // Shutdown on an uninitialised FrameSync must not crash.
    FrameSync sync;
    sync.Shutdown(VK_NULL_HANDLE);
    EXPECT_EQ(sync.ImageAvailable(), VK_NULL_HANDLE);
    EXPECT_EQ(sync.RenderComplete(), VK_NULL_HANDLE);
}

} /* namespace engine::rendering */