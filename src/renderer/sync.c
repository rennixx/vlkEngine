/**
 * @file sync.c
 * @brief Vulkan synchronization primitives implementation
 */

#define VK_NO_PROTOTYPES
#include "sync.h"

#include "../core/logger.h"
#include "../core/assert.h"
#include "../memory/memory.h"

#include <stdlib.h>
#include <string.h>

/* Global sync state */
static ve_sync_state g_sync_state = {0};

VkResult ve_sync_init(void) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    memset(&g_sync_state, 0, sizeof(ve_sync_state));
    g_sync_state.current_frame = 0;

    /* Check for timeline semaphore support */
    g_sync_state.timeline_semaphores = ve_sync_supports_timeline();

    /* Create sync primitives for each frame */
    for (uint32_t i = 0; i < VE_MAX_FRAMES_IN_FLIGHT; i++) {
        /* Render fence - signaled state */
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        VkResult result = vkCreateFence(vk->device, &fence_info, NULL,
                                       &g_sync_state.frames[i].render_fence);
        if (result != VK_SUCCESS) {
            VE_LOG_ERROR("Failed to create render fence for frame %u: %d", i, result);
            ve_sync_shutdown();
            return result;
        }

        /* Compute fence */
        result = vkCreateFence(vk->device, &fence_info, NULL,
                              &g_sync_state.frames[i].compute_fence);
        if (result != VK_SUCCESS) {
            VE_LOG_ERROR("Failed to create compute fence for frame %u: %d", i, result);
            ve_sync_shutdown();
            return result;
        }

        /* Transfer fence */
        result = vkCreateFence(vk->device, &fence_info, NULL,
                              &g_sync_state.frames[i].transfer_fence);
        if (result != VK_SUCCESS) {
            VE_LOG_ERROR("Failed to create transfer fence for frame %u: %d", i, result);
            ve_sync_shutdown();
            return result;
        }

        /* Semaphores */
        g_sync_state.frames[i].image_available = ve_sync_create_semaphore("image_available");
        g_sync_state.frames[i].render_finished = ve_sync_create_semaphore("render_finished");
        g_sync_state.frames[i].compute_finished = ve_sync_create_semaphore("compute_finished");
        g_sync_state.frames[i].transfer_finished = ve_sync_create_semaphore("transfer_finished");

        if (!g_sync_state.frames[i].image_available ||
            !g_sync_state.frames[i].render_finished ||
            !g_sync_state.frames[i].compute_finished ||
            !g_sync_state.frames[i].transfer_finished) {
            VE_LOG_ERROR("Failed to create semaphores for frame %u", i);
            ve_sync_shutdown();
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
    }

    VE_LOG_INFO("Synchronization primitives initialized");
    return VK_SUCCESS;
}

void ve_sync_shutdown(void) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    if (!vk || !vk->device) {
        return;
    }

    /* Wait for all frames to finish */
    if (vk->device) {
        vkDeviceWaitIdle(vk->device);
    }

    for (uint32_t i = 0; i < VE_MAX_FRAMES_IN_FLIGHT; i++) {
        /* Destroy fences */
        if (g_sync_state.frames[i].render_fence != VK_NULL_HANDLE) {
            vkDestroyFence(vk->device, g_sync_state.frames[i].render_fence, NULL);
            g_sync_state.frames[i].render_fence = VK_NULL_HANDLE;
        }
        if (g_sync_state.frames[i].compute_fence != VK_NULL_HANDLE) {
            vkDestroyFence(vk->device, g_sync_state.frames[i].compute_fence, NULL);
            g_sync_state.frames[i].compute_fence = VK_NULL_HANDLE;
        }
        if (g_sync_state.frames[i].transfer_fence != VK_NULL_HANDLE) {
            vkDestroyFence(vk->device, g_sync_state.frames[i].transfer_fence, NULL);
            g_sync_state.frames[i].transfer_fence = VK_NULL_HANDLE;
        }

        /* Destroy semaphores */
        ve_sync_destroy_semaphore(g_sync_state.frames[i].image_available);
        ve_sync_destroy_semaphore(g_sync_state.frames[i].render_finished);
        ve_sync_destroy_semaphore(g_sync_state.frames[i].compute_finished);
        ve_sync_destroy_semaphore(g_sync_state.frames[i].transfer_finished);
    }

    memset(&g_sync_state, 0, sizeof(ve_sync_state));
}

ve_frame_sync* ve_sync_get_current_frame(void) {
    return &g_sync_state.frames[g_sync_state.current_frame];
}

ve_frame_sync* ve_sync_get_frame(uint32_t frame_index) {
    if (frame_index >= VE_MAX_FRAMES_IN_FLIGHT) {
        return NULL;
    }
    return &g_sync_state.frames[frame_index];
}

VkResult ve_sync_wait_for_fences(VkFence fence, uint64_t timeout) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    return vkWaitForFences(vk->device, 1, &fence, VK_TRUE, timeout);
}

VkResult ve_sync_reset_fences(VkFence fence) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    return vkResetFences(vk->device, 1, &fence);
}

VkResult ve_sync_wait_for_frame(uint64_t timeout) {
    ve_frame_sync* frame = ve_sync_get_current_frame();
    return ve_sync_wait_for_fences(frame->render_fence, timeout);
}

VkResult ve_sync_reset_frame(void) {
    ve_frame_sync* frame = ve_sync_get_current_frame();
    return ve_sync_reset_fences(frame->render_fence);
}

void ve_sync_advance_frame(void) {
    g_sync_state.current_frame = (g_sync_state.current_frame + 1) % VE_MAX_FRAMES_IN_FLIGHT;
}

uint32_t ve_sync_get_current_frame_index(void) {
    return g_sync_state.current_frame;
}

bool ve_sync_supports_timeline(void) {
    /* Check if timeline semaphores are available */
    ve_vulkan_context* vk = ve_vulkan_get_context();

    if (!vk || !vk->physical_device) {
        return false;
    }

    /* Check for VK_KHR_timeline_semaphore extension */
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(vk->physical_device, NULL,
                                        &extension_count, NULL);

    VkExtensionProperties* extensions = (VkExtensionProperties*)VE_ALLOCATE_TAG(
        extension_count * sizeof(VkExtensionProperties), VE_MEMORY_TAG_VULKAN);

    if (!extensions) {
        return false;
    }

    vkEnumerateDeviceExtensionProperties(vk->physical_device, NULL,
                                        &extension_count, extensions);

    bool found = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(extensions[i].extensionName, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0) {
            found = true;
            break;
        }
    }

    VE_FREE(extensions);
    return found;
}

VkFence ve_sync_create_fence(VkFenceCreateFlags flags, const char* name) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    VkFenceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags,
    };

    VkFence fence = VK_NULL_HANDLE;
    VkResult result = vkCreateFence(vk->device, &create_info, NULL, &fence);

    if (result == VK_SUCCESS && name && ve_vulkan_is_validation_enabled()) {
        ve_vulkan_set_object_name((uint64_t)fence, VK_OBJECT_TYPE_FENCE, name);
    }

    return fence;
}

VkSemaphore ve_sync_create_semaphore(const char* name) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    VkSemaphoreCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore semaphore = VK_NULL_HANDLE;
    VkResult result = vkCreateSemaphore(vk->device, &create_info, NULL, &semaphore);

    if (result == VK_SUCCESS && name && ve_vulkan_is_validation_enabled()) {
        ve_vulkan_set_object_name((uint64_t)semaphore, VK_OBJECT_TYPE_SEMAPHORE, name);
    }

    return semaphore;
}

void ve_sync_destroy_fence(VkFence fence) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    if (!vk || !vk->device || fence == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyFence(vk->device, fence, NULL);
}

void ve_sync_destroy_semaphore(VkSemaphore semaphore) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    if (!vk || !vk->device || semaphore == VK_NULL_HANDLE) {
        return;
    }
    vkDestroySemaphore(vk->device, semaphore, NULL);
}

VkSemaphore ve_sync_create_timeline_semaphore(uint64_t initial_value, const char* name) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    if (!g_sync_state.timeline_semaphores) {
        VE_LOG_WARN("Timeline semaphores not supported, falling back to binary semaphore");
        return ve_sync_create_semaphore(name);
    }

    /* Requires Vulkan 1.2 or VK_KHR_timeline_semaphore */
#ifdef VK_API_VERSION_1_2
    VkSemaphoreTypeCreateInfoKHR type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR,
        .initialValue = initial_value,
    };

    VkSemaphoreCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_info,
    };

    VkSemaphore semaphore = VK_NULL_HANDLE;
    VkResult result = vkCreateSemaphore(vk->device, &create_info, NULL, &semaphore);

    if (result == VK_SUCCESS && name && ve_vulkan_is_validation_enabled()) {
        ve_vulkan_set_object_name((uint64_t)semaphore, VK_OBJECT_TYPE_SEMAPHORE, name);
    }

    return semaphore;
#else
    /* Timeline semaphores require VK_KHR_timeline_semaphore extension */
    VE_LOG_WARN("Timeline semaphores require Vulkan 1.2+ extension support");
    return VK_NULL_HANDLE;
#endif
}

VkResult ve_sync_signal_timeline(VkSemaphore semaphore, uint64_t value) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    if (!g_sync_state.timeline_semaphores) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

#ifdef VK_API_VERSION_1_2
    VkSemaphoreSignalInfo signal_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = semaphore,
        .value = value,
    };

    return vkSignalSemaphore(vk->device, &signal_info);
#else
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

VkResult ve_sync_wait_timeline(VkSemaphore semaphore, uint64_t value, uint64_t timeout) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    if (!g_sync_state.timeline_semaphores) {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

#ifdef VK_API_VERSION_1_2
    VkSemaphoreWaitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &semaphore,
        .pValues = &value,
    };

    return vkWaitSemaphores(vk->device, &wait_info, timeout);
#else
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}
