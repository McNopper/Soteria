/// @file i_fault_sink.hpp
/// @brief Sink interface for Vulkan SC device faults (vkGetFaultData).
///
/// Fault handling is core Vulkan SC 1.0: the driver records faults
/// (VkFaultLevel / VkFaultType) that the application retrieves with
/// vkGetFaultData.  VkscContext::DrainFaults polls the driver and forwards
/// every recorded fault to an IFaultSink implementation.
///
/// Design decision — polling instead of VkFaultCallbackInfo:
/// The spec also offers a fault callback registered at device creation.
/// Soteria deliberately polls once per frame instead:
///   * PFN_vkFaultCallbackFunction carries no user-context pointer, so
///     reaching a sink from the trampoline would require global/static
///     state (MISRA C++:2023 Rules 6.7.1/6.7.2).
///   * The callback must not call any Vulkan commands and runs mid-command
///     on the API thread — an awkward place for safety reactions.
///   * Vulkan SC removed asynchronous callbacks for determinism (SCID-3);
///     a per-frame poll keeps fault handling on the deterministic frame
///     schedule with a worst-case latency of one frame.
/// Both mechanisms draw from the same driver-side fault record, and each
/// unique fault is reported at most once per mechanism.
///
/// The enums below mirror VkFaultLevel / VkFaultType so this interface
/// stays Vulkan-free (layer rule: interfaces must remain testable without
/// the SDK).  VkscContext performs the translation.

#ifndef VKSC_ENGINE_CORE_I_FAULT_SINK_HPP
#define VKSC_ENGINE_CORE_I_FAULT_SINK_HPP

#include <cstdint>

namespace engine {
namespace core {

/// @brief Mirrors VkFaultLevel (severity of a recorded fault).
enum class FaultLevel : uint32_t
{
    eUnassigned  = 0U,
    eWarning     = 1U,
    eRecoverable = 2U,
    eCritical    = 3U,
};

/// @brief Mirrors VkFaultType (origin of a recorded fault).
enum class FaultType : uint32_t
{
    eInvalid           = 0U,
    eUnassigned        = 1U,
    eImplementation    = 2U,
    eSystem            = 3U,
    ePhysicalDevice    = 4U,
    eCommandBufferFull = 5U,
    eInvalidApiUsage   = 6U,
};

/// @brief Sink for driver-recorded faults — pure abstract interface.
///
/// Implementations must be non-blocking, wait-free and must not allocate:
/// OnFault runs on the render thread once per recorded fault per frame.
class IFaultSink
{
public:
    /// @brief Called once per recorded fault.
    ///
    /// @param level  Fault severity (eCritical ⇒ treat as unrecoverable).
    /// @param type   Fault origin classification.
    virtual void OnFault(FaultLevel level, FaultType type) noexcept = 0;

    /// @brief Called when the driver detected more faults than it recorded.
    ///
    /// The driver-side record is bounded by maxQueryFaultCount; an overflow
    /// means fault information was lost — itself a reportable condition.
    virtual void OnUnrecordedFaults() noexcept = 0;

    IFaultSink()                     noexcept = default;
    virtual ~IFaultSink()            noexcept = default;
    IFaultSink(const IFaultSink&)            = delete;
    IFaultSink& operator=(const IFaultSink&) = delete;
    IFaultSink(IFaultSink&&)                 = delete;
    IFaultSink& operator=(IFaultSink&&)      = delete;
};

} /* namespace core */
} /* namespace engine */

#endif /* VKSC_ENGINE_CORE_I_FAULT_SINK_HPP */
