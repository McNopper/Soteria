/// @file console_health_monitor.hpp
/// @brief Simulation-only health monitor + heartbeat that log to stdout.
///
/// This is example/simulation code only.  A real system replaces this with
/// a supervisor that latches faults, drives the hardware safe-state outputs
/// and services an external watchdog.
///
/// Events are rate-limited to one log line per event kind per
/// kLogIntervalFrames frames so a sustained fault (e.g. permanently invalid
/// sensor data) does not flood the console at 60 fps.  The heartbeat is
/// silent — it exists to demonstrate the seam; a real watchdog observes the
/// Kick() calls themselves.

#ifndef VKSC_SIM_DEMO_CONSOLE_HEALTH_MONITOR_HPP
#define VKSC_SIM_DEMO_CONSOLE_HEALTH_MONITOR_HPP

#include "../../engine/core/fixed_string.hpp"
#include "../../engine/core/log.hpp"
#include "../../engine/core/i_fault_sink.hpp"
#include "../../engine/data/i_health_monitor.hpp"

#include <array>
#include <cstdint>

namespace sim {
namespace demo {

/// @brief Console health monitor + heartbeat for stand-alone demonstration.
class ConsoleHealthMonitor final : public engine::data::IHealthMonitor,
                                    public engine::data::IHeartbeat,
                                    public engine::core::IFaultSink
{
public:
    ConsoleHealthMonitor()  noexcept = default;
    ~ConsoleHealthMonitor() noexcept override = default;

    ConsoleHealthMonitor(const ConsoleHealthMonitor&)            = delete;
    ConsoleHealthMonitor& operator=(const ConsoleHealthMonitor&) = delete;
    ConsoleHealthMonitor(ConsoleHealthMonitor&&)                 = delete;
    ConsoleHealthMonitor& operator=(ConsoleHealthMonitor&&)      = delete;

    void OnHealthEvent(const engine::data::HealthEvent event,
                       const uint64_t frameNumber) noexcept override
    {
        const uint32_t idx = static_cast<uint32_t>(event);
        if (idx >= kEventCount) { return; }

        // Rate-limit per event kind; the first occurrence always logs.
        const uint64_t seen = m_eventCounts[idx];
        m_eventCounts[idx]  = seen + 1U;
        if ((seen != 0U) && ((frameNumber % kLogIntervalFrames) != 0U)) { return; }

        if constexpr (engine::log::kEnabled)
        {
            engine::log::FixedString<96U> s;
            s.Append("[HEALTH] frame=")
             .AppendU64Dec(frameNumber)
             .Append("  event=")
             .Append(EventName(event));
            engine::log::Warn(s.CStr());
        }
    }

    void Kick(const uint64_t /*frameNumber*/) noexcept override
    {
        // Demonstration seam only — a real watchdog observes these calls.
    }

    // ---- IFaultSink --------------------------------------------------------

    void OnFault(const engine::core::FaultLevel level,
                 const engine::core::FaultType  type) noexcept override
    {
        if constexpr (engine::log::kEnabled)
        {
            engine::log::FixedString<96U> s;
            s.Append("[FAULT]  level=")
             .Append(LevelName(level))
             .Append("  type=")
             .Append(TypeName(type));
            engine::log::Warn(s.CStr());
        }
        if (level == engine::core::FaultLevel::eCritical)
        {
            // A critical driver fault is a health event; WARNING/RECOVERABLE
            // entries are logged for the certification record only.
            OnHealthEvent(engine::data::HealthEvent::eDriverFault, m_faultFrame++);
        }
    }

    void OnUnrecordedFaults() noexcept override
    {
        // The driver lost fault records — reportable on its own.
        OnHealthEvent(engine::data::HealthEvent::eDriverFault, m_faultFrame++);
    }

    /// @returns Number of times @p event has been reported (test/demo introspection).
    [[nodiscard]] uint64_t EventCount(const engine::data::HealthEvent event) const noexcept
    {
        const uint32_t idx = static_cast<uint32_t>(event);
        return (idx < kEventCount) ? m_eventCounts[idx] : 0U;
    }

private:
    static constexpr uint32_t kEventCount{10U};  // one slot per HealthEvent enumerator
    static constexpr uint64_t kLogIntervalFrames{60U};

    [[nodiscard]] static const char* LevelName(const engine::core::FaultLevel level) noexcept
    {
        using engine::core::FaultLevel;
        const char* name{"?"};
        switch (level)
        {
            case FaultLevel::eWarning:     name = "WARNING";     break;
            case FaultLevel::eRecoverable: name = "RECOVERABLE"; break;
            case FaultLevel::eCritical:    name = "CRITICAL";    break;
            case FaultLevel::eUnassigned:
            default:                       name = "UNASSIGNED";  break;
        }
        return name;
    }

    [[nodiscard]] static const char* TypeName(const engine::core::FaultType type) noexcept
    {
        using engine::core::FaultType;
        const char* name{"?"};
        switch (type)
        {
            case FaultType::eInvalid:           name = "INVALID";             break;
            case FaultType::eUnassigned:        name = "UNASSIGNED";          break;
            case FaultType::eImplementation:    name = "IMPLEMENTATION";      break;
            case FaultType::eSystem:            name = "SYSTEM";              break;
            case FaultType::ePhysicalDevice:    name = "PHYSICAL_DEVICE";     break;
            case FaultType::eCommandBufferFull: name = "COMMAND_BUFFER_FULL"; break;
            case FaultType::eInvalidApiUsage:   name = "INVALID_API_USAGE";   break;
            default:                            name = "?";                   break;
        }
        return name;
    }

    [[nodiscard]] static const char* EventName(const engine::data::HealthEvent event) noexcept
    {
        using engine::data::HealthEvent;
        const char* name{"Unknown"};
        switch (event)
        {
            case HealthEvent::eGpuHang:           name = "GpuHang";           break;
            case HealthEvent::eDeviceLost:        name = "DeviceLost";        break;
            case HealthEvent::eSurfaceLost:       name = "SurfaceLost";       break;
            case HealthEvent::eAcquireFailed:     name = "AcquireFailed";     break;
            case HealthEvent::ePresentFailed:     name = "PresentFailed";     break;
            case HealthEvent::eSubmitFailed:      name = "SubmitFailed";      break;
            case HealthEvent::eRecordFailed:      name = "RecordFailed";      break;
            case HealthEvent::eInputDataInvalid:  name = "InputDataInvalid";  break;
            case HealthEvent::eFrameDeadlineMiss: name = "FrameDeadlineMiss"; break;
            case HealthEvent::eDriverFault:       name = "DriverFault";       break;
            default:                              name = "Unknown";           break;
        }
        return name;
    }

    std::array<uint64_t, kEventCount> m_eventCounts{};
    uint64_t                          m_faultFrame{0U};
};

} /* namespace demo */
} /* namespace sim */

#endif /* VKSC_SIM_DEMO_CONSOLE_HEALTH_MONITOR_HPP */
