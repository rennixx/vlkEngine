/**
 * @file swapchain.h
 * @brief Vulkan swapchain management
 */

#ifndef VE_SWAPCHAIN_H
#define VE_SWAPCHAIN_H

#include "vulkan_core.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Swapchain configuration
 */
typedef struct ve_swapchain_config {
    uint32_t width;
    uint32_t height;
    bool vsync;
    bool triple_buffering;
    VkSurfaceFormatKHR preferred_format;
    VkPresentModeKHR preferred_present_mode;
    VkImageUsageFlags additional_usage;
} ve_swapchain_config;

/**
 * @brief Swapchain state
 */
typedef struct ve_swapchain {
    VkSwapchainKHR swapchain;
    VkImage* images;
    VkImageView* image_views;
    VkFramebuffer* framebuffers;
    VkFormat format;
    VkExtent2D extent;
    uint32_t image_count;
    uint32_t current_image_index;
    bool out_of_date;
} ve_swapchain;

/**
 * @brief Get the current swapchain
 *
 * @return Swapchain pointer
 */
ve_swapchain* ve_swapchain_get_current(void);

/**
 * @brief Create swapchain with configuration
 *
 * @param config Swapchain configuration
 * @return VK_SUCCESS on success
 */
VkResult ve_swapchain_create(const ve_swapchain_config* config);

/**
 * @brief Destroy swapchain
 */
void ve_swapchain_destroy(void);

/**
 * @brief Recreate swapchain (e.g., after resize)
 *
 * @param config New configuration
 * @return VK_SUCCESS on success
 */
VkResult ve_swapchain_recreate(const ve_swapchain_config* config);

/**
 * @brief Acquire next image
 *
 * @param signal_semaphore Semaphore to signal when ready
 * @return VK_SUCCESS on success, VK_SUBOPTIMAL_KHR if swapchain needs recreation
 */
VkResult ve_swapchain_acquire_next_image(VkSemaphore signal_semaphore);

/**
 * @brief Present image
 *
 * @param wait_semaphore Semaphore to wait on before presenting
 * @return VK_SUCCESS on success
 */
VkResult ve_swapchain_present(VkSemaphore wait_semaphore);

/**
 * @brief Check if swapchain is out of date
 *
 * @return true if swapchain needs recreation
 */
bool ve_swapchain_is_out_of_date(void);

/**
 * @brief Get current image format
 *
 * @return Image format
 */
VkFormat ve_swapchain_get_format(void);

/**
 * @brief Get current swapchain extent
 *
 * @return Swapchain extent
 */
VkExtent2D ve_swapchain_get_extent(void);

/**
 * @brief Get image count
 *
 * @return Number of images in swapchain
 */
uint32_t ve_swapchain_get_image_count(void);

/**
 * @brief Get current image view
 *
 * @return Current image view
 */
VkImageView ve_swapchain_get_current_image_view(void);

/**
 * @brief Get current framebuffer
 *
 * @return Current framebuffer
 */
VkFramebuffer ve_swapchain_get_current_framebuffer(void);

/**
 * @brief Set framebuffers (for render pass compatibility)
 *
 * @param framebuffers Array of framebuffers
 * @param count Number of framebuffers
 * @param owns_framebuffers True if swapchain should destroy them
 */
void ve_swapchain_set_framebuffers(VkFramebuffer* framebuffers, uint32_t count, bool owns_framebuffers);

/**
 * @brief Query optimal surface format
 *
 * @param formats Available formats
 * @param format_count Number of formats
 * @param preferred Preferred format (optional)
 * @return Optimal format
 */
VkSurfaceFormatKHR ve_swapchain_choose_surface_format(const VkSurfaceFormatKHR* formats,
                                                      uint32_t format_count,
                                                      VkSurfaceFormatKHR* preferred);

/**
 * @brief Query optimal present mode
 *
 * @param modes Available present modes
 * @param mode_count Number of modes
 * @param vsync Enable V-Sync
 * @param triple_buffering Enable triple buffering
 * @return Optimal present mode
 */
VkPresentModeKHR ve_swapchain_choose_present_mode(const VkPresentModeKHR* modes,
                                                  uint32_t mode_count,
                                                  bool vsync,
                                                  bool triple_buffering);

/**
 * @brief Query swapchain extent
 *
 * @param capabilities Surface capabilities
 * @param width Desired width
 * @param height Desired height
 * @return Swapchain extent
 */
VkExtent2D ve_swapchain_choose_extent(const VkSurfaceCapabilitiesKHR* capabilities,
                                     uint32_t width,
                                     uint32_t height);

/**
 * @brief Create image views for swapchain images
 *
 * @return VK_SUCCESS on success
 */
VkResult ve_swapchain_create_image_views(void);

/**
 * @brief Create framebuffers for swapchain
 *
 * @param render_pass Render pass to use
 * @param depth_view Optional depth stencil attachment view
 * @return VK_SUCCESS on success
 */
VkResult ve_swapchain_create_framebuffers(VkRenderPass render_pass, VkImageView depth_view);

#ifdef __cplusplus
}
#endif

#endif /* VE_SWAPCHAIN_H */
