/// @file i_health_monitor.hpp
/// @brief Generic interface for classified fault/health events.
///
/// A safety-critical display engine must not silently swallow faults: every
/// abnormal condition detected in the frame path (GPU hang, lost device,
/// failed present, invalid input data, repeated frame-deadline misses, …)
/// is reported through IHealthMonitor.  The implementation owns the reaction
/// policy — latch the event, request the system safe state, notify an
/// external supervisor — the engine only detects and reports.
///
/// This separation keeps the certifiable detection path small and lets each
/// program (avionics, automotive cluster, medical display) define its own
/// safe-state reaction.
///
/// Implementations must be non-blocking, wait-free and must not allocate
/// memory: OnHealthEvent is called on the render thread, potentially while
/// the system is already in a degraded state.

#ifndef VKSC_ENGINE_DATA_I_HEALTH_MONITOR_HPP
#define VKSC_ENGINE_DATA_I_HEALTH_MONITOR_HPP

#include <cstdint>

namespace engine {
namespace data {

/// @brief Classified fault/health events detected by the engine frame path.
enum class HealthEvent : uint32_t
{
    eGpuHang           = 0U,  ///< Fence wait timed out — possible GPU fault.
    eDeviceLost        = 1U,  ///< VK_ERROR_DEVICE_LOST from any Vulkan call.
    eSurfaceLost       = 2U,  ///< VK_ERROR_SURFACE_LOST_KHR detected.
    eAcquireFailed     = 3U,  ///< vkAcquireNextImageKHR unrecoverable error.
    ePresentFailed     = 4U,  ///< vkQueuePresentKHR unrecoverable error.
    eSubmitFailed      = 5U,  ///< vkQueueSubmit failed.
    eRecordFailed      = 6U,  ///< Frame renderer returned VK_NULL_HANDLE.
    eInputDataInvalid  = 7U,  ///< Input snapshot invalid this frame (sensor fault).
    eFrameDeadlineMiss = 8U,  ///< Frame budget monitor escalated (sustained misses).
    eDriverFault       = 9U,  ///< Critical fault recorded by the Vulkan SC driver
                              ///< (vkGetFaultData, VkFaultLevel::CRITICAL).
};

/// @brief Sink for classified health events — pure abstract interface.
///
/// Implementations are non-copyable, non-movable.
class IHealthMonitor
{
public:
    /// @brief Called once per detected health event, on the render thread.
    ///
    /// @param event        Classified event.
    /// @param frameNumber  Frame counter at detection time.
    virtual void OnHealthEvent(HealthEvent event, uint64_t frameNumber) noexcept = 0;

    IHealthMonitor()                       noexcept = default;
    virtual ~IHealthMonitor()              noexcept = default;
    IHealthMonitor(const IHealthMonitor&)            = delete;
    IHealthMonitor& operator=(const IHealthMonitor&) = delete;
    IHealthMonitor(IHealthMonitor&&)                 = delete;
    IHealthMonitor& operator=(IHealthMonitor&&)      = delete;
};

/// @brief Heartbeat seam for external watchdog supervision.
///
/// An external watchdog (hardware timer, supervisor partition, separate
/// process) observes Kick() calls; their absence within the watchdog period
/// proves the render loop has stalled even when no fault was detected
/// (e.g. an infinite loop outside the fence-wait path).
///
/// Kept separate from IHealthMonitor: heartbeats are periodic and expected,
/// health events are exceptional.  A supervisor may implement both.
class IHeartbeat
{
public:
    /// @brief Called once per successfully presented frame.
    ///
    /// Must be wait-free and must not allocate memory.
    ///
    /// @param frameNumber  Frame counter of the frame just presented.
    virtual void Kick(uint64_t frameNumber) noexcept = 0;

    IHeartbeat()                   noexcept = default;
    virtual ~IHeartbeat()          noexcept = default;
    IHeartbeat(const IHeartbeat&)            = delete;
    IHeartbeat& operator=(const IHeartbeat&) = delete;
    IHeartbeat(IHeartbeat&&)                 = delete;
    IHeartbeat& operator=(IHeartbeat&&)      = delete;
};

} /* namespace data */
} /* namespace engine */

#endif /* VKSC_ENGINE_DATA_I_HEALTH_MONITOR_HPP */
