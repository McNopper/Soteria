/// @file context_reinit_qual_test.cpp
/// @brief Qualification test for the VkscContext double-init guard.
///
/// Verifies: "A second call to Init() on an already-initialised context shall
/// return kAlreadyInitialised without modifying state."

#include <gtest/gtest.h>
#include "engine/core/vksc_context.hpp"

namespace engine {

TEST(Qualification_ContextReinit, SecondInitReturnsAlreadyInitialised)
{
    VkscContext ctx;
    VkscContextConfig cfg{};

    Result r = ctx.Init(cfg);
    if (!IsOk(r)) {
        GTEST_SKIP() << "VulkanSC not available (" << ResultToString(r) << ")";
    }

    // State before the second call.
    VkDevice deviceBefore = ctx.Device();

    EXPECT_EQ(ctx.Init(cfg), Result::kAlreadyInitialised);

    // State must be unchanged.
    EXPECT_TRUE(ctx.IsInitialised());
    EXPECT_EQ(ctx.Device(), deviceBefore);

    ctx.Shutdown();
}

} /* namespace engine */
