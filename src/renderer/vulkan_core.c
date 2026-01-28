/**
 * @file vulkan_core.c
 * @brief Vulkan initialization and core management implementation
 */

#define VK_NO_PROTOTYPES
#include "vulkan_core.h"

#include "../core/logger.h"
#include "../core/assert.h"
#include "../platform/platform.h"
#include "../memory/memory.h"

#include <stdlib.h>
#include <string.h>

/* GLFW will be used for window/surface creation */
#include <GLFW/glfw3.h>

/* Required device extensions */
static const char* g_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE4_EXTENSION_NAME,
    /* Optional but recommended */
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
};

static const uint32_t g_device_extension_count = sizeof(g_device_extensions) / sizeof(g_device_extensions[0]);

/* Validation layers */
static const char* g_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};

static const uint32_t g_validation_layer_count = sizeof(g_validation_layers) / sizeof(g_validation_layers[0]);

/* Global Vulkan context */
static ve_vulkan_context g_vulkan_context = {0};

/* Debug messenger callback */
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    (void)user_data;
    (void)message_type;

    switch (message_severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            VE_LOG_TRACE("Vulkan Validation: %s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            VE_LOG_INFO("Vulkan Validation: %s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            VE_LOG_WARN("Vulkan Validation: %s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            VE_LOG_ERROR("Vulkan Validation: %s", callback_data->pMessage);
            break;
        default:
            break;
    }

    /* Print queue labels if present */
    if (callback_data->queueLabelCount > 0) {
        VE_LOG_DEBUG("  Queue Labels:");
        for (uint32_t i = 0; i < callback_data->queueLabelCount; i++) {
            VE_LOG_DEBUG("    %s", callback_data->pQueueLabels[i].pLabelName);
        }
    }

    /* Print command buffer labels if present */
    if (callback_data->cmdBufLabelCount > 0) {
        VE_LOG_DEBUG("  Command Buffer Labels:");
        for (uint32_t i = 0; i < callback_data->cmdBufLabelCount; i++) {
            VE_LOG_DEBUG("    %s", callback_data->pCmdBufLabels[i].pLabelName);
        }
    }

    /* Print objects if present */
    if (callback_data->objectCount > 0) {
        VE_LOG_DEBUG("  Objects:");
        for (uint32_t i = 0; i < callback_data->objectCount; i++) {
            VE_LOG_DEBUG("    Type: %d, Handle: %p, Name: %s",
                callback_data->pObjects[i].objectType,
                (void*)callback_data->pObjects[i].objectHandle,
                callback_data->pObjects[i].pObjectName);
        }
    }

    return VK_FALSE;
}

ve_vulkan_context* ve_vulkan_get_context(void) {
    return &g_vulkan_context;
}

bool ve_vulkan_init(const char* application_name, uint32_t application_version, bool enable_validation) {
    if (g_vulkan_context.initialized) {
        VE_LOG_WARN("Vulkan already initialized");
        return true;
    }

    memset(&g_vulkan_context, 0, sizeof(ve_vulkan_context));
    g_vulkan_context.validation_enabled = enable_validation;

    /* Load Volk for dynamic Vulkan loading */
    if (!volkInitialize()) {
        VE_LOG_ERROR("Failed to initialize Volk - Vulkan runtime not found");
        return false;
    }

    uint32_t vulkan_version = volkGetInstanceVersion();
    VE_LOG_INFO("Vulkan version: %u.%u.%u",
        VK_VERSION_MAJOR(vulkan_version),
        VK_VERSION_MINOR(vulkan_version),
        VK_VERSION_PATCH(vulkan_version));

    /* Check validation layer support */
    if (enable_validation && !ve_vulkan_check_validation_support()) {
        VE_LOG_WARN("Validation layers requested but not available");
        g_vulkan_context.validation_enabled = false;
    }

    /* Create instance */
    VkResult result = ve_vulkan_create_instance(application_name, application_version);
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create Vulkan instance: %d", result);
        return false;
    }

    /* Load Vulkan function pointers */
    volkLoadInstance(g_vulkan_context.instance);

    /* Setup debug messenger */
    if (g_vulkan_context.validation_enabled) {
        result = ve_vulkan_setup_debug_messenger();
        if (result != VK_SUCCESS) {
            VE_LOG_ERROR("Failed to setup debug messenger: %d", result);
            ve_vulkan_shutdown();
            return false;
        }
    }

    /* Create surface - will be initialized properly when window is created */
    /* This will be done by the window manager */

    /* Pick physical device */
    result = ve_vulkan_pick_physical_device();
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to pick physical device: %d", result);
        ve_vulkan_shutdown();
        return false;
    }

    VkPhysicalDeviceProperties* props = &g_vulkan_context.device_properties.properties;
    VE_LOG_INFO("Physical Device: %s", props->deviceName);
    VE_LOG_INFO("  Driver Version: %u.%u.%u",
        VK_VERSION_MAJOR(props->driverVersion),
        VK_VERSION_MINOR(props->driverVersion),
        VK_VERSION_PATCH(props->driverVersion));
    VE_LOG_INFO("  API Version: %u.%u.%u",
        VK_VERSION_MAJOR(props->apiVersion),
        VK_VERSION_MINOR(props->apiVersion),
        VK_VERSION_PATCH(props->apiVersion));
    VE_LOG_INFO("  Device Type: %d", props->deviceType);

    /* Create logical device */
    result = ve_vulkan_create_logical_device();
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create logical device: %d", result);
        ve_vulkan_shutdown();
        return false;
    }

    /* Load device-level function pointers */
    volkLoadDevice(g_vulkan_context.device);

    g_vulkan_context.initialized = true;
    g_vulkan_context.current_frame = 0;

    VE_LOG_INFO("Vulkan initialized successfully");
    return true;
}

void ve_vulkan_shutdown(void) {
    if (!g_vulkan_context.initialized) {
        return;
    }

    VE_LOG_INFO("Shutting down Vulkan...");

    /* Wait for device to finish */
    if (g_vulkan_context.device) {
        vkDeviceWaitIdle(g_vulkan_context.device);
    }

    /* Destroy surface */
    if (g_vulkan_context.surface) {
        vkDestroySurfaceKHR(g_vulkan_context.instance, g_vulkan_context.surface, NULL);
        g_vulkan_context.surface = VK_NULL_HANDLE;
    }

    /* Destroy debug messenger */
    if (g_vulkan_context.debug_messenger) {
        vkDestroyDebugUtilsMessengerEXT(g_vulkan_context.instance,
                                        g_vulkan_context.debug_messenger, NULL);
    }

    /* Destroy logical device */
    if (g_vulkan_context.device) {
        vkDestroyDevice(g_vulkan_context.device, NULL);
        g_vulkan_context.device = VK_NULL_HANDLE;
    }

    /* Destroy instance */
    if (g_vulkan_context.instance) {
        vkDestroyInstance(g_vulkan_context.instance, NULL);
        g_vulkan_context.instance = VK_NULL_HANDLE;
    }

    /* Free extension/layer strings */
    for (uint32_t i = 0; i < g_vulkan_context.enabled_extension_count; i++) {
        VE_FREE(g_vulkan_context.enabled_extensions[i]);
    }
    for (uint32_t i = 0; i < g_vulkan_context.enabled_layer_count; i++) {
        VE_FREE(g_vulkan_context.enabled_layers[i]);
    }

    memset(&g_vulkan_context, 0, sizeof(ve_vulkan_context));
}

bool ve_vulkan_is_validation_enabled(void) {
    return g_vulkan_context.validation_enabled;
}

bool ve_vulkan_get_required_extensions(const char** extensions, uint32_t* count) {
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    if (!glfw_extensions) {
        VE_LOG_ERROR("Failed to get GLFW required extensions");
        return false;
    }

    /* Add debug messenger extension if validation is enabled */
    uint32_t total_count = glfw_extension_count;
    if (g_vulkan_context.validation_enabled) {
        total_count += 1;
    }

    if (extensions && *count >= total_count) {
        memcpy(extensions, glfw_extensions, glfw_extension_count * sizeof(const char*));
        if (g_vulkan_context.validation_enabled) {
            extensions[glfw_extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        *count = total_count;
        return true;
    }

    *count = total_count;
    return false;
}

bool ve_vulkan_check_validation_support(void) {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties* available_layers = (VkLayerProperties*)VE_ALLOCATE_TAG(
        layer_count * sizeof(VkLayerProperties), VE_MEMORY_TAG_VULKAN);

    if (!available_layers) {
        return false;
    }

    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    bool all_layers_available = true;
    for (uint32_t i = 0; i < g_validation_layer_count; i++) {
        bool layer_found = false;
        for (uint32_t j = 0; j < layer_count; j++) {
            if (strcmp(g_validation_layers[i], available_layers[j].layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            all_layers_available = false;
            VE_LOG_WARN("Validation layer not found: %s", g_validation_layers[i]);
        }
    }

    VE_FREE(available_layers);
    return all_layers_available;
}

VkResult ve_vulkan_create_instance(const char* application_name, uint32_t application_version) {
    /* Get required extensions */
    uint32_t extension_count = 0;
    const char** extensions = NULL;

    if (!ve_vulkan_get_required_extensions(NULL, &extension_count)) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    extensions = (const char**)VE_ALLOCATE_TAG(extension_count * sizeof(const char*), VE_MEMORY_TAG_VULKAN);
    if (!extensions) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (!ve_vulkan_get_required_extensions(extensions, &extension_count)) {
        VE_FREE(extensions);
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    /* Check extension support */
    uint32_t available_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &available_count, NULL);
    VkExtensionProperties* available = (VkExtensionProperties*)VE_ALLOCATE_TAG(
        available_count * sizeof(VkExtensionProperties), VE_MEMORY_TAG_VULKAN);

    if (available) {
        vkEnumerateInstanceExtensionProperties(NULL, &available_count, available);

        for (uint32_t i = 0; i < extension_count; i++) {
            bool found = false;
            for (uint32_t j = 0; j < available_count; j++) {
                if (strcmp(extensions[i], available[j].extensionName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                VE_LOG_ERROR("Required instance extension not found: %s", extensions[i]);
                VE_FREE(available);
                VE_FREE(extensions);
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        }

        VE_FREE(available);
    }

    /* Log enabled extensions */
    VE_LOG_DEBUG("Enabled instance extensions:");
    for (uint32_t i = 0; i < extension_count; i++) {
        VE_LOG_DEBUG("  %s", extensions[i]);
        /* Store in context */
        if (i < VE_MAX_EXTENSIONS) {
            size_t len = strlen(extensions[i]) + 1;
            g_vulkan_context.enabled_extensions[i] = (char*)VE_ALLOCATE_TAG(len, VE_MEMORY_TAG_VULKAN);
            if (g_vulkan_context.enabled_extensions[i]) {
                memcpy(g_vulkan_context.enabled_extensions[i], extensions[i], len);
            }
        }
    }
    g_vulkan_context.enabled_extension_count = extension_count;

    /* Create application info */
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = application_name,
        .applicationVersion = application_version,
        .pEngineName = "Vulkan Engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = volkGetInstanceVersion(),
    };

    /* Create instance info */
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
    };

    /* Setup debug messenger for instance creation/destruction */
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };

    if (g_vulkan_context.validation_enabled) {
        create_info.enabledLayerCount = g_validation_layer_count;
        create_info.ppEnabledLayerNames = g_validation_layers;
        create_info.pNext = &debug_create_info;

        /* Store enabled layers */
        for (uint32_t i = 0; i < g_validation_layer_count; i++) {
            if (i < VE_MAX_LAYERS) {
                size_t len = strlen(g_validation_layers[i]) + 1;
                g_vulkan_context.enabled_layers[i] = (char*)VE_ALLOCATE_TAG(len, VE_MEMORY_TAG_VULKAN);
                if (g_vulkan_context.enabled_layers[i]) {
                    memcpy(g_vulkan_context.enabled_layers[i], g_validation_layers[i], len);
                }
            }
        }
        g_vulkan_context.enabled_layer_count = g_validation_layer_count;
    }

    VkResult result = vkCreateInstance(&create_info, NULL, &g_vulkan_context.instance);
    VE_FREE(extensions);
    return result;
}

VkResult ve_vulkan_setup_debug_messenger(void) {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };

    return vkCreateDebugUtilsMessengerEXT(g_vulkan_context.instance, &create_info,
                                          NULL, &g_vulkan_context.debug_messenger);
}

VkResult ve_vulkan_create_surface(void* window_handle) {
    if (!window_handle) {
        VE_LOG_ERROR("Invalid window handle");
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    GLFWwindow* window = (GLFWwindow*)window_handle;
    VkResult result = glfwCreateWindowSurface(g_vulkan_context.instance, window,
                                              NULL, &g_vulkan_context.surface);

    if (result == VK_SUCCESS) {
        VE_LOG_INFO("Vulkan surface created");
    }

    return result;
}

VkResult ve_vulkan_pick_physical_device(void) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_context.instance, &device_count, NULL);

    if (device_count == 0) {
        VE_LOG_ERROR("No physical devices found");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)VE_ALLOCATE_TAG(
        device_count * sizeof(VkPhysicalDevice), VE_MEMORY_TAG_VULKAN);

    if (!devices) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkEnumeratePhysicalDevices(g_vulkan_context.instance, &device_count, devices);

    /* Score devices and pick the best one */
    int best_score = -1;
    VkPhysicalDevice best_device = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < device_count; i++) {
        if (ve_vulkan_is_device_suitable(devices[i])) {
            int score = 0;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(devices[i], &props);

            /* Discrete GPUs are preferred */
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 1000;
            }

            /* Maximum texture size is a proxy for overall power */
            score += props.limits.maxImageDimension2D;

            if (score > best_score) {
                best_score = score;
                best_device = devices[i];
            }
        }
    }

    VE_FREE(devices);

    if (best_device == VK_NULL_HANDLE) {
        VE_LOG_ERROR("No suitable physical device found");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    g_vulkan_context.physical_device = best_device;

    /* Store device properties and features */
    vkGetPhysicalDeviceProperties(best_device, &g_vulkan_context.device_properties.properties);
    vkGetPhysicalDeviceMemoryProperties(best_device, &g_vulkan_context.device_properties.memory_properties);
    vkGetPhysicalDeviceFeatures(best_device, &g_vulkan_context.device_properties.features);

    /* Query extension-specific properties */
    VkPhysicalDeviceDescriptorIndexingPropertiesEXT indexing_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,
    };

    VkPhysicalDeviceTimelineSemaphorePropertiesKHR timeline_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES_KHR,
        .pNext = &indexing_props,
    };

    VkPhysicalDeviceVulkan11Properties vulkan11_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
        .pNext = &timeline_props,
    };

    VkPhysicalDeviceVulkan12Properties vulkan12_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
        .pNext = &vulkan11_props,
    };

    VkPhysicalDeviceVulkan13Properties vulkan13_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES,
        .pNext = &vulkan12_props,
    };

    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &vulkan13_props,
    };

    vkGetPhysicalDeviceProperties2(best_device, &props2);

    g_vulkan_context.device_properties.descriptor_indexing = indexing_props;
    g_vulkan_context.device_properties.timeline_semaphore = timeline_props;
    g_vulkan_context.device_properties.vulkan11 = vulkan11_props;
    g_vulkan_context.device_properties.vulkan12 = vulkan12_props;
    g_vulkan_context.device_properties.vulkan13 = vulkan13_props;

    return VK_SUCCESS;
}

bool ve_vulkan_is_device_suitable(VkPhysicalDevice device) {
    ve_queue_family_indices indices = ve_vulkan_find_queue_families(device);

    if (!indices.graphics_valid || !indices.present_valid) {
        return false;
    }

    /* Check extension support */
    bool extensions_supported = ve_vulkan_check_device_extension_support(
        device, g_device_extensions, g_device_extension_count);

    if (!extensions_supported) {
        return false;
    }

    /* Check swapchain support */
    ve_swapchain_support swapchain_support = ve_vulkan_query_swapchain_support(device);

    bool swapchain_adequate = swapchain_support.format_count != 0 &&
                              swapchain_support.present_mode_count != 0;

    VE_FREE(swapchain_support.formats);
    VE_FREE(swapchain_support.present_modes);

    return swapchain_adequate;
}

ve_queue_family_indices ve_vulkan_find_queue_families(VkPhysicalDevice device) {
    ve_queue_family_indices indices = {0};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)VE_ALLOCATE_TAG(
        queue_family_count * sizeof(VkQueueFamilyProperties), VE_MEMORY_TAG_VULKAN);

    if (!queue_families) {
        return indices;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    for (uint32_t i = 0; i < queue_family_count; i++) {
        /* Graphics queue */
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.graphics_valid = true;
        }

        /* Compute queue (preferably separate from graphics) */
        if ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.compute_family = i;
            indices.compute_valid = true;
        } else if (!indices.compute_valid && (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.compute_family = i;
            indices.compute_valid = true;
        }

        /* Transfer queue (preferably separate) */
        if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.transfer_family = i;
            indices.transfer_valid = true;
        } else if (!indices.transfer_valid && (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            indices.transfer_family = i;
            indices.transfer_valid = true;
        }

        /* Present support */
        VkBool32 present_support = false;
        if (g_vulkan_context.surface) {
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g_vulkan_context.surface, &present_support);
            if (present_support) {
                indices.present_family = i;
                indices.present_valid = true;
            }
        }

        if (indices.graphics_valid && indices.present_valid && indices.compute_valid && indices.transfer_valid) {
            break;
        }
    }

    VE_FREE(queue_families);
    return indices;
}

bool ve_vulkan_check_device_extension_support(VkPhysicalDevice device,
                                              const char** extensions,
                                              uint32_t extension_count)
{
    uint32_t available_count = 0;
    vkEnumerateDeviceExtensionProperties(device, NULL, &available_count, NULL);

    VkExtensionProperties* available = (VkExtensionProperties*)VE_ALLOCATE_TAG(
        available_count * sizeof(VkExtensionProperties), VE_MEMORY_TAG_VULKAN);

    if (!available) {
        return false;
    }

    vkEnumerateDeviceExtensionProperties(device, NULL, &available_count, available);

    bool all_found = true;
    for (uint32_t i = 0; i < extension_count; i++) {
        bool found = false;
        for (uint32_t j = 0; j < available_count; j++) {
            if (strcmp(extensions[i], available[j].extensionName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            VE_LOG_WARN("Required device extension not found: %s", extensions[i]);
            all_found = false;
        }
    }

    VE_FREE(available);
    return all_found;
}

ve_swapchain_support ve_vulkan_query_swapchain_support(VkPhysicalDevice device) {
    ve_swapchain_support details = {0};

    if (g_vulkan_context.surface) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, g_vulkan_context.surface,
                                                  &details.capabilities);

        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vulkan_context.surface,
                                            &format_count, NULL);

        if (format_count > 0) {
            details.formats = (VkSurfaceFormatKHR*)VE_ALLOCATE_TAG(
                format_count * sizeof(VkSurfaceFormatKHR), VE_MEMORY_TAG_VULKAN);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vulkan_context.surface,
                                                &format_count, details.formats);
        }
        details.format_count = format_count;

        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vulkan_context.surface,
                                                  &present_mode_count, NULL);

        if (present_mode_count > 0) {
            details.present_modes = (VkPresentModeKHR*)VE_ALLOCATE_TAG(
                present_mode_count * sizeof(VkPresentModeKHR), VE_MEMORY_TAG_VULKAN);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vulkan_context.surface,
                                                      &present_mode_count, details.present_modes);
        }
        details.present_mode_count = present_mode_count;
    }

    return details;
}

VkResult ve_vulkan_create_logical_device(void) {
    ve_queue_family_indices indices = ve_vulkan_find_queue_families(g_vulkan_context.physical_device);
    g_vulkan_context.queue_families = indices;

    /* Create unique queue families */
    VkDeviceQueueCreateInfo queue_create_infos[VE_MAX_QUEUE_FAMILIES] = {0};
    uint32_t unique_queue_families[VE_MAX_QUEUE_FAMILIES] = {0};
    uint32_t unique_count = 0;

    #define ADD_QUEUE_FAMILY(family) \
        do { \
            bool already_added = false; \
            for (uint32_t i = 0; i < unique_count; i++) { \
                if (unique_queue_families[i] == family) { \
                    already_added = true; \
                    break; \
                } \
            } \
            if (!already_added && unique_count < VE_MAX_QUEUE_FAMILIES) { \
                unique_queue_families[unique_count++] = family; \
            } \
        } while (0)

    ADD_QUEUE_FAMILY(indices.graphics_family);
    ADD_QUEUE_FAMILY(indices.compute_family);
    ADD_QUEUE_FAMILY(indices.transfer_family);
    ADD_QUEUE_FAMILY(indices.present_family);

    float queue_priority = 1.0f;
    for (uint32_t i = 0; i < unique_count; i++) {
        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_queue_families[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
        queue_create_infos[i] = queue_info;
    }

    /* Device features */
    VkPhysicalDeviceFeatures device_features = {
        .robustBufferAccess = VK_FALSE,
        .fullDrawIndexUint32 = VK_FALSE,
        .imageCubeArray = VK_TRUE,
        .independentBlend = VK_TRUE,
        .geometryShader = VK_FALSE,
        .tessellationShader = VK_FALSE,
        .sampleRateShading = VK_TRUE,
        .dualSrcBlend = VK_FALSE,
        .logicOp = VK_FALSE,
        .multiDrawIndirect = VK_TRUE,
        .drawIndirectFirstInstance = VK_TRUE,
        .depthClamp = VK_TRUE,
        .depthBiasClamp = VK_FALSE,
        .fillModeNonSolid = VK_TRUE,
        .depthBounds = VK_FALSE,
        .wideLines = VK_FALSE,
        .largePoints = VK_FALSE,
        .alphaToOne = VK_FALSE,
        .multiViewport = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
        .textureCompressionETC2 = VK_FALSE,
        .textureCompressionASTC_LDR = VK_FALSE,
        .textureCompressionBC = VK_TRUE,
        .occlusionQueryPrecise = VK_TRUE,
        .pipelineStatisticsQuery = VK_FALSE,
        .fragmentStoresAndAtomics = VK_TRUE,
        .shaderTessellationAndGeometryPointSize = VK_FALSE,
        .shaderImageGatherExtended = VK_TRUE,
        .shaderStorageImageExtendedFormats = VK_TRUE,
        .shaderStorageImageReadWithoutFormat = VK_TRUE,
        .shaderStorageImageWriteWithoutFormat = VK_TRUE,
        .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
        .shaderSampledImageArrayDynamicIndexing = VK_TRUE,
        .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
        .shaderStorageImageArrayDynamicIndexing = VK_TRUE,
        .shaderClipDistance = VK_TRUE,
        .shaderCullDistance = VK_TRUE,
        .shaderFloat64 = VK_FALSE,
        .shaderInt64 = VK_TRUE,
        .shaderInt16 = VK_FALSE,
        .shaderResourceMinLod = VK_FALSE,
        .sparseBinding = VK_FALSE,
        .sparseResidencyBuffer = VK_FALSE,
        .sparseResidencyImage2D = VK_FALSE,
        .sparseResidencyImage3D = VK_FALSE,
        .sparseResidency2Samples = VK_FALSE,
        .sparseResidency4Samples = VK_FALSE,
        .sparseResidency8Samples = VK_FALSE,
        .sparseResidency16Samples = VK_FALSE,
        .sparseResidencyAliased = VK_FALSE,
        .variableMultisampleRate = VK_FALSE,
        .inheritedQueries = VK_FALSE,
    };

    /* Create logical device */
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = unique_count,
        .pQueueCreateInfos = queue_create_infos,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = g_device_extension_count,
        .ppEnabledExtensionNames = g_device_extensions,
    };

    /* Enable validation layers for device (deprecated but for compatibility) */
    if (g_vulkan_context.validation_enabled) {
        create_info.enabledLayerCount = g_validation_layer_count;
        create_info.ppEnabledLayerNames = g_validation_layers;
    }

    VkResult result = vkCreateDevice(g_vulkan_context.physical_device, &create_info,
                                     NULL, &g_vulkan_context.device);

    if (result == VK_SUCCESS) {
        /* Get queue handles */
        vkGetDeviceQueue(g_vulkan_context.device, indices.graphics_family, 0, &g_vulkan_context.queues.graphics);
        vkGetDeviceQueue(g_vulkan_context.device, indices.present_family, 0, &g_vulkan_context.queues.present);
        vkGetDeviceQueue(g_vulkan_context.device, indices.compute_family, 0, &g_vulkan_context.queues.compute);
        vkGetDeviceQueue(g_vulkan_context.device, indices.transfer_family, 0, &g_vulkan_context.queues.transfer);
    }

    return result;
}

void ve_vulkan_wait_idle(void) {
    if (g_vulkan_context.device) {
        vkDeviceWaitIdle(g_vulkan_context.device);
    }
}

uint32_t ve_vulkan_get_current_frame(void) {
    return g_vulkan_context.current_frame;
}

void ve_vulkan_advance_frame(void) {
    g_vulkan_context.current_frame = (g_vulkan_context.current_frame + 1) % VE_MAX_FRAMES_IN_FLIGHT;
}

bool ve_vulkan_supports_bindless(void) {
    /* Check if descriptor indexing is supported */
    for (uint32_t i = 0; i < g_device_extension_count; i++) {
        if (strcmp(g_device_extensions[i], VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

bool ve_vulkan_supports_raytracing(void) {
    /* Check for ray tracing extensions */
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(g_vulkan_context.physical_device, NULL,
                                        &extension_count, NULL);

    VkExtensionProperties* extensions = (VkExtensionProperties*)VE_ALLOCATE_TAG(
        extension_count * sizeof(VkExtensionProperties), VE_MEMORY_TAG_VULKAN);

    if (!extensions) {
        return false;
    }

    vkEnumerateDeviceExtensionProperties(g_vulkan_context.physical_device, NULL,
                                        &extension_count, extensions);

    bool found = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
            found = true;
            break;
        }
    }

    VE_FREE(extensions);
    return found;
}

bool ve_vulkan_supports_mesh_shaders(void) {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(g_vulkan_context.physical_device, NULL,
                                        &extension_count, NULL);

    VkExtensionProperties* extensions = (VkExtensionProperties*)VE_ALLOCATE_TAG(
        extension_count * sizeof(VkExtensionProperties), VE_MEMORY_TAG_VULKAN);

    if (!extensions) {
        return false;
    }

    vkEnumerateDeviceExtensionProperties(g_vulkan_context.physical_device, NULL,
                                        &extension_count, extensions);

    bool found = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
            found = true;
            break;
        }
    }

    VE_FREE(extensions);
    return found;
}

bool ve_vulkan_supports_compute(void) {
    /* All Vulkan devices support compute shaders */
    return true;
}

uint32_t ve_vulkan_find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties* mem_props = &g_vulkan_context.device_properties.memory_properties;

    for (uint32_t i = 0; i < mem_props->memoryTypeCount; i++) {
        if ((type_bits & (1 << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    VE_LOG_ERROR("Failed to find suitable memory type");
    return UINT32_MAX;
}

bool ve_vulkan_is_format_supported(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(g_vulkan_context.physical_device, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR) {
        return (props.linearTilingFeatures & features) == features;
    } else {
        return (props.optimalTilingFeatures & features) == features;
    }
}

void ve_vulkan_set_object_name(uint64_t handle, VkObjectType type, const char* name) {
    if (!g_vulkan_context.validation_enabled || !name) {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT name_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = name,
    };

    vkSetDebugUtilsObjectNameEXT(g_vulkan_context.device, &name_info);
}

void ve_vulkan_begin_debug_label(VkCommandBuffer cmd, const char* name, float r, float g, float b) {
    if (!g_vulkan_context.validation_enabled || !name) {
        return;
    }

    VkDebugUtilsLabelEXT label_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = name,
        .color = {r, g, b, 1.0f},
    };

    vkCmdBeginDebugUtilsLabelEXT(cmd, &label_info);
}

void ve_vulkan_end_debug_label(VkCommandBuffer cmd) {
    if (g_vulkan_context.validation_enabled) {
        vkCmdEndDebugUtilsLabelEXT(cmd);
    }
}

void ve_vulkan_insert_debug_label(VkCommandBuffer cmd, const char* name, float r, float g, float b) {
    if (!g_vulkan_context.validation_enabled || !name) {
        return;
    }

    VkDebugUtilsLabelEXT label_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = name,
        .color = {r, g, b, 1.0f},
    };

    vkCmdInsertDebugUtilsLabelEXT(cmd, &label_info);
}
