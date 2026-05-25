/// @file display_output.cpp
/// @brief VK_KHR_display surface creation implementation.
///
/// Uses only fixed-size stack arrays (no dynamic allocation).
/// Every error path logs a message and returns a specific Result code.

#include "display_output.hpp"
#include "../core/log.hpp"

#include <cstdint>

namespace engine {
namespace wsi {

// ---- Init -------------------------------------------------------------------

Result DisplayOutput::Init(VkInstance instance, VkPhysicalDevice pd) noexcept
{
    if (m_initialised)
    {
        log::Error("DisplayOutput::Init called on already-initialised object.");
        return Result::kAlreadyInitialised;
    }
    if ((instance == VK_NULL_HANDLE) || (pd == VK_NULL_HANDLE))
    {
        log::Error("DisplayOutput::Init: null instance or physical device.");
        return Result::kInvalidArgument;
    }

    m_instance = instance;  /* store for Shutdown */

    // ---- Enumerate displays -------------------------------------------------
    uint32_t displayCount{0U};
    VkResult r = vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &displayCount, nullptr);
    if ((r != VK_SUCCESS) || (displayCount == 0U))
    {
        log::Error("DisplayOutput: no physical displays found.");
        return Result::kVkscNoDisplay;
    }
    if (displayCount > kMaxDisplays) { displayCount = kMaxDisplays; }

    VkDisplayPropertiesKHR displays[kMaxDisplays]{};
    r = vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &displayCount, displays);
    if (r != VK_SUCCESS)
    {
        log::Error("DisplayOutput: vkGetPhysicalDeviceDisplayPropertiesKHR failed.");
        return Result::kVkscNoDisplay;
    }

    const VkDisplayKHR chosenDisplay = displays[0U].display;
    log::Info("DisplayOutput: using display:");
    log::Info((displays[0U].displayName != nullptr) ? displays[0U].displayName : "(unnamed)");

    // ---- Enumerate display modes --------------------------------------------
    uint32_t modeCount{0U};
    r = vkGetDisplayModePropertiesKHR(pd, chosenDisplay, &modeCount, nullptr);
    if ((r != VK_SUCCESS) || (modeCount == 0U))
    {
        log::Error("DisplayOutput: no display modes found.");
        return Result::kVkscNoDisplay;
    }
    if (modeCount > kMaxModes) { modeCount = kMaxModes; }

    VkDisplayModePropertiesKHR modes[kMaxModes]{};
    r = vkGetDisplayModePropertiesKHR(pd, chosenDisplay, &modeCount, modes);
    if (r != VK_SUCCESS)
    {
        log::Error("DisplayOutput: vkGetDisplayModePropertiesKHR failed.");
        return Result::kVkscNoDisplay;
    }

    // Select mode with the largest pixel count (width * height).
    uint32_t bestModeIdx{0U};
    uint32_t bestPixels{0U};
    for (uint32_t m{0U}; m < modeCount; ++m)
    {
        const VkExtent2D& ext = modes[m].parameters.visibleRegion;
        const uint32_t    pixels = ext.width * ext.height;
        if (pixels > bestPixels)
        {
            bestPixels  = pixels;
            bestModeIdx = m;
        }
    }
    const VkDisplayModePropertiesKHR& bestMode = modes[bestModeIdx];

    // ---- Enumerate display planes -------------------------------------------
    uint32_t planeCount{0U};
    r = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(pd, &planeCount, nullptr);
    if ((r != VK_SUCCESS) || (planeCount == 0U))
    {
        log::Error("DisplayOutput: no display planes found.");
        return Result::kVkscNoDisplay;
    }
    if (planeCount > kMaxPlanes) { planeCount = kMaxPlanes; }

    VkDisplayPlanePropertiesKHR planes[kMaxPlanes]{};
    r = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(pd, &planeCount, planes);
    if (r != VK_SUCCESS)
    {
        log::Error("DisplayOutput: vkGetPhysicalDeviceDisplayPlanePropertiesKHR failed.");
        return Result::kVkscNoDisplay;
    }

    // Find a plane that supports the chosen display.
    uint32_t chosenPlaneIdx{kInvalidIndex};
    for (uint32_t p{0U}; p < planeCount; ++p)
    {
        uint32_t supportedCount{0U};
        r = vkGetDisplayPlaneSupportedDisplaysKHR(pd, p, &supportedCount, nullptr);
        if ((r != VK_SUCCESS) || (supportedCount == 0U)) { continue; }

        VkDisplayKHR supportedDisplays[kMaxDisplays]{};
        if (supportedCount > kMaxDisplays) { supportedCount = kMaxDisplays; }
        r = vkGetDisplayPlaneSupportedDisplaysKHR(pd, p, &supportedCount, supportedDisplays);
        if (r != VK_SUCCESS) { continue; }

        for (uint32_t sd{0U}; sd < supportedCount; ++sd)
        {
            if (supportedDisplays[sd] == chosenDisplay)
            {
                chosenPlaneIdx = p;
                break;
            }
        }
        if (chosenPlaneIdx != kInvalidIndex) { break; }
    }

    if (chosenPlaneIdx == kInvalidIndex)
    {
        log::Error("DisplayOutput: no plane supports the chosen display.");
        return Result::kVkscNoDisplay;
    }

    // ---- Create display plane surface ---------------------------------------
    VkDisplaySurfaceCreateInfoKHR surfaceCI{};
    surfaceCI.sType           = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    surfaceCI.pNext           = nullptr;
    surfaceCI.flags           = 0U;
    surfaceCI.displayMode     = bestMode.displayMode;
    surfaceCI.planeIndex      = chosenPlaneIdx;
    surfaceCI.planeStackIndex = planes[chosenPlaneIdx].currentStackIndex;
    surfaceCI.transform       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surfaceCI.globalAlpha     = 1.0F;
    surfaceCI.alphaMode       = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    surfaceCI.imageExtent     = bestMode.parameters.visibleRegion;

    r = vkCreateDisplayPlaneSurfaceKHR(instance, &surfaceCI, nullptr, &m_surface);
    if (r != VK_SUCCESS)
    {
        log::Error("DisplayOutput: vkCreateDisplayPlaneSurfaceKHR failed.");
        return Result::kVkscSurfaceFailed;
    }

    m_extent      = bestMode.parameters.visibleRegion;
    m_initialised = true;

    log::Info("DisplayOutput: surface created.");
    return Result::kOk;
}

// ---- Shutdown ---------------------------------------------------------------

void DisplayOutput::Shutdown() noexcept
{
    if (!m_initialised) { return; }

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
        log::Info("DisplayOutput: surface destroyed.");
    }

    m_instance    = VK_NULL_HANDLE;
    m_extent      = {};
    m_initialised = false;
}

} /* namespace wsi */
} /* namespace engine */
