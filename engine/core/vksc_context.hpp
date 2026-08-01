/// @file vksc_context.hpp
/// @brief Vulkan SC instance + device lifecycle manager.
///
/// VkscContext owns the VkInstance, the selected VkPhysicalDevice, and the
/// VkDevice.  It is the first object created and the last destroyed.
/// All methods are noexcept — exceptions are globally disabled.

#ifndef VKSC_ENGINE_CORE_VKSC_CONTEXT_HPP
#define VKSC_ENGINE_CORE_VKSC_CONTEXT_HPP

#include "result.hpp"

#include <vulkan/vulkan_sc.h>

#include <array>
#include <cstdint>

namespace engine {

/// @brief Declares object counts for VkDeviceObjectReservationCreateInfo.
///
/// Each field maps 1:1 to the corresponding field in the VulkanSC struct.
/// Set every field to the exact number of objects your application will
/// create via the corresponding vkCreate* call.
struct VkscResourceReservation
{
    uint32_t semaphores{0U};
    uint32_t commandBuffers{0U};
    uint32_t fences{0U};
    uint32_t deviceMemoryAllocations{0U};
    uint32_t buffers{0U};
    uint32_t images{0U};                   ///< Non-swapchain images only; swapchain images are implicit.
    uint32_t imageViews{0U};
    uint32_t layeredImageViews{0U};
    uint32_t pipelineCaches{0U};
    uint32_t pipelineLayouts{0U};
    uint32_t renderPasses{0U};
    uint32_t graphicsPipelines{0U};
    uint32_t computePipelines{0U};
    uint32_t framebuffers{0U};
    uint32_t commandPools{0U};
    uint32_t swapchains{0U};
    uint32_t surfaces{0U};
    uint32_t subpassDescriptions{0U};
    uint32_t attachmentDescriptions{0U};
    uint32_t descriptorSetLayouts{0U};
    uint32_t descriptorPools{0U};
    uint32_t descriptorSets{0U};
    uint32_t samplers{0U};
    /// Limits (minimum 1 for each resource type that uses image views).
    uint32_t maxImageViewMipLevels{1U};
    uint32_t maxImageViewArrayLayers{1U};
};

/// @brief Configuration passed to VkscContext::Init.
///
/// All fields have safe defaults.  Zero-initialise then override as needed.
/// Callers must fill in all resource counts that match the objects they
/// intend to create during the device lifetime (static allocation contract).
struct VkscContextConfig
{
    /// Application name written into VkApplicationInfo.
    const char* appName{nullptr};

    /// Engine name written into VkApplicationInfo.
    const char* engineName{nullptr};

    /// Maximum number of physical devices to enumerate.  Enumeration stops at
    /// the first maxPhysicalDevices devices reported by the driver.
    /// Values above kMaxPhysicalDevices (8) are clamped; 0 is treated as 1.
    /// The default (8) enumerates every device the driver reports.
    static constexpr uint32_t kMaxPhysicalDevices{8U};
    uint32_t maxPhysicalDevices{kMaxPhysicalDevices};

    /// Instance extensions to enable (e.g. VK_KHR_SURFACE_EXTENSION_NAME,
    /// VK_KHR_DISPLAY_EXTENSION_NAME).  Unused slots must remain nullptr.
    static constexpr uint32_t kMaxExtensions{8U};
    std::array<const char*, kMaxExtensions> instanceExtensions{};
    uint32_t    instanceExtensionCount{0U};

    /// Device extensions to enable (e.g. VK_KHR_SWAPCHAIN_EXTENSION_NAME).
    std::array<const char*, kMaxExtensions> deviceExtensions{};
    uint32_t    deviceExtensionCount{0U};

    /// Static object allocation contract.
    VkscResourceReservation resources{};

    /// Pipeline cache data to declare in the reservation.
    /// Must point to valid VkPipelineCacheCreateInfo structs for the
    /// lifetime of the Init() call.  nullptr is allowed only when
    /// pipelineCacheInfoCount == 0 (no pipeline caches will be created).
    const VkPipelineCacheCreateInfo* pipelineCacheInfos{nullptr};
    uint32_t                         pipelineCacheInfoCount{0U};

    /// Pipeline pool sizes — must be declared for every poolEntrySize bucket
    /// that will be used when creating pipelines via vkCreateGraphicsPipelines.
    /// Each VkPipelinePoolSize entry declares one size class (poolEntrySize) and
    /// the number of pipelines (poolEntryCount) in that class.
    /// Must match the poolEntrySize values in VkPipelineOfflineCreateInfo.
    /// nullptr is allowed only when pipelinePoolSizeCount == 0.
    const VkPipelinePoolSize* pipelinePoolSizes{nullptr};
    uint32_t                  pipelinePoolSizeCount{0U};

    /// @brief Preferred VkDriverId for physical device selection.
    ///
    /// When non-zero the context performs a two-pass device selection:
    ///   - Pass 1: choose the first device whose driverID (from
    ///             VkPhysicalDeviceDriverProperties) matches this value.
    ///   - Pass 2: fall back to the first device with a graphics queue.
    ///
    /// Set to VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN for emulation builds
    /// so that the Vulkan-SC emulation ICD is always preferred over any native
    /// hardware SC ICD present on the same system.
    /// Leave as VkDriverId{0} in production (picks first graphics device).
    VkDriverId preferredDriverId{static_cast<VkDriverId>(0)};
};

/// @brief Manages the Vulkan SC instance and device for one safety domain.
///
/// Typical lifecycle:
/// @code
///   VkscContext ctx;
///   Result r = ctx.Init(config);
///   if (!IsOk(r)) { /* handle */ }
///   // ... use ctx.Device() ...
///   ctx.Shutdown();
/// @endcode
///
/// The class is non-copyable and non-movable; one instance per safety domain.
class VkscContext
{
public:
    VkscContext()  noexcept = default;
    ~VkscContext() noexcept = default;

    VkscContext(const VkscContext&)            = delete;
    VkscContext& operator=(const VkscContext&) = delete;
    VkscContext(VkscContext&&)                 = delete;
    VkscContext& operator=(VkscContext&&)      = delete;

    /// @brief Initialise Vulkan SC: create instance, select physical device,
    ///        reserve minimal resources, create logical device.
    ///
    /// May only be called once.  Calling Init on an already-initialised
    /// context returns kError without modifying state.
    ///
    /// @param config  Configuration parameters.
    /// @returns Result::kOk on success; a specific error code otherwise.
    [[nodiscard]] Result Init(const VkscContextConfig& config) noexcept;

    /// @brief Destroy all Vulkan SC handles in reverse creation order.
    ///
    /// Safe to call on an uninitialised context (becomes a no-op).
    void Shutdown() noexcept;

    /// @brief Return the logical device handle.
    ///
    /// Returns VK_NULL_HANDLE if Init has not succeeded.
    [[nodiscard]] VkDevice Device() const noexcept { return m_device; }

    /// @brief Return the physical device handle.
    [[nodiscard]] VkPhysicalDevice PhysicalDevice() const noexcept { return m_physicalDevice; }

    /// @brief Return the Vulkan SC instance handle.
    [[nodiscard]] VkInstance Instance() const noexcept { return m_instance; }

    /// @brief Return the graphics queue obtained after device creation.
    [[nodiscard]] VkQueue GraphicsQueue() const noexcept { return m_graphicsQueue; }

    /// @brief Return the graphics queue family index selected during Init.
    [[nodiscard]] uint32_t GraphicsQueueFamily() const noexcept { return m_graphicsQueueFamily; }

    /// @brief Return true when the context has been successfully initialised.
    [[nodiscard]] bool IsInitialised() const noexcept { return m_initialised; }

private:
    static constexpr uint32_t kMaxQueueFamilies{16U};
    static constexpr uint32_t kInvalidQueueFamily{0xFFFFFFFFU};

    VkInstance       m_instance{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice         m_device{VK_NULL_HANDLE};
    VkQueue          m_graphicsQueue{VK_NULL_HANDLE};
    uint32_t         m_graphicsQueueFamily{kInvalidQueueFamily};
    bool             m_initialised{false};

    /// @brief Select a physical device that supports a graphics queue.
    ///
    /// Performs a two-pass selection when @p preferredDriverId is non-zero:
    ///   Pass 1 — first device whose VkPhysicalDeviceDriverProperties.driverID
    ///            matches @p preferredDriverId and has a graphics queue.
    ///   Pass 2 — first device with a graphics queue (no driver filter).
    ///
    /// @param[out] outDevice         Receives the selected device handle.
    /// @param[out] outQueueFamily    Receives the graphics queue family index.
    /// @param      preferredDriverId Target driver ID or VkDriverId{0} for any.
    /// @param      maxDevices        Enumeration cap (clamped to kMaxPhysicalDevices; 0 → 1).
    /// @returns Result::kOk on success.
    [[nodiscard]] Result SelectPhysicalDevice(VkPhysicalDevice& outDevice,
                                              uint32_t&         outQueueFamily,
                                              VkDriverId        preferredDriverId,
                                              uint32_t          maxDevices) const noexcept;
};

} /* namespace engine */

#endif /* VKSC_ENGINE_CORE_VKSC_CONTEXT_HPP */
