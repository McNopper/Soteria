/// @file vksc_context.cpp
/// @brief Vulkan SC context initialisation and shutdown.
///
/// Implements the Vulkan SC bring-up sequence required by the spec:
///   1. vkCreateInstance   (VKSC_API_VERSION_1_0, with caller-specified extensions)
///   2. vkEnumeratePhysicalDevices
///   3. Select device + graphics queue family
///   4. vkCreateDevice with VkDeviceObjectReservationCreateInfo (all object counts
///      provided upfront by the caller via VkscContextConfig::resources)
///   5. vkGetDeviceQueue to retrieve the graphics queue
///   6. vkDestroyDevice / vkDestroyInstance on shutdown
///
/// Every Init() error path cleans up all previously-created handles.
/// Shutdown() is safe to call on a partially-initialised context.
///
/// @satisfies   SWS_Context_001
/// @satisfies   SWS_Context_002
/// @verifiedby  UT_Context_001
/// @verifiedby  UT_Context_002

#include "vksc_context.hpp"

#include "fixed_string.hpp"
#include "log.hpp"
#include "safety_macros.hpp"

#include <cstdint>

namespace engine {

namespace {

constexpr uint32_t kAppVersion    = VK_MAKE_API_VERSION(0U, 0U, 1U, 0U);
constexpr uint32_t kEngineVersion = VK_MAKE_API_VERSION(0U, 0U, 1U, 0U);

constexpr const char* kDefaultAppName    = "soteria-app";
constexpr const char* kDefaultEngineName = "soteria";

} /* anonymous namespace */

// ---- Init -------------------------------------------------------------------

Result VkscContext::Init(const VkscContextConfig& config) noexcept
{
    if (m_initialised)
    {
        log::Error("VkscContext::Init called on already-initialised context.");
        return Result::kAlreadyInitialised;
    }

    const char* const appName =
        (config.appName != nullptr) ? config.appName : kDefaultAppName;
    const char* const engineName =
        (config.engineName != nullptr) ? config.engineName : kDefaultEngineName;

    // ---- 1. Application info ------------------------------------------------
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext              = nullptr;
    appInfo.pApplicationName   = appName;
    appInfo.applicationVersion = kAppVersion;
    appInfo.pEngineName        = engineName;
    appInfo.engineVersion      = kEngineVersion;
    appInfo.apiVersion         = VKSC_API_VERSION_1_0;

    // ---- 2. Instance create info --------------------------------------------
    VkInstanceCreateInfo instanceCI{};
    instanceCI.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pNext                   = nullptr;
    instanceCI.flags                   = 0U;
    instanceCI.pApplicationInfo        = &appInfo;
    instanceCI.enabledLayerCount       = 0U;
    instanceCI.ppEnabledLayerNames     = nullptr;
    instanceCI.enabledExtensionCount   = config.instanceExtensionCount;
    instanceCI.ppEnabledExtensionNames =
        (config.instanceExtensionCount > 0U) ? config.instanceExtensions : nullptr;

    log::Info("VkscContext: creating Vulkan SC instance...");
    const VkResult instanceResult = vkCreateInstance(&instanceCI, nullptr, &m_instance);
    if (instanceResult != VK_SUCCESS)
    {
        log::Error("VkscContext: vkCreateInstance failed.");
        return Result::kVkscInstanceFailed;
    }
    log::Info("VkscContext: instance created.");

    // ---- 3. Select physical device + queue family ---------------------------
    const Result selectResult =
        SelectPhysicalDevice(m_physicalDevice, m_graphicsQueueFamily,
                             config.preferredDriverId);
    if (!IsOk(selectResult))
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        return selectResult;
    }

    // ---- 4. Queue create info -----------------------------------------------
    const float queuePriority{1.0F};
    VkDeviceQueueCreateInfo queueCI{};
    queueCI.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCI.pNext            = nullptr;
    queueCI.flags            = 0U;
    queueCI.queueFamilyIndex = m_graphicsQueueFamily;
    queueCI.queueCount       = 1U;
    queueCI.pQueuePriorities = &queuePriority;

    // ---- 5. Vulkan SC resource reservation ----------------------------------
    //
    // VkDeviceObjectReservationCreateInfo declares all object counts upfront.
    // Callers supply these counts in VkscContextConfig::resources so the
    // context layer remains generic and does not hardcode object counts.
    const VkscResourceReservation& res = config.resources;

    VkDeviceObjectReservationCreateInfo reservationCI{};
    reservationCI.sType                          = VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO;
    reservationCI.pNext                          = nullptr;
    reservationCI.pipelineCacheCreateInfoCount   = config.pipelineCacheInfoCount;
    reservationCI.pPipelineCacheCreateInfos      = config.pipelineCacheInfos;
    reservationCI.pipelinePoolSizeCount          = config.pipelinePoolSizeCount;
    reservationCI.pPipelinePoolSizes             = config.pipelinePoolSizes;
    reservationCI.semaphoreRequestCount          = res.semaphores;
    reservationCI.commandBufferRequestCount      = res.commandBuffers;
    reservationCI.fenceRequestCount              = res.fences;
    reservationCI.deviceMemoryRequestCount       = res.deviceMemoryAllocations;
    reservationCI.bufferRequestCount             = res.buffers;
    reservationCI.imageRequestCount              = res.images;
    reservationCI.eventRequestCount              = 0U;
    reservationCI.queryPoolRequestCount          = 0U;
    reservationCI.bufferViewRequestCount         = 0U;
    reservationCI.imageViewRequestCount          = res.imageViews;
    reservationCI.layeredImageViewRequestCount   = res.layeredImageViews;
    reservationCI.pipelineCacheRequestCount      = res.pipelineCaches;
    reservationCI.pipelineLayoutRequestCount     = res.pipelineLayouts;
    reservationCI.renderPassRequestCount         = res.renderPasses;
    reservationCI.graphicsPipelineRequestCount   = res.graphicsPipelines;
    reservationCI.computePipelineRequestCount    = res.computePipelines;
    reservationCI.descriptorSetLayoutRequestCount= res.descriptorSetLayouts;
    reservationCI.samplerRequestCount            = res.samplers;
    reservationCI.descriptorPoolRequestCount     = res.descriptorPools;
    reservationCI.descriptorSetRequestCount      = res.descriptorSets;
    reservationCI.framebufferRequestCount        = res.framebuffers;
    reservationCI.commandPoolRequestCount        = res.commandPools;
    reservationCI.samplerYcbcrConversionRequestCount = 0U;
    reservationCI.surfaceRequestCount            = res.surfaces;
    reservationCI.swapchainRequestCount          = res.swapchains;
    reservationCI.displayModeRequestCount        = 0U;
    reservationCI.subpassDescriptionRequestCount = res.subpassDescriptions;
    reservationCI.attachmentDescriptionRequestCount = res.attachmentDescriptions;
    reservationCI.descriptorSetLayoutBindingRequestCount = 0U;
    reservationCI.descriptorSetLayoutBindingLimit= 0U;
    reservationCI.maxImageViewMipLevels          = res.maxImageViewMipLevels;
    reservationCI.maxImageViewArrayLayers        = res.maxImageViewArrayLayers;
    reservationCI.maxLayeredImageViewMipLevels   = 0U;
    reservationCI.maxOcclusionQueriesPerPool     = 0U;
    reservationCI.maxPipelineStatisticsQueriesPerPool = 0U;
    reservationCI.maxTimestampQueriesPerPool     = 0U;
    reservationCI.maxImmutableSamplersPerDescriptorSetLayout = 0U;

    // VkPhysicalDeviceVulkanSC10Features is REQUIRED by the VulkanSC emulation
    // ICD in the pNext chain of VkDeviceCreateInfo.  Omitting it causes
    // VK_ERROR_INITIALIZATION_FAILED regardless of other parameters.
    // shaderAtomicInstructions = VK_FALSE: we do not use atomic shader ops.
    VkPhysicalDeviceVulkanSC10Features sc10Features{};
    sc10Features.sType                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES;
    sc10Features.pNext                    = &reservationCI;
    sc10Features.shaderAtomicInstructions = VK_FALSE;

    // ---- 6. Device create info ----------------------------------------------
    VkDeviceCreateInfo deviceCI{};
    deviceCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.pNext                   = &sc10Features;   // SC10Features → reservationCI
    deviceCI.flags                   = 0U;
    deviceCI.queueCreateInfoCount    = 1U;
    deviceCI.pQueueCreateInfos       = &queueCI;
    deviceCI.enabledLayerCount       = 0U;
    deviceCI.ppEnabledLayerNames     = nullptr;
    deviceCI.enabledExtensionCount   = config.deviceExtensionCount;
    deviceCI.ppEnabledExtensionNames =
        (config.deviceExtensionCount > 0U) ? config.deviceExtensions : nullptr;
    deviceCI.pEnabledFeatures        = nullptr;

    log::Info("VkscContext: creating logical device...");
    const VkResult deviceResult =
        vkCreateDevice(m_physicalDevice, &deviceCI, nullptr, &m_device);
    if (deviceResult != VK_SUCCESS)
    {
        if constexpr (engine::log::kEnabled)
        {
            engine::log::FixedString<64U> s;
            s.Append("VkscContext: vkCreateDevice failed (VkResult=")
             .AppendIDec(static_cast<int32_t>(deviceResult))
             .Append(" / 0x")
             .AppendHex(static_cast<uint32_t>(deviceResult), 8U)
             .Append(").");
            log::Error(s.CStr());
        }
        vkDestroyInstance(m_instance, nullptr);
        m_instance             = VK_NULL_HANDLE;
        m_physicalDevice       = VK_NULL_HANDLE;
        m_graphicsQueueFamily  = kInvalidQueueFamily;
        return Result::kVkscDeviceFailed;
    }

    // ---- 7. Retrieve graphics queue ----------------------------------------
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0U, &m_graphicsQueue);

    log::Info("VkscContext: logical device created.");
    m_initialised = true;
    return Result::kOk;
}

// ---- Shutdown ---------------------------------------------------------------

void VkscContext::Shutdown() noexcept
{
    if (!m_initialised)
    {
        return;
    }

    log::Info("VkscContext: shutting down...");

    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
        m_device       = VK_NULL_HANDLE;
        m_graphicsQueue = VK_NULL_HANDLE;
        log::Info("VkscContext: logical device destroyed.");
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        log::Info("VkscContext: instance destroyed.");
    }

    m_physicalDevice      = VK_NULL_HANDLE;
    m_graphicsQueueFamily = kInvalidQueueFamily;
    m_initialised         = false;

    log::Info("VkscContext: shutdown complete.");
}

// ---- SelectPhysicalDevice ---------------------------------------------------

Result VkscContext::SelectPhysicalDevice(VkPhysicalDevice& outDevice,
                                         uint32_t&         outQueueFamily,
                                         VkDriverId        preferredDriverId) const noexcept
{
    uint32_t deviceCount{0U};
    const VkResult countResult =
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if ((countResult != VK_SUCCESS) || (deviceCount == 0U))
    {
        log::Error("VkscContext: no Vulkan SC physical devices found.");
        return Result::kVkscEnumerateFailed;
    }

    if (deviceCount > kMaxPhysicalDevices)
    {
        deviceCount = kMaxPhysicalDevices;
    }

    VkPhysicalDevice devices[kMaxPhysicalDevices]{};
    const VkResult enumResult =
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices);
    if (enumResult != VK_SUCCESS)
    {
        log::Error("VkscContext: vkEnumeratePhysicalDevices failed.");
        return Result::kVkscEnumerateFailed;
    }

    // Log all enumerated devices to aid ICD troubleshooting.
    for (uint32_t d{0U}; d < deviceCount; ++d)
    {
        VkPhysicalDeviceDriverProperties driverProps{};
        driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        driverProps.pNext = nullptr;

        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &driverProps;

        vkGetPhysicalDeviceProperties2(devices[d], &props2);

        if constexpr (engine::log::kEnabled)
        {
            engine::log::FixedString<512U> s;
            s.Append("VkscContext:  [")
             .AppendUDec(d)
             .Append("] ")
             .Append(static_cast<const char*>(props2.properties.deviceName))
             .Append("  vendorID=0x")
             .AppendHex(props2.properties.vendorID, 4U)
             .Append("  driverID=")
             .AppendIDec(static_cast<int32_t>(driverProps.driverID))
             .Append("  driver=")
             .Append(static_cast<const char*>(driverProps.driverName));
            log::Info(s.CStr());
        }
    }

    // Two-pass selection:
    //   Pass 0: if preferredDriverId is set, match driverID exactly.
    //   Pass 1: accept the first device that has a graphics queue (no filter).
    const uint32_t passes = (preferredDriverId != static_cast<VkDriverId>(0)) ? 2U : 1U;

    for (uint32_t pass{0U}; pass < passes; ++pass)
    {
        const bool useFilter  = (pass == 0U) && (passes == 2U);

        for (uint32_t d{0U}; d < deviceCount; ++d)
        {
            if (useFilter)
            {
                VkPhysicalDeviceDriverProperties driverProps{};
                driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
                driverProps.pNext = nullptr;

                VkPhysicalDeviceProperties2 props2{};
                props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                props2.pNext = &driverProps;

                vkGetPhysicalDeviceProperties2(devices[d], &props2);

                if (driverProps.driverID != preferredDriverId)
                {
                    continue;
                }
            }

            uint32_t familyCount{0U};
            vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &familyCount, nullptr);

            if (familyCount == 0U) { continue; }
            if (familyCount > kMaxQueueFamilies) { familyCount = kMaxQueueFamilies; }

            VkQueueFamilyProperties families[kMaxQueueFamilies]{};
            vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &familyCount, families);

            for (uint32_t f{0U}; f < familyCount; ++f)
            {
                if ((families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U)
                {
                    outDevice      = devices[d];
                    outQueueFamily = f;

                    VkPhysicalDeviceProperties props{};
                    vkGetPhysicalDeviceProperties(devices[d], &props);

                    if constexpr (engine::log::kEnabled)
                    {
                        engine::log::FixedString<256U> s;
                        s.Append("VkscContext: selected physical device: ")
                         .Append(static_cast<const char*>(props.deviceName))
                         .Append(" (pass ")
                         .AppendUDec(pass)
                         .Append(")");
                        log::Info(s.CStr());
                    }

                    return Result::kOk;
                }
            }
        }

        if ((pass == 0U) && (passes == 2U))
        {
            log::Warn("VkscContext: preferred driver not found; falling back to any device.");
        }
    }

    log::Error("VkscContext: no device with a graphics queue found.");
    return Result::kNotFound;
}

} /* namespace engine */
