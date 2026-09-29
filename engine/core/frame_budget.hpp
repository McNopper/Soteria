/// @file frame_budget.hpp
/// @brief Deterministic frame-deadline monitor — pure arithmetic, no Vulkan.
///
/// Safety-critical displays must guarantee a fresh frame every display
/// period.  FrameBudgetMonitor records per-frame render times against a
/// configurable deadline and tracks deadline misses.  When misses are
/// consecutive for long enough (maxConsecutiveMisses) the monitor escalates:
/// the caller is expected to raise a health event (see
/// engine/data/i_health_monitor.hpp) and transition to the system safe state.
///
/// The monitor itself owns no reaction policy — detection and reaction are
/// separated so the detection logic stays trivially testable offline.
///
/// No dynamic memory, no C library calls, noexcept throughout.

#ifndef VKSC_ENGINE_CORE_FRAME_BUDGET_HPP
#define VKSC_ENGINE_CORE_FRAME_BUDGET_HPP

#include <cmath>    // std::isfinite
#include <cstdint>

namespace engine {
namespace core {

/// @brief Per-frame render-time budget monitor.
///
/// Non-copyable, non-movable.  All methods are wait-free O(1).
class FrameBudgetMonitor
{
public:
    /// @brief Monitor configuration.
    struct Config
    {
        /// Frame deadline in milliseconds.  Frames whose render time exceeds
        /// this value count as misses.  Must be > 0; otherwise Init clamps
        /// to the default 60 Hz budget.
        float deadlineMs{1000.0F / 60.0F};

        /// Number of consecutive misses that triggers escalation.
        /// 0 disables escalation (misses are still counted).
        uint32_t maxConsecutiveMisses{3U};
    };

    FrameBudgetMonitor()  noexcept = default;
    ~FrameBudgetMonitor() noexcept = default;

    FrameBudgetMonitor(const FrameBudgetMonitor&)            = delete;
    FrameBudgetMonitor& operator=(const FrameBudgetMonitor&) = delete;
    FrameBudgetMonitor(FrameBudgetMonitor&&)                 = delete;
    FrameBudgetMonitor& operator=(FrameBudgetMonitor&&)      = delete;

    /// @brief (Re)initialise with @p cfg and clear all counters.
    void Init(const Config& cfg) noexcept
    {
        m_deadlineMs = (cfg.deadlineMs > 0.0F) ? cfg.deadlineMs : kDefaultDeadlineMs;
        m_maxConsecutiveMisses = cfg.maxConsecutiveMisses;
        Reset();
    }

    /// @brief Record one completed frame's render time.
    ///
    /// A non-finite @p renderMs (NaN/Inf) is treated as a miss: a broken
    /// time measurement must never masquerade as a healthy frame.
    ///
    /// @returns true when the frame was within budget.
    [[nodiscard]] bool RecordFrame(const float renderMs) noexcept
    {
        ++m_frameCount;
        const bool withinBudget = std::isfinite(renderMs) && (renderMs <= m_deadlineMs);
        if (withinBudget)
        {
            m_consecutiveMisses = 0U;
        }
        else
        {
            ++m_totalMisses;
            if (m_consecutiveMisses < UINT32_MAX)
            {
                ++m_consecutiveMisses;
            }
        }
        return withinBudget;
    }

    /// @brief Clear all counters (deadline configuration is kept).
    void Reset() noexcept
    {
        m_frameCount         = 0U;
        m_totalMisses        = 0U;
        m_consecutiveMisses  = 0U;
    }

    /// @returns true when consecutive misses have reached the escalation
    ///          threshold configured in Init (0 = never escalates).
    [[nodiscard]] bool Escalated() const noexcept
    {
        return (m_maxConsecutiveMisses > 0U) &&
               (m_consecutiveMisses >= m_maxConsecutiveMisses);
    }

    [[nodiscard]] uint64_t FrameCount()        const noexcept { return m_frameCount; }
    [[nodiscard]] uint32_t TotalMisses()       const noexcept { return m_totalMisses; }
    [[nodiscard]] uint32_t ConsecutiveMisses() const noexcept { return m_consecutiveMisses; }
    [[nodiscard]] float    DeadlineMs()        const noexcept { return m_deadlineMs; }

private:
    static constexpr float kDefaultDeadlineMs{1000.0F / 60.0F};

    float    m_deadlineMs{kDefaultDeadlineMs};
    uint32_t m_maxConsecutiveMisses{3U};
    uint64_t m_frameCount{0U};
    uint32_t m_totalMisses{0U};
    uint32_t m_consecutiveMisses{0U};
};

} /* namespace core */
} /* namespace engine */

#endif /* VKSC_ENGINE_CORE_FRAME_BUDGET_HPP */
