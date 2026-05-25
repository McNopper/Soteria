/// @file display_output.hpp
/// @brief IRenderOutput backend that uses VK_KHR_display for surface creation.
///
/// The VulkanSC emulation layer supports only VK_KHR_display for WSI --
/// VK_KHR_win32_surface is NOT present in the emulation ICD extension list.
/// This class enumerates physical displays and planes and creates a
/// VkDisplayPlaneSurfaceKHR.
///
/// Init() selects:
///   - The first available display.
///   - The display mode with the largest visible area.
///   - The first display plane that supports the chosen display.
///
/// @satisfies SWS_WSI_010  DisplayOutput creates surface via VK_KHR_display.
/// @satisfies SWS_WSI_011  All fixed-size arrays are statically bounded.

#ifndef VKSC_ENGINE_WSI_DISPLAY_OUTPUT_HPP
#define VKSC_ENGINE_WSI_DISPLAY_OUTPUT_HPP

#include "i_render_output.hpp"
#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>
#include <cstdint>

namespace engine {
namespace wsi {

/// @brief VK_KHR_display surface backend.
///
/// Exactly one instance per physical display channel.
/// Non-copyable, non-movable.
class DisplayOutput final : public IRenderOutput
{
public:
    DisplayOutput() noexcept = default;
    ~DisplayOutput() noexcept override = default;

    DisplayOutput(const DisplayOutput&)            = delete;
    DisplayOutput& operator=(const DisplayOutput&) = delete;
    DisplayOutput(DisplayOutput&&)                 = delete;
    DisplayOutput& operator=(DisplayOutput&&)      = delete;

    /// @brief Enumerate displays and create a VkDisplayPlaneSurfaceKHR.
    ///
    /// @param instance   Vulkan SC instance (must be valid).
    /// @param pd         Physical device (must be valid).
    /// @returns Result::kOk on success; kVkscNoDisplay or kVkscSurfaceFailed otherwise.
    [[nodiscard]] Result Init(VkInstance       instance,
                              VkPhysicalDevice pd) noexcept override;

    /// @brief Destroy the surface.  Safe to call when not initialised.
    void Shutdown() noexcept override;

    [[nodiscard]] VkSurfaceKHR     Surface()     const noexcept override { return m_surface; }
    [[nodiscard]] VkFormat         ColorFormat() const noexcept override { return VK_FORMAT_B8G8R8A8_UNORM; }
    [[nodiscard]] RenderOutputMode Mode()        const noexcept override { return RenderOutputMode::eDirectDisplay; }
    [[nodiscard]] uint32_t         Width()       const noexcept override { return m_extent.width; }
    [[nodiscard]] uint32_t         Height()      const noexcept override { return m_extent.height; }

private:
    static constexpr uint32_t kMaxDisplays{4U};
    static constexpr uint32_t kMaxModes{16U};
    static constexpr uint32_t kMaxPlanes{8U};
    static constexpr uint32_t kInvalidIndex{0xFFFFFFFFU};

    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkInstance   m_instance{VK_NULL_HANDLE};  ///< Stored for Shutdown (does not own).
    VkExtent2D   m_extent{};
    bool         m_initialised{false};
};

} /* namespace wsi */
} /* namespace engine */

#endif /* VKSC_ENGINE_WSI_DISPLAY_OUTPUT_HPP */
