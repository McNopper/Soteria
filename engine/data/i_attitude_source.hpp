/// @file i_attitude_source.hpp
/// @brief Generic interface for attitude and navigation data input.
///
/// Any sensor-fusion module, simulation data generator, ARINC-429 reader,
/// or CAN-bus parser implements IAttitudeSource.  The rendering layer
/// receives data through this interface and has no dependency on the
/// data source implementation.
///
/// AttitudeData fields use aviation sign conventions:
///   rollDeg    : positive = right bank (right wing down).
///   pitchDeg   : positive = nose up.
///   headingDeg : 0..360 (magnetic), 0 = North.
///   airspeedKt : calibrated airspeed in knots (>= 0).
///   altitudeFt : barometric altitude in feet.
///   valid      : false when the data source reports a sensor failure or
///                data age has exceeded the declared maximum latency.
///
/// AttitudeData.valid must be checked before rendering; when false the
/// renderer must display a failure indication.

#ifndef VKSC_ENGINE_DATA_I_ATTITUDE_SOURCE_HPP
#define VKSC_ENGINE_DATA_I_ATTITUDE_SOURCE_HPP

#include <cstdint>

namespace engine {
namespace data {

/// @brief One snapshot of attitude and navigation state.
struct AttitudeData
{
    float    rollDeg{0.0F};     ///< Roll angle in degrees (right-bank positive).
    float    pitchDeg{0.0F};    ///< Pitch angle in degrees (nose-up positive).
    float    headingDeg{0.0F};  ///< Magnetic heading (0..360 degrees).
    float    airspeedKt{0.0F};  ///< Calibrated airspeed in knots.
    float    altitudeFt{0.0F};  ///< Barometric altitude in feet.
    bool     valid{false};      ///< False when data is stale or source has failed.
};

/// @brief Source of attitude data -- pure abstract interface.
///
/// Implementations must be non-copyable, non-movable.  GetAttitude()
/// must be wait-free and must not allocate memory.
class IAttitudeSource
{
public:
    /// @brief Return the latest attitude snapshot.
    ///
    /// Called once per render frame.  If valid is false the renderer must
    /// display a failure indication and must not use the numeric fields.
    ///
    /// @returns AttitudeData with valid == false on sensor failure.
    [[nodiscard]] virtual AttitudeData GetAttitude() const noexcept = 0;

    IAttitudeSource()                  noexcept = default;
    virtual ~IAttitudeSource()                noexcept = default;
    IAttitudeSource(const IAttitudeSource&)            = delete;
    IAttitudeSource& operator=(const IAttitudeSource&) = delete;
    IAttitudeSource(IAttitudeSource&&)                 = delete;
    IAttitudeSource& operator=(IAttitudeSource&&)      = delete;
};

} /* namespace data */
} /* namespace engine */

#endif /* VKSC_ENGINE_DATA_I_ATTITUDE_SOURCE_HPP */
