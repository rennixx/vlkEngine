/**
 * @file sync.h
 * @brief Vulkan synchronization primitives (fences, semaphores)
 */

#ifndef VE_SYNC_H
#define VE_SYNC_H

#include "vulkan_core.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Synchronization primitives for a frame
 */
typedef struct ve_frame_sync {
    VkFence render_fence;
    VkFence compute_fence;
    VkFence transfer_fence;

    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkSemaphore compute_finished;
    VkSemaphore transfer_finished;
} ve_frame_sync;

/**
 * @brief Global sync state
 */
typedef struct ve_sync_state {
    ve_frame_sync frames[VE_MAX_FRAMES_IN_FLIGHT];
    uint32_t current_frame;
    bool timeline_semaphores;
} ve_sync_state;

/**
 * @brief Initialize synchronization primitives
 *
 * @return VK_SUCCESS on success
 */
VkResult ve_sync_init(void);

/**
 * @brief Shutdown synchronization primitives
 */
void ve_sync_shutdown(void);

/**
 * @brief Get current frame sync
 *
 * @return Frame sync primitives
 */
ve_frame_sync* ve_sync_get_current_frame(void);

/**
 * @brief Get frame sync by index
 *
 * @param frame_index Frame index
 * @return Frame sync primitives
 */
ve_frame_sync* ve_sync_get_frame(uint32_t frame_index);

/**
 * @brief Wait for fence
 *
 * @param fence Fence to wait on
 * @param timeout Timeout in nanoseconds (UINT64_MAX = wait forever)
 * @return VK_SUCCESS on success
 */
VkResult ve_sync_wait_for_fences(VkFence fence, uint64_t timeout);

/**
 * @brief Reset fence
 *
 * @param fence Fence to reset
 * @return VK_SUCCESS on success
 */
VkResult ve_sync_reset_fences(VkFence fence);

/**
 * @brief Wait for current frame fence
 *
 * @param timeout Timeout in nanoseconds
 * @return VK_SUCCESS on success
 */
VkResult ve_sync_wait_for_frame(uint64_t timeout);

/**
 * @brief Reset current frame fences
 *
 * @return VK_SUCCESS on success
 */
VkResult ve_sync_reset_frame(void);

/**
 * @brief Advance to next frame
 */
void ve_sync_advance_frame(void);

/**
 * @brief Get current frame index
 *
 * @return Frame index
 */
uint32_t ve_sync_get_current_frame_index(void);

/**
 * @brief Check if timeline semaphores are supported
 *
 * @return true if supported
 */
bool ve_sync_supports_timeline(void);

/**
 * @brief Create a fence
 *
 * @param flags Fence creation flags
 * @param name Debug name
 * @return New fence or VK_NULL_HANDLE on failure
 */
VkFence ve_sync_create_fence(VkFenceCreateFlags flags, const char* name);

/**
 * @brief Create a semaphore
 *
 * @param name Debug name
 * @return New semaphore or VK_NULL_HANDLE on failure
 */
VkSemaphore ve_sync_create_semaphore(const char* name);

/**
 * @brief Destroy a fence
 *
 * @param fence Fence to destroy
 */
void ve_sync_destroy_fence(VkFence fence);

/**
 * @brief Destroy a semaphore
 *
 * @param semaphore Semaphore to destroy
 */
void ve_sync_destroy_semaphore(VkSemaphore semaphore);

/**
 * @brief Create timeline semaphore
 *
 * @param initial_value Initial timeline value
 * @param name Debug name
 * @return New semaphore or VK_NULL_HANDLE on failure
 */
VkSemaphore ve_sync_create_timeline_semaphore(uint64_t initial_value, const char* name);

/**
 * @brief Signal timeline semaphore
 *
 * @param semaphore Timeline semaphore
 * @param value Value to signal
 * @return VK_SUCCESS on success
 */
VkResult ve_sync_signal_timeline(VkSemaphore semaphore, uint64_t value);

/**
 * @brief Wait on timeline semaphore
 *
 * @param semaphore Timeline semaphore
 * @param value Value to wait for
 * @param timeout Timeout in nanoseconds
 * @return VK_SUCCESS on success
 */
VkResult ve_sync_wait_timeline(VkSemaphore semaphore, uint64_t value, uint64_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* VE_SYNC_H */
