/// @file command_pool_test.cpp
/// @brief Unit tests for engine::rendering::CommandPool.
///
/// Tests in this file exercise only input-validation paths and are fully
/// offline (no Vulkan SC device required).  Tests that require a live device
/// are covered in tests/integration/.

#include <gtest/gtest.h>
#include "engine/rendering/command_pool.hpp"

namespace engine::rendering {

TEST(CommandPoolTest, HandleIsNullBeforeInit)
{
    CommandPool pool;
    EXPECT_EQ(pool.Handle(), VK_NULL_HANDLE);
}

TEST(CommandPoolTest, InitFailsWithNullDevice)
{
    CommandPool pool;
    EXPECT_EQ(pool.Init(VK_NULL_HANDLE, 0U, 1024U, 8U), Result::kInvalidArgument);
}

TEST(CommandPoolTest, ShutdownOnUninitIsNop)
{
    // Shutdown on an uninitialised pool must not crash and must leave Handle() null.
    CommandPool pool;
    pool.Shutdown(VK_NULL_HANDLE);
    EXPECT_EQ(pool.Handle(), VK_NULL_HANDLE);
}

TEST(CommandPoolTest, AllocateBuffersFailsWithNullDevice)
{
    // Null device is rejected before the initialisation check.
    CommandPool pool;
    VkCommandBuffer buf{VK_NULL_HANDLE};
    EXPECT_EQ(pool.AllocateBuffers(VK_NULL_HANDLE, 1U, &buf), Result::kInvalidArgument);
}

TEST(CommandPoolTest, AllocateBuffersFailsWithZeroCount)
{
    CommandPool pool;
    VkCommandBuffer buf{VK_NULL_HANDLE};
    EXPECT_EQ(pool.AllocateBuffers(VK_NULL_HANDLE, 0U, &buf), Result::kInvalidArgument);
}

TEST(CommandPoolTest, AllocateBuffersFailsWithNullOutputArray)
{
    CommandPool pool;
    EXPECT_EQ(pool.AllocateBuffers(VK_NULL_HANDLE, 1U, nullptr), Result::kInvalidArgument);
}

} /* namespace engine::rendering */