/// @file i_frame_report.hpp
/// @brief Generic interface for per-frame rendering telemetry output.
///
/// Every completed render frame calls IFrameReport::OnFrameComplete() with
/// timing and display state.  Safety monitors, health watchdogs, recording
/// subsystems, and test harnesses implement this interface.
///
/// @satisfies SWS_DATA_003  IFrameReport decouples telemetry sink from renderer.
/// @satisfies SWS_DATA_004  OnFrameComplete is called exactly once per presented frame.
/// @satisfies SRS-TEL-001   Telemetry sink interface exposes per-frame timing metrics.
/// @satisfies SRS-TEL-002   OnFrameComplete must be non-blocking and non-allocating.

#ifndef VKSC_ENGINE_DATA_I_FRAME_REPORT_HPP
#define VKSC_ENGINE_DATA_I_FRAME_REPORT_HPP

#include <cstdint>

namespace engine {
namespace data {

/// @brief Telemetry captured for one completed render frame.
struct FrameMetrics
{
    uint64_t frameNumber{0U};         ///< Monotonically increasing frame counter (starts at 0).
    float    renderTimeMs{0.0F};      ///< GPU submit wall time in milliseconds.
    float    presentTimeMs{0.0F};     ///< vkQueuePresentKHR wall time in milliseconds.
    float    displayedRollDeg{0.0F};  ///< Roll angle actually rendered.
    float    displayedPitchDeg{0.0F}; ///< Pitch angle actually rendered.
    bool     framePresented{false};   ///< False if present was skipped due to an error.
};

/// @brief Sink for per-frame telemetry -- pure abstract interface.
///
/// Implementations must be non-copyable, non-movable.  OnFrameComplete()
/// must be non-blocking and must not allocate memory.
class IFrameReport
{
public:
    /// @brief Called once per frame after submit and present complete.
    ///
    /// @param metrics  Telemetry for the frame that just completed.
    virtual void OnFrameComplete(const FrameMetrics& metrics) noexcept = 0;

    IFrameReport()                    noexcept = default;
    virtual ~IFrameReport()             noexcept = default;
    IFrameReport(const IFrameReport&)            = delete;
    IFrameReport& operator=(const IFrameReport&) = delete;
    IFrameReport(IFrameReport&&)                 = delete;
    IFrameReport& operator=(IFrameReport&&)      = delete;
};

} /* namespace data */
} /* namespace engine */

#endif /* VKSC_ENGINE_DATA_I_FRAME_REPORT_HPP */
