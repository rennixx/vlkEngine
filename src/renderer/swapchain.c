/**
 * @file swapchain.c
 * @brief Vulkan swapchain management implementation
 */

#define VK_NO_PROTOTYPES
#include "swapchain.h"

#include "../core/logger.h"
#include "../core/assert.h"
#include "../memory/memory.h"

#include <stdlib.h>
#include <string.h>

/* Global swapchain state */
static ve_swapchain g_swapchain = {0};
static bool g_owns_framebuffers = false;

ve_swapchain* ve_swapchain_get_current(void) {
    return &g_swapchain;
}

VkResult ve_swapchain_create(const ve_swapchain_config* config) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->physical_device && vk->surface);

    /* Destroy existing swapchain if any */
    if (g_swapchain.swapchain != VK_NULL_HANDLE) {
        ve_swapchain_destroy();
    }

    /* Query swapchain support */
    ve_swapchain_support support = ve_vulkan_query_swapchain_support(vk->physical_device);

    /* Choose format and present mode */
    VkSurfaceFormatKHR surface_format;
    if (config->preferred_format.format != VK_FORMAT_UNDEFINED) {
        surface_format = ve_swapchain_choose_surface_format(
            support.formats, support.format_count, &config->preferred_format);
    } else {
        surface_format = ve_swapchain_choose_surface_format(
            support.formats, support.format_count, NULL);
    }

    VkPresentModeKHR present_mode = ve_swapchain_choose_present_mode(
        support.present_modes, support.present_mode_count,
        config->vsync, config->triple_buffering);

    VkExtent2D extent = ve_swapchain_choose_extent(&support.capabilities,
                                                   config->width,
                                                   config->height);

    /* Image count */
    uint32_t image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    /* Swapchain create info */
    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | config->additional_usage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = support.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    /* Queue family sharing */
    uint32_t queue_family_indices[2] = {
        vk->queue_families.graphics_family,
        vk->queue_families.present_family
    };

    if (vk->queue_families.graphics_family != vk->queue_families.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }

    VkResult result = vkCreateSwapchainKHR(vk->device, &create_info, NULL, &g_swapchain.swapchain);

    /* Free support details */
    VE_FREE(support.formats);
    VE_FREE(support.present_modes);

    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create swapchain: %d", result);
        return result;
    }

    /* Store swapchain properties */
    g_swapchain.format = surface_format.format;
    g_swapchain.extent = extent;

    /* Get swapchain images */
    vkGetSwapchainImagesKHR(vk->device, g_swapchain.swapchain, &image_count, NULL);
    g_swapchain.images = (VkImage*)VE_ALLOCATE_TAG(image_count * sizeof(VkImage), VE_MEMORY_TAG_VULKAN);
    vkGetSwapchainImagesKHR(vk->device, g_swapchain.swapchain, &image_count, g_swapchain.images);
    g_swapchain.image_count = image_count;

    /* Create image views */
    result = ve_swapchain_create_image_views();
    if (result != VK_SUCCESS) {
        ve_swapchain_destroy();
        return result;
    }

    g_swapchain.out_of_date = false;
    g_swapchain.framebuffers = NULL;

    VE_LOG_INFO("Swapchain created: %ux%u, %u images", extent.width, extent.height, image_count);
    return VK_SUCCESS;
}

void ve_swapchain_destroy(void) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    if (!vk) {
        return;
    }

    ve_vulkan_wait_idle();

    /* Destroy framebuffers */
    if (g_swapchain.framebuffers) {
        if (g_owns_framebuffers) {
            for (uint32_t i = 0; i < g_swapchain.image_count; i++) {
                if (g_swapchain.framebuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(vk->device, g_swapchain.framebuffers[i], NULL);
                }
            }
        }
        VE_FREE(g_swapchain.framebuffers);
        g_swapchain.framebuffers = NULL;
    }

    /* Destroy image views */
    if (g_swapchain.image_views) {
        for (uint32_t i = 0; i < g_swapchain.image_count; i++) {
            if (g_swapchain.image_views[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(vk->device, g_swapchain.image_views[i], NULL);
            }
        }
        VE_FREE(g_swapchain.image_views);
        g_swapchain.image_views = NULL;
    }

    /* Destroy images (owned by swapchain, not allocated by us) */
    if (g_swapchain.images) {
        VE_FREE(g_swapchain.images);
        g_swapchain.images = NULL;
    }

    /* Destroy swapchain */
    if (g_swapchain.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vk->device, g_swapchain.swapchain, NULL);
        g_swapchain.swapchain = VK_NULL_HANDLE;
    }

    g_swapchain.image_count = 0;
    g_swapchain.current_image_index = 0;
}

VkResult ve_swapchain_recreate(const ve_swapchain_config* config) {
    ve_vulkan_wait_idle();

    VkResult result = ve_swapchain_create(config);
    if (result == VK_SUCCESS) {
        VE_LOG_INFO("Swapchain recreated: %ux%u", config->width, config->height);
    }

    return result;
}

VkResult ve_swapchain_acquire_next_image(VkSemaphore signal_semaphore) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    VkResult result = vkAcquireNextImageKHR(
        vk->device,
        g_swapchain.swapchain,
        UINT64_MAX,
        signal_semaphore,
        VK_NULL_HANDLE,
        &g_swapchain.current_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        g_swapchain.out_of_date = true;
        return VK_SUBOPTIMAL_KHR;
    }

    if (result == VK_SUBOPTIMAL_KHR) {
        g_swapchain.out_of_date = true;
        return VK_SUCCESS;  /* Can still present */
    }

    return result;
}

VkResult ve_swapchain_present(VkSemaphore wait_semaphore) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &g_swapchain.swapchain,
        .pImageIndices = &g_swapchain.current_image_index,
    };

    VkResult result = vkQueuePresentKHR(vk->queues.present, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        g_swapchain.out_of_date = true;
        return result;
    }

    g_swapchain.out_of_date = false;
    return result;
}

bool ve_swapchain_is_out_of_date(void) {
    return g_swapchain.out_of_date;
}

VkFormat ve_swapchain_get_format(void) {
    return g_swapchain.format;
}

VkExtent2D ve_swapchain_get_extent(void) {
    return g_swapchain.extent;
}

uint32_t ve_swapchain_get_image_count(void) {
    return g_swapchain.image_count;
}

VkImageView ve_swapchain_get_current_image_view(void) {
    if (g_swapchain.current_image_index < g_swapchain.image_count && g_swapchain.image_views) {
        return g_swapchain.image_views[g_swapchain.current_image_index];
    }
    return VK_NULL_HANDLE;
}

VkFramebuffer ve_swapchain_get_current_framebuffer(void) {
    if (g_swapchain.current_image_index < g_swapchain.image_count && g_swapchain.framebuffers) {
        return g_swapchain.framebuffers[g_swapchain.current_image_index];
    }
    return VK_NULL_HANDLE;
}

void ve_swapchain_set_framebuffers(VkFramebuffer* framebuffers, uint32_t count, bool owns_framebuffers) {
    /* Free old framebuffers if we own them */
    if (g_owns_framebuffers && g_swapchain.framebuffers) {
        ve_vulkan_context* vk = ve_vulkan_get_context();
        for (uint32_t i = 0; i < g_swapchain.image_count; i++) {
            if (g_swapchain.framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(vk->device, g_swapchain.framebuffers[i], NULL);
            }
        }
        VE_FREE(g_swapchain.framebuffers);
    }

    g_swapchain.framebuffers = framebuffers;
    g_swapchain.image_count = count;
    g_owns_framebuffers = owns_framebuffers;
}

VkSurfaceFormatKHR ve_swapchain_choose_surface_format(const VkSurfaceFormatKHR* formats,
                                                      uint32_t format_count,
                                                      VkSurfaceFormatKHR* preferred)
{
    VE_ASSERT(formats && format_count > 0);

    /* Check preferred format */
    if (preferred) {
        for (uint32_t i = 0; i < format_count; i++) {
            if (formats[i].format == preferred->format &&
                formats[i].colorSpace == preferred->colorSpace) {
                return formats[i];
            }
        }
    }

    /* Look for B8G8R8A8_SRGB with nonlinear color space */
    for (uint32_t i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[i];
        }
    }

    /* Fallback to first available */
    return formats[0];
}

VkPresentModeKHR ve_swapchain_choose_present_mode(const VkPresentModeKHR* modes,
                                                  uint32_t mode_count,
                                                  bool vsync,
                                                  bool triple_buffering)
{
    VE_ASSERT(modes && mode_count > 0);

    /* Mailbox mode is best for triple buffering without tearing */
    if (triple_buffering && !vsync) {
        for (uint32_t i = 0; i < mode_count; i++) {
            if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                return modes[i];
            }
        }
    }

    /* Immediate mode for lowest latency (may cause tearing) */
    if (!vsync) {
        for (uint32_t i = 0; i < mode_count; i++) {
            if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return modes[i];
            }
        }
    }

    /* FIFO is guaranteed and is V-Sync */
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ve_swapchain_choose_extent(const VkSurfaceCapabilitiesKHR* capabilities,
                                     uint32_t width,
                                     uint32_t height)
{
    VE_ASSERT(capabilities);

    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    }

    VkExtent2D extent = {
        .width = width,
        .height = height
    };

    extent.width = (uint32_t)fmaxf(
        (float)capabilities->minImageExtent.width,
        fminf((float)capabilities->maxImageExtent.width, (float)width));

    extent.height = (uint32_t)fmaxf(
        (float)capabilities->minImageExtent.height,
        fminf((float)capabilities->maxImageExtent.height, (float)height));

    return extent;
}

VkResult ve_swapchain_create_image_views(void) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    g_swapchain.image_views = (VkImageView*)VE_ALLOCATE_TAG(
        g_swapchain.image_count * sizeof(VkImageView), VE_MEMORY_TAG_VULKAN);

    if (!g_swapchain.image_views) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (uint32_t i = 0; i < g_swapchain.image_count; i++) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = g_swapchain.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = g_swapchain.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        VkResult result = vkCreateImageView(vk->device, &create_info, NULL,
                                           &g_swapchain.image_views[i]);
        if (result != VK_SUCCESS) {
            VE_LOG_ERROR("Failed to create image view %u: %d", i, result);

            /* Clean up already created views */
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyImageView(vk->device, g_swapchain.image_views[j], NULL);
            }
            VE_FREE(g_swapchain.image_views);
            g_swapchain.image_views = NULL;

            return result;
        }
    }

    return VK_SUCCESS;
}

VkResult ve_swapchain_create_framebuffers(VkRenderPass render_pass, VkImageView depth_view) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);
    VE_ASSERT(render_pass != VK_NULL_HANDLE);

    /* Free old framebuffers if we own them */
    if (g_owns_framebuffers && g_swapchain.framebuffers) {
        for (uint32_t i = 0; i < g_swapchain.image_count; i++) {
            if (g_swapchain.framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(vk->device, g_swapchain.framebuffers[i], NULL);
            }
        }
        VE_FREE(g_swapchain.framebuffers);
    }

    g_swapchain.framebuffers = (VkFramebuffer*)VE_ALLOCATE_TAG(
        g_swapchain.image_count * sizeof(VkFramebuffer), VE_MEMORY_TAG_VULKAN);

    if (!g_swapchain.framebuffers) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    g_owns_framebuffers = true;

    for (uint32_t i = 0; i < g_swapchain.image_count; i++) {
        VkImageView attachments[] = {
            g_swapchain.image_views[i],
            depth_view
        };

        uint32_t attachment_count = (depth_view != VK_NULL_HANDLE) ? 2 : 1;

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = attachment_count,
            .pAttachments = attachments,
            .width = g_swapchain.extent.width,
            .height = g_swapchain.extent.height,
            .layers = 1,
        };

        VkResult result = vkCreateFramebuffer(vk->device, &framebuffer_info, NULL,
                                             &g_swapchain.framebuffers[i]);
        if (result != VK_SUCCESS) {
            VE_LOG_ERROR("Failed to create framebuffer %u: %d", i, result);

            /* Clean up already created framebuffers */
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(vk->device, g_swapchain.framebuffers[j], NULL);
            }
            VE_FREE(g_swapchain.framebuffers);
            g_swapchain.framebuffers = NULL;

            return result;
        }
    }

    return VK_SUCCESS;
}
