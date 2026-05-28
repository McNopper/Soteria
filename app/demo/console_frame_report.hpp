/// @file console_frame_report.hpp
/// @brief Simulation-only frame report that logs metrics to stdout.
///
/// This is example/simulation code only.  A real system replaces this with
/// a health-monitor, data-recorder, or ARINC-429 telemetry transmitter.
///
/// Logs one line every kLogIntervalFrames frames to avoid flooding the
/// console during a 60 fps render loop.
///
/// Output is routed through engine::log::Info and is therefore governed by
/// the VKSC_ENABLE_LOGGING compile switch: no output is produced in
/// production (Release) builds.

#ifndef VKSC_SIM_DEMO_CONSOLE_FRAME_REPORT_HPP
#define VKSC_SIM_DEMO_CONSOLE_FRAME_REPORT_HPP

#include "../../engine/core/fixed_string.hpp"
#include "../../engine/core/log.hpp"
#include "../../engine/data/i_frame_report.hpp"

#include <cstdint>

namespace sim {
namespace demo {

/// @brief Console telemetry sink for stand-alone demonstration.
class ConsoleFrameReport final : public engine::data::IFrameReport
{
public:
    ConsoleFrameReport()  noexcept = default;
    ~ConsoleFrameReport() noexcept override = default;

    ConsoleFrameReport(const ConsoleFrameReport&)            = delete;
    ConsoleFrameReport& operator=(const ConsoleFrameReport&) = delete;
    ConsoleFrameReport(ConsoleFrameReport&&)                 = delete;
    ConsoleFrameReport& operator=(ConsoleFrameReport&&)      = delete;

    void OnFrameComplete(const engine::data::FrameMetrics& m) noexcept override
    {
        if ((m.frameNumber % kLogIntervalFrames) != 0ULL) { return; }

        if constexpr (engine::log::kEnabled)
        {
            engine::log::FixedString<128U> s;
            s.Append("[FRAME] #")
             .AppendU64Dec(m.frameNumber)
             .Append("  roll=")
             .AppendFloat(m.displayedRollDeg,  2U, true)
             .Append(" deg  pitch=")
             .AppendFloat(m.displayedPitchDeg, 2U, true)
             .Append(" deg  render=")
             .AppendFloat(m.renderTimeMs, 2U, false)
             .Append(" ms  present=")
             .Append(m.framePresented ? "OK" : "SKIP");
            engine::log::Info(s.CStr());
        }
    }

private:
    static constexpr uint64_t kLogIntervalFrames{60ULL};
};

} /* namespace demo */
} /* namespace sim */

#endif /* VKSC_SIM_DEMO_CONSOLE_FRAME_REPORT_HPP */
