/// @file demo_attitude_source.hpp
/// @brief Simulation-only attitude source that oscillates roll and pitch.
///
/// This is example/simulation code only.  It is NOT part of the certifiable
/// engine layer.  A real system replaces this with a sensor-fusion module or
/// ARINC-429/CAN-bus reader that implements IAttitudeSource.
///
/// Roll : +/- 25 degrees, full period 8 seconds.
/// Pitch: +/- 10 degrees, full period 6 seconds.
/// Data is always reported as valid (no failure injection in this demo).

#ifndef VKSC_SIM_DEMO_ATTITUDE_SOURCE_HPP
#define VKSC_SIM_DEMO_ATTITUDE_SOURCE_HPP

#include "../../engine/data/i_attitude_source.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cmath>
#include <cstdint>

namespace sim {
namespace demo {

/// @brief Oscillating attitude source for stand-alone demonstration.
class DemoAttitudeSource final : public engine::data::IAttitudeSource
{
public:
    DemoAttitudeSource()  noexcept = default;
    ~DemoAttitudeSource() noexcept override = default;

    DemoAttitudeSource(const DemoAttitudeSource&)            = delete;
    DemoAttitudeSource& operator=(const DemoAttitudeSource&) = delete;
    DemoAttitudeSource(DemoAttitudeSource&&)                 = delete;
    DemoAttitudeSource& operator=(DemoAttitudeSource&&)      = delete;

    [[nodiscard]] engine::data::AttitudeData GetAttitude() const noexcept override
    {
        const float tSec =
            static_cast<float>(GetTickCount64()) * 1.0e-3F;

        engine::data::AttitudeData data{};
        data.rollDeg    = 25.0F  * std::sin(kTwoPi * tSec / 8.0F);
        data.pitchDeg   = 10.0F  * std::sin(kTwoPi * tSec / 6.0F);
        data.headingDeg = 0.0F;
        data.airspeedKt = 120.0F;
        data.altitudeFt = 3500.0F;
        data.valid      = true;
        return data;
    }

private:
    static constexpr float kTwoPi{6.28318530717958647692F};
};

} /* namespace demo */
} /* namespace sim */

#endif /* VKSC_SIM_DEMO_ATTITUDE_SOURCE_HPP */
