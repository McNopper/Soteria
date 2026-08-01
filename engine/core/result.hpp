/// @file result.hpp
/// @brief Engine-wide result / error-code enumeration.
///
/// Exceptions are disabled (/EHs-c-).  All fallible functions return
/// Result and are marked [[nodiscard]] so callers cannot silently ignore them
/// (MISRA C++:2023 Rule 0.1.2).

#ifndef VKSC_ENGINE_CORE_RESULT_HPP
#define VKSC_ENGINE_CORE_RESULT_HPP

#include <cstdint>

namespace engine {

/// @brief Engine result code.
///
/// kOk  == 0 so that a zero-initialised variable indicates success.
/// All failure codes are negative to allow sign-check fast-paths.
enum class Result : int32_t
{
    kOk                      =   0,  ///< Operation succeeded.
    kError                   =  -1,  ///< Generic unclassified error.
    kInvalidArgument         =  -2,  ///< A required argument was null or out of range.
    kNotFound                =  -3,  ///< A required resource (device, queue, …) was not found.
    kVkscInstanceFailed      =  -4,  ///< vkCreateInstance returned a non-VK_SUCCESS code.
    kVkscDeviceFailed        =  -5,  ///< vkCreateDevice returned a non-VK_SUCCESS code.
    kVkscEnumerateFailed     =  -6,  ///< vkEnumeratePhysicalDevices failed or found no devices.
    kVkscCacheFailed         =  -7,  ///< vkCreatePipelineCache failed.
    kVkscSurfaceFailed       =  -8,  ///< vkCreateDisplayPlaneSurfaceKHR (or equivalent) failed.
    kVkscNoDisplay           =  -9,  ///< No physical display, mode, or plane was found.
    kVkscSwapchainFailed     = -10,  ///< vkCreateSwapchainKHR failed.
    kVkscRenderPassFailed    = -11,  ///< vkCreateRenderPass failed.
    kVkscPipelineLayoutFailed= -12,  ///< vkCreatePipelineLayout failed.
    kVkscPipelineFailed      = -13,  ///< vkCreateGraphicsPipelines failed or UUID mismatch.
    kVkscBufferFailed        = -14,  ///< vkCreateBuffer or vkAllocateMemory failed.
    kVkscCommandPoolFailed   = -15,  ///< vkCreateCommandPool or vkAllocateCommandBuffers failed.
    kVkscSyncFailed          = -16,  ///< vkCreateSemaphore or vkCreateFence failed.
    kVkscFramebufferFailed   = -17,  ///< vkCreateFramebuffer failed.
    kVkscAcquireFailed       = -18,  ///< vkAcquireNextImageKHR returned an unrecoverable error.
    kVkscPresentFailed       = -19,  ///< vkQueuePresentKHR returned an unrecoverable error.
    kVkscSurfaceLost         = -20,  ///< VK_ERROR_SURFACE_LOST_KHR detected; must recreate surface.
    kAlreadyInitialised      = -21,  ///< Init() was called on an already-initialised object.
    kVkscTimeoutFailed       = -22,  ///< vkWaitForFences timed out (possible GPU hang).
};

/// @brief Return true when @p r represents a successful outcome.
[[nodiscard]] constexpr bool IsOk(const Result r) noexcept
{
    return r == Result::kOk;
}

/// @brief Return a human-readable description of @p r.
///
/// The returned pointer is a string literal — its lifetime is the program lifetime.
///
/// MISRA C++:2023 Rule 9.4.2 requires a default label in every switch.
/// Exhaustiveness is still enforced by the compiler: -Wswitch-enum (enabled in
/// CMakeLists.txt) warns when an enumerator is not handled even though a
/// default label is present.  The default branch additionally protects against
/// out-of-range enum values produced by invalid casts.
[[nodiscard]] constexpr const char* ResultToString(const Result r) noexcept
{
    const char* str{"Unknown"};
    switch (r)
    {
        case Result::kOk:                       str = "Ok";                       break;
        case Result::kError:                    str = "Error";                    break;
        case Result::kInvalidArgument:          str = "InvalidArgument";          break;
        case Result::kNotFound:                 str = "NotFound";                 break;
        case Result::kVkscInstanceFailed:       str = "VkscInstanceFailed";       break;
        case Result::kVkscDeviceFailed:         str = "VkscDeviceFailed";         break;
        case Result::kVkscEnumerateFailed:      str = "VkscEnumerateFailed";      break;
        case Result::kVkscCacheFailed:          str = "VkscCacheFailed";          break;
        case Result::kVkscSurfaceFailed:        str = "VkscSurfaceFailed";        break;
        case Result::kVkscNoDisplay:            str = "VkscNoDisplay";            break;
        case Result::kVkscSwapchainFailed:      str = "VkscSwapchainFailed";      break;
        case Result::kVkscRenderPassFailed:     str = "VkscRenderPassFailed";     break;
        case Result::kVkscPipelineLayoutFailed: str = "VkscPipelineLayoutFailed"; break;
        case Result::kVkscPipelineFailed:       str = "VkscPipelineFailed";       break;
        case Result::kVkscBufferFailed:         str = "VkscBufferFailed";         break;
        case Result::kVkscCommandPoolFailed:    str = "VkscCommandPoolFailed";    break;
        case Result::kVkscSyncFailed:           str = "VkscSyncFailed";           break;
        case Result::kVkscFramebufferFailed:    str = "VkscFramebufferFailed";    break;
        case Result::kVkscAcquireFailed:        str = "VkscAcquireFailed";        break;
        case Result::kVkscPresentFailed:        str = "VkscPresentFailed";        break;
        case Result::kVkscSurfaceLost:          str = "VkscSurfaceLost";          break;
        case Result::kAlreadyInitialised:       str = "AlreadyInitialised";       break;
        case Result::kVkscTimeoutFailed:        str = "VkscTimeoutFailed";        break;
        default:                                str = "Unknown";                  break;
    }
    return str;
}

} /* namespace engine */

#endif /* VKSC_ENGINE_CORE_RESULT_HPP */
