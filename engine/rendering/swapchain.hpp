/// @file swapchain.hpp
/// @brief Generic VulkanSC swapchain lifecycle manager.
///
/// Queries surface capabilities, selects format and present mode, creates
/// VkSwapchainKHR, retrieves images, and creates one VkImageView per image.
///
/// All arrays are fixed-size (kMaxImages = 3).  No dynamic allocation.

#ifndef VKSC_ENGINE_RENDERING_SWAPCHAIN_HPP
#define VKSC_ENGINE_RENDERING_SWAPCHAIN_HPP

#include "../core/result.hpp"

#include <vulkan/vulkan_sc.h>
#include <array>
#include <cstdint>

namespace engine {
namespace rendering {

/// @brief Generic VulkanSC swapchain.  One instance per output surface.
class SwapchainSc
{
public:
    SwapchainSc()  noexcept = default;
    ~SwapchainSc() noexcept = default;

    SwapchainSc(const SwapchainSc&)            = delete;
    SwapchainSc& operator=(const SwapchainSc&) = delete;
    SwapchainSc(SwapchainSc&&)                 = delete;
    SwapchainSc& operator=(SwapchainSc&&)      = delete;

    /// @brief Parameters for swapchain creation.
    struct Config
    {
        VkPhysicalDevice physDevice{VK_NULL_HANDLE};
        VkDevice         device{VK_NULL_HANDLE};
        VkSurfaceKHR     surface{VK_NULL_HANDLE};
        uint32_t         queueFamilyIndex{0U};
        VkFormat         requiredFormat{VK_FORMAT_B8G8R8A8_UNORM};
        uint32_t         preferredWidth{0U};   ///< 0 = use surface currentExtent.
        uint32_t         preferredHeight{0U};  ///< 0 = use surface currentExtent.
    };

    /// @brief Create the swapchain, images, and image views.
    ///
    /// @param cfg  Creation parameters.
    /// @returns Result::kOk on success.
    [[nodiscard]] Result Init(const Config& cfg) noexcept;

    /// @brief Destroy image views and swapchain.  Safe on partial init.
    void Shutdown(VkDevice device) noexcept;

    [[nodiscard]] VkSwapchainKHR Handle()                const noexcept { return m_swapchain; }
    [[nodiscard]] VkFormat       Format()                const noexcept { return m_format; }
    [[nodiscard]] VkExtent2D     Extent()                const noexcept { return m_extent; }
    [[nodiscard]] uint32_t       ImageCount()            const noexcept { return m_imageCount; }

    /// @brief Return the image view at @p index.
    ///        Returns VK_NULL_HANDLE when index >= ImageCount().
    [[nodiscard]] VkImageView    ImageView(uint32_t index) const noexcept;

    /// @brief Return the raw swapchain image at @p index.
    ///        Returns VK_NULL_HANDLE when index >= ImageCount().
    [[nodiscard]] VkImage        Image(uint32_t index)     const noexcept;

    static constexpr uint32_t kMaxImages{3U};

private:
    VkSwapchainKHR                  m_swapchain{VK_NULL_HANDLE};
    std::array<VkImage, kMaxImages>     m_images{};
    std::array<VkImageView, kMaxImages> m_imageViews{};
    uint32_t       m_imageCount{0U};
    VkFormat       m_format{VK_FORMAT_UNDEFINED};
    VkExtent2D     m_extent{};
    bool           m_initialised{false};
};

} /* namespace rendering */
} /* namespace engine */

#endif /* VKSC_ENGINE_RENDERING_SWAPCHAIN_HPP */
