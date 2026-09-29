/// @file frame_budget_test.cpp
/// @brief Unit tests for engine::core::FrameBudgetMonitor.
///
/// The monitor is pure arithmetic — these tests run fully offline.

#include <gtest/gtest.h>
#include "engine/core/frame_budget.hpp"

#include <limits>

namespace engine {
namespace core {

namespace {

FrameBudgetMonitor::Config MakeConfig(const float deadlineMs,
                                      const uint32_t maxConsecutive)
{
    FrameBudgetMonitor::Config cfg{};
    cfg.deadlineMs           = deadlineMs;
    cfg.maxConsecutiveMisses = maxConsecutive;
    return cfg;
}

} /* anonymous namespace */

TEST(FrameBudgetTest, DefaultsAreNotEscalated)
{
    FrameBudgetMonitor m;
    EXPECT_FALSE(m.Escalated());
    EXPECT_EQ(m.FrameCount(),        0U);
    EXPECT_EQ(m.TotalMisses(),       0U);
    EXPECT_EQ(m.ConsecutiveMisses(), 0U);
}

TEST(FrameBudgetTest, WithinBudgetFrameIsNotAMiss)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 3U));
    EXPECT_TRUE(m.RecordFrame(5.0F));
    EXPECT_EQ(m.FrameCount(),  1U);
    EXPECT_EQ(m.TotalMisses(), 0U);
}

TEST(FrameBudgetTest, OverBudgetFrameIsAMiss)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 3U));
    EXPECT_FALSE(m.RecordFrame(10.1F));
    EXPECT_EQ(m.TotalMisses(),       1U);
    EXPECT_EQ(m.ConsecutiveMisses(), 1U);
}

TEST(FrameBudgetTest, ExactDeadlineIsWithinBudget)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 3U));
    EXPECT_TRUE(m.RecordFrame(10.0F));
    EXPECT_EQ(m.TotalMisses(), 0U);
}

TEST(FrameBudgetTest, SuccessResetsConsecutiveButNotTotal)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 3U));
    (void)m.RecordFrame(11.0F);
    (void)m.RecordFrame(12.0F);
    (void)m.RecordFrame(1.0F);
    EXPECT_EQ(m.TotalMisses(),       2U);
    EXPECT_EQ(m.ConsecutiveMisses(), 0U);
}

TEST(FrameBudgetTest, EscalatesAfterConfiguredConsecutiveMisses)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 3U));
    (void)m.RecordFrame(11.0F);
    EXPECT_FALSE(m.Escalated());
    (void)m.RecordFrame(11.0F);
    EXPECT_FALSE(m.Escalated());
    (void)m.RecordFrame(11.0F);
    EXPECT_TRUE(m.Escalated());
}

TEST(FrameBudgetTest, EscalationDisabledWhenThresholdIsZero)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 0U));
    for (uint32_t i{0U}; i < 10U; ++i) { (void)m.RecordFrame(11.0F); }
    EXPECT_EQ(m.TotalMisses(), 10U);
    EXPECT_FALSE(m.Escalated());
}

TEST(FrameBudgetTest, NonFiniteRenderTimeCountsAsMiss)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 2U));
    EXPECT_FALSE(m.RecordFrame(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(m.RecordFrame(std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(m.Escalated());
}

TEST(FrameBudgetTest, ZeroOrNegativeDeadlineFallsBackToDefault)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(0.0F, 3U));
    EXPECT_FLOAT_EQ(m.DeadlineMs(), 1000.0F / 60.0F);
    m.Init(MakeConfig(-5.0F, 3U));
    EXPECT_FLOAT_EQ(m.DeadlineMs(), 1000.0F / 60.0F);
}

TEST(FrameBudgetTest, ResetClearsCountersKeepsDeadline)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 1U));
    (void)m.RecordFrame(11.0F);
    EXPECT_TRUE(m.Escalated());
    m.Reset();
    EXPECT_FALSE(m.Escalated());
    EXPECT_EQ(m.FrameCount(),        0U);
    EXPECT_EQ(m.TotalMisses(),       0U);
    EXPECT_EQ(m.ConsecutiveMisses(), 0U);
    EXPECT_FLOAT_EQ(m.DeadlineMs(), 10.0F);
}

TEST(FrameBudgetTest, LongRunOfMissesKeepsCounting)
{
    FrameBudgetMonitor m;
    m.Init(MakeConfig(10.0F, 0U));
    for (uint32_t i{0U}; i < 1000U; ++i) { (void)m.RecordFrame(11.0F); }
    EXPECT_EQ(m.ConsecutiveMisses(), 1000U);
    EXPECT_EQ(m.TotalMisses(),       1000U);
}

} /* namespace core */
} /* namespace engine */
