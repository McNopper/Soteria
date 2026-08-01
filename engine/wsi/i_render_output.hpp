/// @file i_render_output.hpp
/// @brief Abstract render-output interface supporting AR and non-AR display modes.
///
/// All window-system and AR-compositor backends implement this interface.
/// The concrete implementation is injected into the engine at construction;
/// the engine never references a concrete type directly.
///
/// Output modes:
///   eDirectDisplay    -- opaque RGB framebuffer delivered to a display
///                        controller (avionics PFD/MFD, automotive cluster).
///   eArOverlay        -- RGBA framebuffer composited over a camera feed by
///                        an on-SoC or external blending unit (automotive AR HUD,
///                        ADAS overlays, parking assist).
///   eArSeeThroughHud  -- RGBA framebuffer projected through an optical combiner
///                        onto a see-through surface (avionics HUD, waveguide
///                        windshield HUD).  Uses a wide-range float format to
///                        preserve highlight luminance across the combiner.
///
/// The RenderOutputMode is immutable after Init(); the interface enables
/// backend substitution at link time.

#ifndef VKSC_ENGINE_WSI_I_RENDER_OUTPUT_HPP
#define VKSC_ENGINE_WSI_I_RENDER_OUTPUT_HPP

#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>

#include <cstdint>

namespace engine {

/// @brief Selects how the rendered framebuffer reaches the final display.
///
/// The mode is chosen once before engine construction and must match the
/// physical hardware path.  Selecting the wrong mode is a configuration
/// error caught at Init() time.
enum class RenderOutputMode : uint32_t
{
    eDirectDisplay   = 0U, ///< Opaque RGB; direct-to-display (non-AR)
    eArOverlay       = 1U, ///< Pre-multiplied RGBA; compositor blend (AR)
    eArSeeThroughHud = 2U  ///< Pre-multiplied RGBA float; optical HUD (AR)
};

/// @brief Abstract interface for all render output / WSI backends.
///
/// Implementations must be non-copyable.  One instance per physical
/// display channel.
class IRenderOutput
{
public:
    /// @brief Initialise the backend and create a VkSurfaceKHR.
    ///
    /// @param instance  Vulkan SC instance (already created).
    /// @param pd        Selected physical device.
    /// @returns Result::kOk on success; specific error code otherwise.
    [[nodiscard]] virtual Result Init(VkInstance       instance,
                                     VkPhysicalDevice pd) noexcept = 0;

    /// @brief Destroy the VkSurfaceKHR and release backend resources.
    ///
    /// Safe to call on an uninitialised backend.
    virtual void Shutdown() noexcept = 0;

    /// @returns The VkSurfaceKHR created by Init().
    ///          Returns VK_NULL_HANDLE before a successful Init().
    [[nodiscard]] virtual VkSurfaceKHR Surface() const noexcept = 0;

    /// @returns The image format the swapchain must use for this output.
    ///
    /// Guaranteed values per mode:
    ///   eDirectDisplay   : VK_FORMAT_B8G8R8A8_UNORM
    ///   eArOverlay       : VK_FORMAT_R8G8B8A8_UNORM  (pre-multiplied alpha)
    ///   eArSeeThroughHud : VK_FORMAT_R16G16B16A16_SFLOAT (pre-multiplied alpha)
    [[nodiscard]] virtual VkFormat ColorFormat() const noexcept = 0;

    /// @returns The output mode this backend was constructed for.
    ///          Immutable after Init().
    [[nodiscard]] virtual RenderOutputMode Mode() const noexcept = 0;

    /// @returns Width of the render target in pixels.
    [[nodiscard]] virtual uint32_t Width() const noexcept = 0;

    /// @returns Height of the render target in pixels.
    [[nodiscard]] virtual uint32_t Height() const noexcept = 0;

    IRenderOutput()                                = default;
    virtual ~IRenderOutput()                       noexcept = default;

    IRenderOutput(const IRenderOutput&)            = delete;
    IRenderOutput& operator=(const IRenderOutput&) = delete;
    IRenderOutput(IRenderOutput&&)                 = delete;
    IRenderOutput& operator=(IRenderOutput&&)      = delete;
};

} /* namespace engine */

#endif /* VKSC_ENGINE_WSI_I_RENDER_OUTPUT_HPP */
