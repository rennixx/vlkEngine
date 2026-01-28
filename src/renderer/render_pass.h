/**
 * @file render_pass.h
 * @brief Vulkan render pass management
 */

#ifndef VE_RENDER_PASS_H
#define VE_RENDER_PASS_H

#include "vulkan_core.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: Implement render pass management */
typedef struct ve_render_pass {
    VkRenderPass pass;
    VkFormat color_format;
    VkFormat depth_format;
    uint32_t color_attachment_count;
} ve_render_pass;

/**
 * @brief Create a basic render pass
 */
VkResult ve_render_pass_create_basic(VkFormat color_format, VkFormat depth_format, VkRenderPass* render_pass);

/**
 * @brief Destroy render pass
 */
void ve_render_pass_destroy(VkRenderPass render_pass);

#ifdef __cplusplus
}
#endif

#endif /* VE_RENDER_PASS_H */
