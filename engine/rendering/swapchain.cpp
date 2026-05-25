/// @file swapchain.cpp
/// @brief Generic VulkanSC swapchain lifecycle implementation.

#include "swapchain.hpp"
#include "../core/log.hpp"

#include <cstdint>

namespace engine {
namespace rendering {

// ---- Init -------------------------------------------------------------------

Result SwapchainSc::Init(const Config& cfg) noexcept
{
    if (m_initialised)
    {
        log::Error("SwapchainSc::Init called on already-initialised object.");
        return Result::kAlreadyInitialised;
    }
    if ((cfg.device == VK_NULL_HANDLE) || (cfg.physDevice == VK_NULL_HANDLE) ||
        (cfg.surface == VK_NULL_HANDLE))
    {
        log::Error("SwapchainSc::Init: null device, physDevice, or surface.");
        return Result::kInvalidArgument;
    }

    // ---- Query surface capabilities ----------------------------------------
    VkSurfaceCapabilitiesKHR caps{};
    VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(cfg.physDevice, cfg.surface, &caps);
    if (r != VK_SUCCESS)
    {
        log::Error("SwapchainSc: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed.");
        return Result::kVkscSwapchainFailed;
    }

    // ---- Verify format support ---------------------------------------------
    static constexpr uint32_t kMaxFormats{16U};
    uint32_t formatCount{0U};
    r = vkGetPhysicalDeviceSurfaceFormatsKHR(cfg.physDevice, cfg.surface, &formatCount, nullptr);
    if ((r != VK_SUCCESS) || (formatCount == 0U))
    {
        log::Error("SwapchainSc: no surface formats found.");
        return Result::kVkscSwapchainFailed;
    }
    if (formatCount > kMaxFormats) { formatCount = kMaxFormats; }

    VkSurfaceFormatKHR formats[kMaxFormats]{};
    r = vkGetPhysicalDeviceSurfaceFormatsKHR(cfg.physDevice, cfg.surface, &formatCount, formats);
    if (r != VK_SUCCESS)
    {
        log::Error("SwapchainSc: vkGetPhysicalDeviceSurfaceFormatsKHR failed.");
        return Result::kVkscSwapchainFailed;
    }

    bool formatFound{false};
    for (uint32_t fi{0U}; fi < formatCount; ++fi)
    {
        if (formats[fi].format == cfg.requiredFormat)
        {
            formatFound = true;
            break;
        }
    }
    if (!formatFound)
    {
        log::Error("SwapchainSc: required surface format not supported.");
        return Result::kVkscSwapchainFailed;
    }

    // ---- Select extent -----------------------------------------------------
    VkExtent2D extent{};
    if (caps.currentExtent.width != 0xFFFFFFFFU)
    {
        extent = caps.currentExtent;
    }
    else
    {
        extent.width  = (cfg.preferredWidth  > 0U) ? cfg.preferredWidth  : 1280U;
        extent.height = (cfg.preferredHeight > 0U) ? cfg.preferredHeight : 720U;
        if (extent.width  < caps.minImageExtent.width)  { extent.width  = caps.minImageExtent.width; }
        if (extent.height < caps.minImageExtent.height) { extent.height = caps.minImageExtent.height; }
        if (extent.width  > caps.maxImageExtent.width)  { extent.width  = caps.maxImageExtent.width; }
        if (extent.height > caps.maxImageExtent.height) { extent.height = caps.maxImageExtent.height; }
    }

    // ---- Choose image count ------------------------------------------------
    uint32_t imageCount = caps.minImageCount + 1U;
    if ((caps.maxImageCount > 0U) && (imageCount > caps.maxImageCount))
    {
        imageCount = caps.maxImageCount;
    }
    if (imageCount > kMaxImages) { imageCount = kMaxImages; }

    // ---- Create swapchain --------------------------------------------------
    VkSwapchainCreateInfoKHR swapCI{};
    swapCI.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapCI.pNext            = nullptr;
    swapCI.flags            = 0U;
    swapCI.surface          = cfg.surface;
    swapCI.minImageCount    = imageCount;
    swapCI.imageFormat      = cfg.requiredFormat;
    swapCI.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapCI.imageExtent      = extent;
    swapCI.imageArrayLayers = 1U;
    swapCI.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapCI.queueFamilyIndexCount = 0U;
    swapCI.pQueueFamilyIndices   = nullptr;
    swapCI.preTransform     = caps.currentTransform;
    swapCI.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapCI.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
    swapCI.clipped          = VK_TRUE;
    swapCI.oldSwapchain     = VK_NULL_HANDLE;

    r = vkCreateSwapchainKHR(cfg.device, &swapCI, nullptr, &m_swapchain);
    if (r != VK_SUCCESS)
    {
        log::Error("SwapchainSc: vkCreateSwapchainKHR failed.");
        return Result::kVkscSwapchainFailed;
    }

    // ---- Retrieve swapchain images -----------------------------------------
    r = vkGetSwapchainImagesKHR(cfg.device, m_swapchain, &m_imageCount, nullptr);
    if ((r != VK_SUCCESS) || (m_imageCount == 0U))
    {
        log::Error("SwapchainSc: vkGetSwapchainImagesKHR (count) failed.");
        // In VulkanSC, vkDestroySwapchainKHR does not exist.
        // The swapchain is freed implicitly when vkDestroyDevice is called.
        m_swapchain = VK_NULL_HANDLE;
        return Result::kVkscSwapchainFailed;
    }
    if (m_imageCount > kMaxImages) { m_imageCount = kMaxImages; }

    r = vkGetSwapchainImagesKHR(cfg.device, m_swapchain, &m_imageCount, m_images);
    if (r != VK_SUCCESS)
    {
        log::Error("SwapchainSc: vkGetSwapchainImagesKHR failed.");
        m_swapchain  = VK_NULL_HANDLE;
        m_imageCount = 0U;
        return Result::kVkscSwapchainFailed;
    }

    // ---- Create image views -------------------------------------------------
    for (uint32_t i{0U}; i < m_imageCount; ++i)
    {
        VkImageViewCreateInfo viewCI{};
        viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.pNext                           = nullptr;
        viewCI.flags                           = 0U;
        viewCI.image                           = m_images[i];
        viewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format                          = cfg.requiredFormat;
        viewCI.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCI.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCI.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCI.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCI.subresourceRange.baseMipLevel   = 0U;
        viewCI.subresourceRange.levelCount     = 1U;
        viewCI.subresourceRange.baseArrayLayer = 0U;
        viewCI.subresourceRange.layerCount     = 1U;

        r = vkCreateImageView(cfg.device, &viewCI, nullptr, &m_imageViews[i]);
        if (r != VK_SUCCESS)
        {
            log::Error("SwapchainSc: vkCreateImageView failed.");
            // Destroy already-created views; swapchain freed by vkDestroyDevice.
            for (uint32_t j{0U}; j < i; ++j)
            {
                vkDestroyImageView(cfg.device, m_imageViews[j], nullptr);
                m_imageViews[j] = VK_NULL_HANDLE;
            }
            m_swapchain  = VK_NULL_HANDLE;
            m_imageCount = 0U;
            return Result::kVkscSwapchainFailed;
        }
    }

    m_format      = cfg.requiredFormat;
    m_extent      = extent;
    m_initialised = true;

    log::Info("SwapchainSc: swapchain created.");
    return Result::kOk;
}

// ---- Shutdown ---------------------------------------------------------------

void SwapchainSc::Shutdown(VkDevice device) noexcept
{
    if (!m_initialised) { return; }

    for (uint32_t i{0U}; i < m_imageCount; ++i)
    {
        if (m_imageViews[i] != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, m_imageViews[i], nullptr);
            m_imageViews[i] = VK_NULL_HANDLE;
        }
    }

    if (m_swapchain != VK_NULL_HANDLE)
    {
        // In VulkanSC, vkDestroySwapchainKHR does not exist.
        // The swapchain object is freed implicitly when vkDestroyDevice is called.
        m_swapchain = VK_NULL_HANDLE;
    }

    m_imageCount  = 0U;
    m_format      = VK_FORMAT_UNDEFINED;
    m_extent      = {};
    m_initialised = false;

    log::Info("SwapchainSc: destroyed.");
}

// ---- Accessors --------------------------------------------------------------

VkImageView SwapchainSc::ImageView(uint32_t index) const noexcept
{
    if (index >= m_imageCount) { return VK_NULL_HANDLE; }
    return m_imageViews[index];
}

VkImage SwapchainSc::Image(uint32_t index) const noexcept
{
    if (index >= m_imageCount) { return VK_NULL_HANDLE; }
    return m_images[index];
}

} /* namespace rendering */
} /* namespace engine */
