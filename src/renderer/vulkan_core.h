/**
 * @file vulkan_core.h
 * @brief Vulkan initialization and core management
 */

#ifndef VE_VULKAN_CORE_H
#define VE_VULKAN_CORE_H

#include <vulkan/vulkan_core.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Volk will be used for dynamic loading - define this before including volk.h */
#define VK_NO_PROTOTYPES
#include <volk.h>

/* Vulkan configuration */
#define VE_MAX_EXTENSIONS 64
#define VE_MAX_LAYERS 32
#define VE_MAX_QUEUE_FAMILIES 8
#define VE_MAX_FRAMES_IN_FLIGHT 3
#define VE_MAX_COMPUTE_QUEUES 1
#define VE_MAX_TRANSFER_QUEUES 1

/* Forward declarations */
typedef struct ve_vulkan_context ve_vulkan_context;

/**
 * @brief Vulkan queue family indices
 */
typedef struct ve_queue_family_indices {
    uint32_t graphics_family;
    uint32_t compute_family;
    uint32_t transfer_family;
    uint32_t present_family;

    bool graphics_valid;
    bool compute_valid;
    bool transfer_valid;
    bool present_valid;
} ve_queue_family_indices;

/**
 * @brief Vulkan queue handles
 */
typedef struct ve_queues {
    VkQueue graphics;
    VkQueue compute;
    VkQueue transfer;
    VkQueue present;
} ve_queues;

/**
 * @brief Vulkan device features
 */
typedef struct vulkan_device_features {
    /* Basic features */
    bool robustBufferAccess;
    bool fullDrawIndexUint32;
    bool imageCubeArray;
    bool independentBlend;
    bool geometryShader;
    bool tessellationShader;
    bool sampleRateShading;
    bool dualSrcBlend;
    bool logicOp;
    bool multiDrawIndirect;
    bool drawIndirectFirstInstance;
    bool depthClamp;
    bool depthBiasClamp;
    bool fillModeNonSolid;
    bool depthBounds;
    bool wideLines;
    bool largePoints;
    bool alphaToOne;
    bool multiViewport;
    bool samplerAnisotropy;
    bool textureCompressionETC2;
    bool textureCompressionASTC_LDR;
    bool textureCompressionBC;
    bool occlusionQueryPrecise;
    bool pipelineStatisticsQuery;
    bool fragmentStoresAndAtomics;
    bool shaderTessellationAndGeometryPointSize;
    bool shaderImageGatherExtended;
    bool shaderStorageImageExtendedFormats;
    bool shaderStorageImageReadWithoutFormat;
    bool shaderStorageImageWriteWithoutFormat;
    bool shaderUniformBufferArrayDynamicIndexing;
    bool shaderSampledImageArrayDynamicIndexing;
    bool shaderStorageBufferArrayDynamicIndexing;
    bool shaderStorageImageArrayDynamicIndexing;
    bool shaderClipDistance;
    bool shaderCullDistance;
    bool shaderFloat64;
    bool shaderInt64;
    bool shaderInt16;
    bool shaderResourceMinLod;
    bool sparseBinding;
    bool sparseResidencyBuffer;
    bool sparseResidencyImage2D;
    bool sparseResidencyImage3D;
    bool sparseResidency2Samples;
    bool sparseResidency4Samples;
    bool sparseResidency8Samples;
    bool sparseResidency16Samples;
    bool sparseResidencyAliased;
    bool variableMultisampleRate;
    bool inheritedQueries;

    /* Extension-specific features */
    bool descriptorIndexing;
    bool shaderDrawParameters;
    bool timelineSemaphore;
    bool vulkanMemoryModel;
    bool shaderSubgroupExtendedTypes;
    bool separateDepthStencilLayouts;
    bool hostQueryReset;
    bool indirectDrawing;
    bool shaderInt8;
    bool shaderAtomicInt64;
    bool shaderFloat16;
    bool shaderFloatAtomic;
} vulkan_device_features;

/**
 * @brief Vulkan device properties
 */
typedef struct vulkan_device_properties {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPhysicalDeviceFeatures features;

    /* Extension properties */
    VkPhysicalDeviceDescriptorIndexingPropertiesEXT descriptor_indexing;
    VkPhysicalDeviceTimelineSemaphorePropertiesKHR timeline_semaphore;
    VkPhysicalDeviceVulkan11Properties vulkan11;
    VkPhysicalDeviceVulkan12Properties vulkan12;
    VkPhysicalDeviceVulkan13Properties vulkan13;
} vulkan_device_properties;

/**
 * @brief Swapchain support details
 */
typedef struct ve_swapchain_support {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;
    uint32_t format_count;
    VkPresentModeKHR* present_modes;
    uint32_t present_mode_count;
} ve_swapchain_support;

/**
 * @brief Vulkan context
 */
struct ve_vulkan_context {
    /* Instance */
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    /* Surface */
    VkSurfaceKHR surface;

    /* Physical device */
    VkPhysicalDevice physical_device;
    vulkan_device_properties device_properties;
    vulkan_device_features device_features;

    /* Logical device */
    VkDevice device;

    /* Queues */
    ve_queue_family_indices queue_families;
    ve_queues queues;

    /* Extensions and layers */
    char* enabled_extensions[VE_MAX_EXTENSIONS];
    uint32_t enabled_extension_count;
    char* enabled_layers[VE_MAX_LAYERS];
    uint32_t enabled_layer_count;

    /* State */
    bool validation_enabled;
    bool initialized;
    uint32_t current_frame;
};

/**
 * @brief Get the global Vulkan context
 *
 * @return Vulkan context pointer
 */
ve_vulkan_context* ve_vulkan_get_context(void);

/**
 * @brief Initialize Vulkan
 *
 * @param application_name Application name
 * @param application_version Application version (VK_MAKE_VERSION)
 * @param enable_validation Enable validation layers
 * @return true on success
 */
bool ve_vulkan_init(const char* application_name, uint32_t application_version, bool enable_validation);

/**
 * @brief Shutdown Vulkan
 */
void ve_vulkan_shutdown(void);

/**
 * @brief Check if validation is enabled
 *
 * @return true if validation layers are active
 */
bool ve_vulkan_is_validation_enabled(void);

/**
 * @brief Get required instance extensions
 *
 * @param extensions Output array
 * @param count Input/Output count
 * @return true on success
 */
bool ve_vulkan_get_required_extensions(const char** extensions, uint32_t* count);

/**
 * @brief Check if validation layers are available
 *
 * @return true if available
 */
bool ve_vulkan_check_validation_support(void);

/**
 * @brief Pick a suitable physical device
 *
 * @return VK_SUCCESS on success
 */
VkResult ve_vulkan_pick_physical_device(void);

/**
 * @brief Check if a physical device is suitable
 *
 * @param device Physical device to check
 * @return true if suitable
 */
bool ve_vulkan_is_device_suitable(VkPhysicalDevice device);

/**
 * @brief Get queue family indices for a device
 *
 * @param device Physical device
 * @return Queue family indices
 */
ve_queue_family_indices ve_vulkan_find_queue_families(VkPhysicalDevice device);

/**
 * @brief Check if device supports required extensions
 *
 * @param device Physical device
 * @param extensions Required extensions
 * @param extension_count Number of extensions
 * @return true if all supported
 */
bool ve_vulkan_check_device_extension_support(VkPhysicalDevice device,
                                              const char** extensions,
                                              uint32_t extension_count);

/**
 * @brief Query swapchain support
 *
 * @param device Physical device
 * @return Swapchain support details
 */
ve_swapchain_support ve_vulkan_query_swapchain_support(VkPhysicalDevice device);

/**
 * @brief Create logical device
 *
 * @return VK_SUCCESS on success
 */
VkResult ve_vulkan_create_logical_device(void);

/**
 * @brief Create Vulkan instance
 *
 * @param application_name Application name
 * @param application_version Application version
 * @return VK_SUCCESS on success
 */
VkResult ve_vulkan_create_instance(const char* application_name, uint32_t application_version);

/**
 * @brief Setup debug messenger
 *
 * @return VK_SUCCESS on success
 */
VkResult ve_vulkan_setup_debug_messenger(void);

/**
 * @brief Create surface
 *
 * @param window_handle Platform-specific window handle
 * @return VK_SUCCESS on success
 */
VkResult ve_vulkan_create_surface(void* window_handle);

/**
 * @brief Wait for device to be idle
 */
void ve_vulkan_wait_idle(void);

/**
 * @brief Get current frame index
 *
 * @return Frame index (0 to VE_MAX_FRAMES_IN_FLIGHT - 1)
 */
uint32_t ve_vulkan_get_current_frame(void);

/**
 * @brief Advance frame index
 */
void ve_vulkan_advance_frame(void);

/* Device feature queries */

/**
 * @brief Check if device supports bindless descriptors
 *
 * @return true if supported
 */
bool ve_vulkan_supports_bindless(void);

/**
 * @brief Check if device supports ray tracing
 *
 * @return true if supported
 */
bool ve_vulkan_supports_raytracing(void);

/**
 * @brief Check if device supports mesh shaders
 *
 * @return true if supported
 */
bool ve_vulkan_supports_mesh_shaders(void);

/**
 * @brief Check if device supports compute shaders
 *
 * @return true if supported
 */
bool ve_vulkan_supports_compute(void);

/**
 * @brief Get device memory type index
 *
 * @param type_bits Memory type bits
 * @param properties Required property flags
 * @return Memory type index
 */
uint32_t ve_vulkan_find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags properties);

/**
 * @brief Get format support
 *
 * @param format Format to check
 * @param tiling Image tiling
 * @param features Required feature flags
 * @return true if supported
 */
bool ve_vulkan_is_format_supported(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features);

/* Vulkan helper macros */

/**
 * @brief Vulkan result check with logging
 */
#define VE_VK_CHECK(result) \
    do { \
        VkResult _res = (result); \
        if (_res != VK_SUCCESS) { \
            VE_LOG_ERROR("Vulkan error: %s:%d - %d", __FILE__, __LINE__, _res); \
        } \
    } while (0)

/**
 * @brief Vulkan result check with message
 */
#define VE_VK_CHECK_MSG(result, msg) \
    do { \
        VkResult _res = (result); \
        if (_res != VK_SUCCESS) { \
            VE_LOG_ERROR("Vulkan error: %s - %d", msg, _res); \
        } \
    } while (0)

/**
 * @brief Set Vulkan object name for debugging
 */
#define VE_VK_SET_OBJECT_NAME(handle, type, name) \
    ve_vulkan_set_object_name((uint64_t)(handle), type, name)

/**
 * @brief Set Vulkan object name (implementation)
 */
void ve_vulkan_set_object_name(uint64_t handle, VkObjectType type, const char* name);

/**
 * @brief Begin a debug label region
 */
void ve_vulkan_begin_debug_label(VkCommandBuffer cmd, const char* name, float r, float g, float b);

/**
 * @brief End a debug label region
 */
void ve_vulkan_end_debug_label(VkCommandBuffer cmd);

/**
 * @brief Insert a debug label
 */
void ve_vulkan_insert_debug_label(VkCommandBuffer cmd, const char* name, float r, float g, float b);

#ifdef __cplusplus
}
#endif

#endif /* VE_VULKAN_CORE_H */
