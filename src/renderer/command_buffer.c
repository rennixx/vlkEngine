/**
 * @file command_buffer.c
 * @brief Vulkan command buffer management implementation
 */

#define VK_NO_PROTOTYPES
#include "command_buffer.h"

#include "../core/logger.h"
#include "../core/assert.h"
#include "../memory/memory.h"

#include <stdlib.h>
#include <string.h>

/* Global command pool state */
static ve_command_pool g_command_pools[3] = {0};  /* Graphics, Compute, Transfer */
static ve_command_buffer* g_frame_command_buffers[VE_MAX_FRAMES_IN_FLIGHT][3] = {0};
static bool g_initialized = false;

static const char* pool_type_names[] = {
    "graphics_command_pool",
    "compute_command_pool",
    "transfer_command_pool"
};

VkResult ve_command_buffer_init(void) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    /* Create command pools */
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    /* Graphics pool */
    pool_info.queueFamilyIndex = vk->queue_families.graphics_family;
    VkResult result = vkCreateCommandPool(vk->device, &pool_info, NULL, &g_command_pools[VE_COMMAND_BUFFER_GRAPHICS].pool);
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create graphics command pool: %d", result);
        return result;
    }
    g_command_pools[VE_COMMAND_BUFFER_GRAPHICS].type = VE_COMMAND_BUFFER_GRAPHICS;
    g_command_pools[VE_COMMAND_BUFFER_GRAPHICS].queue_family_index = vk->queue_families.graphics_family;

    /* Compute pool */
    pool_info.queueFamilyIndex = vk->queue_families.compute_family;
    result = vkCreateCommandPool(vk->device, &pool_info, NULL, &g_command_pools[VE_COMMAND_BUFFER_COMPUTE].pool);
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create compute command pool: %d", result);
        ve_command_buffer_shutdown();
        return result;
    }
    g_command_pools[VE_COMMAND_BUFFER_COMPUTE].type = VE_COMMAND_BUFFER_COMPUTE;
    g_command_pools[VE_COMMAND_BUFFER_COMPUTE].queue_family_index = vk->queue_families.compute_family;

    /* Transfer pool */
    pool_info.queueFamilyIndex = vk->queue_families.transfer_family;
    result = vkCreateCommandPool(vk->device, &pool_info, NULL, &g_command_pools[VE_COMMAND_BUFFER_TRANSFER].pool);
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to create transfer command pool: %d", result);
        ve_command_buffer_shutdown();
        return result;
    }
    g_command_pools[VE_COMMAND_BUFFER_TRANSFER].type = VE_COMMAND_BUFFER_TRANSFER;
    g_command_pools[VE_COMMAND_BUFFER_TRANSFER].queue_family_index = vk->queue_families.transfer_family;

    /* Pre-allocate per-frame command buffers */
    for (uint32_t frame = 0; frame < VE_MAX_FRAMES_IN_FLIGHT; frame++) {
        for (uint32_t type = 0; type < 3; type++) {
            g_frame_command_buffers[frame][type] = ve_command_buffer_allocate(
                (ve_command_buffer_type)type, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            if (!g_frame_command_buffers[frame][type]) {
                VE_LOG_ERROR("Failed to allocate frame command buffer");
                ve_command_buffer_shutdown();
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            }
        }
    }

    g_initialized = true;
    VE_LOG_INFO("Command buffers initialized");
    return VK_SUCCESS;
}

void ve_command_buffer_shutdown(void) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    if (!vk || !vk->device) {
        return;
    }

    /* Free per-frame command buffers */
    for (uint32_t frame = 0; frame < VE_MAX_FRAMES_IN_FLIGHT; frame++) {
        for (uint32_t type = 0; type < 3; type++) {
            if (g_frame_command_buffers[frame][type]) {
                ve_command_buffer_free(g_frame_command_buffers[frame][type]);
                g_frame_command_buffers[frame][type] = NULL;
            }
        }
    }

    /* Destroy command pools */
    for (uint32_t i = 0; i < 3; i++) {
        if (g_command_pools[i].pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vk->device, g_command_pools[i].pool, NULL);
            g_command_pools[i].pool = VK_NULL_HANDLE;
        }
    }

    g_initialized = false;
}

ve_command_pool* ve_command_buffer_get_pool(ve_command_buffer_type type) {
    if (!g_initialized || type >= 3) {
        return NULL;
    }
    return &g_command_pools[type];
}

ve_command_buffer* ve_command_buffer_allocate(ve_command_buffer_type type, VkCommandBufferLevel level) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    ve_command_pool* pool = ve_command_buffer_get_pool(type);
    if (!pool) {
        VE_LOG_ERROR("Invalid command pool type: %d", type);
        return NULL;
    }

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool->pool,
        .level = level,
        .commandBufferCount = 1,
    };

    VkCommandBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers(vk->device, &alloc_info, &buffer);
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to allocate command buffer: %d", result);
        return NULL;
    }

    ve_command_buffer* cmd = (ve_command_buffer*)VE_ALLOCATE_TAG(sizeof(ve_command_buffer), VE_MEMORY_TAG_VULKAN);
    if (!cmd) {
        vkFreeCommandBuffers(vk->device, pool->pool, 1, &buffer);
        return NULL;
    }

    cmd->buffer = buffer;
    cmd->type = type;
    cmd->is_recording = false;
    cmd->is_submitting = false;

    return cmd;
}

void ve_command_buffer_free(ve_command_buffer* cmd) {
    ve_vulkan_context* vk = ve_vulkan_get_context();
    if (!vk || !vk->device || !cmd) {
        return;
    }

    ve_command_pool* pool = ve_command_buffer_get_pool(cmd->type);
    if (pool) {
        vkFreeCommandBuffers(vk->device, pool->pool, 1, &cmd->buffer);
    }

    VE_FREE(cmd);
}

VkResult ve_command_buffer_begin(ve_command_buffer* cmd, VkCommandBufferUsageFlags flags) {
    VE_ASSERT(cmd && !cmd->is_recording);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };

    VkResult result = vkBeginCommandBuffer(cmd->buffer, &begin_info);
    if (result == VK_SUCCESS) {
        cmd->is_recording = true;
    }

    return result;
}

VkResult ve_command_buffer_end(ve_command_buffer* cmd) {
    VE_ASSERT(cmd && cmd->is_recording);

    VkResult result = vkEndCommandBuffer(cmd->buffer);
    if (result == VK_SUCCESS) {
        cmd->is_recording = false;
    }

    return result;
}

VkResult ve_command_buffer_reset(ve_command_buffer* cmd, VkCommandBufferResetFlags flags) {
    VE_ASSERT(cmd && !cmd->is_recording);

    return vkResetCommandBuffer(cmd->buffer, flags);
}

VkResult ve_command_buffer_submit(ve_command_buffer* cmd,
                                  const VkSemaphore* wait_semaphores,
                                  const VkPipelineStageFlags* wait_stages,
                                  uint32_t wait_count,
                                  const VkSemaphore* signal_semaphores,
                                  uint32_t signal_count,
                                  VkFence fence)
{
    VE_ASSERT(cmd && !cmd->is_recording);

    ve_vulkan_context* vk = ve_vulkan_get_context();
    VE_ASSERT(vk && vk->device);

    /* Get appropriate queue */
    VkQueue queue = VK_NULL_HANDLE;
    switch (cmd->type) {
        case VE_COMMAND_BUFFER_GRAPHICS:
            queue = vk->queues.graphics;
            break;
        case VE_COMMAND_BUFFER_COMPUTE:
            queue = vk->queues.compute;
            break;
        case VE_COMMAND_BUFFER_TRANSFER:
            queue = vk->queues.transfer;
            break;
    }

    if (queue == VK_NULL_HANDLE) {
        VE_LOG_ERROR("No queue available for command buffer type: %d", cmd->type);
        return VK_ERROR_DEVICE_LOST;
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = wait_count,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd->buffer,
        .signalSemaphoreCount = signal_count,
        .pSignalSemaphores = signal_semaphores,
    };

    cmd->is_submitting = true;
    VkResult result = vkQueueSubmit(queue, 1, &submit_info, fence);
    cmd->is_submitting = false;

    return result;
}

VkResult ve_command_buffer_submit_simple(ve_command_buffer* cmd, VkQueue queue, VkFence fence) {
    return ve_command_buffer_submit(cmd, NULL, NULL, 0, NULL, 0, fence);
}

void ve_command_buffer_begin_render_pass(ve_command_buffer* cmd,
                                        VkRenderPass render_pass,
                                        VkFramebuffer framebuffer,
                                        VkOffset2D offset,
                                        VkExtent2D extent,
                                        const VkClearValue* clear_values,
                                        uint32_t clear_count)
{
    VE_ASSERT(cmd && cmd->is_recording);

    VkRenderPassBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = offset,
            .extent = extent,
        },
        .clearValueCount = clear_count,
        .pClearValues = clear_values,
    };

    vkCmdBeginRenderPass(cmd->buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void ve_command_buffer_end_render_pass(ve_command_buffer* cmd) {
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdEndRenderPass(cmd->buffer);
}

void ve_command_buffer_set_viewport(ve_command_buffer* cmd, const VkViewport* viewport) {
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdSetViewport(cmd->buffer, 0, 1, viewport);
}

void ve_command_buffer_set_scissor(ve_command_buffer* cmd, const VkRect2D* scissor) {
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdSetScissor(cmd->buffer, 0, 1, scissor);
}

void ve_command_buffer_set_viewports(ve_command_buffer* cmd,
                                    uint32_t first_viewport,
                                    uint32_t viewport_count,
                                    const VkViewport* viewports)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdSetViewport(cmd->buffer, first_viewport, viewport_count, viewports);
}

void ve_command_buffer_set_scissors(ve_command_buffer* cmd,
                                   uint32_t first_scissor,
                                   uint32_t scissor_count,
                                   const VkRect2D* scissors)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdSetScissor(cmd->buffer, first_scissor, scissor_count, scissors);
}

void ve_command_buffer_bind_pipeline(ve_command_buffer* cmd, VkPipeline pipeline) {
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdBindPipeline(cmd->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void ve_command_buffer_bind_vertex_buffers(ve_command_buffer* cmd,
                                          uint32_t first_binding,
                                          uint32_t binding_count,
                                          const VkBuffer* buffers,
                                          const VkDeviceSize* offsets)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdBindVertexBuffers(cmd->buffer, first_binding, binding_count, buffers, offsets);
}

void ve_command_buffer_bind_index_buffer(ve_command_buffer* cmd,
                                        VkBuffer buffer,
                                        VkDeviceSize offset,
                                        VkIndexType index_type)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdBindIndexBuffer(cmd->buffer, buffer, offset, index_type);
}

void ve_command_buffer_draw_indexed(ve_command_buffer* cmd,
                                   uint32_t index_count,
                                   uint32_t instance_count,
                                   uint32_t first_index,
                                   int32_t vertex_offset,
                                   uint32_t first_instance)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdDrawIndexed(cmd->buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void ve_command_buffer_draw(ve_command_buffer* cmd,
                           uint32_t vertex_count,
                           uint32_t instance_count,
                           uint32_t first_vertex,
                           uint32_t first_instance)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdDraw(cmd->buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void ve_command_buffer_dispatch(ve_command_buffer* cmd,
                               uint32_t group_count_x,
                               uint32_t group_count_y,
                               uint32_t group_count_z)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdDispatch(cmd->buffer, group_count_x, group_count_y, group_count_z);
}

void ve_command_buffer_pipeline_barrier(ve_command_buffer* cmd,
                                       VkPipelineStageFlags src_stage_mask,
                                       VkPipelineStageFlags dst_stage_mask,
                                       VkDependencyFlags dependency_flags,
                                       uint32_t memory_barrier_count,
                                       const VkMemoryBarrier* memory_barriers,
                                       uint32_t buffer_memory_barrier_count,
                                       const VkBufferMemoryBarrier* buffer_memory_barriers,
                                       uint32_t image_memory_barrier_count,
                                       const VkImageMemoryBarrier* image_memory_barriers)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdPipelineBarrier(cmd->buffer, src_stage_mask, dst_stage_mask, dependency_flags,
                        memory_barrier_count, memory_barriers,
                        buffer_memory_barrier_count, buffer_memory_barriers,
                        image_memory_barrier_count, image_memory_barriers);
}

void ve_command_buffer_image_barrier(ve_command_buffer* cmd,
                                    VkImage image,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout,
                                    VkAccessFlags src_access_mask,
                                    VkAccessFlags dst_access_mask,
                                    VkPipelineStageFlags src_stage_mask,
                                    VkPipelineStageFlags dst_stage_mask,
                                    const VkImageSubresourceRange* subresource_range)
{
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src_access_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = *subresource_range,
    };

    ve_command_buffer_pipeline_barrier(cmd, src_stage_mask, dst_stage_mask, 0,
                                      0, NULL, 0, NULL, 1, &barrier);
}

void ve_command_buffer_copy_buffer(ve_command_buffer* cmd,
                                  VkBuffer src,
                                  VkBuffer dst,
                                  uint32_t region_count,
                                  const VkBufferCopy* regions)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdCopyBuffer(cmd->buffer, src, dst, region_count, regions);
}

void ve_command_buffer_copy_buffer_to_image(ve_command_buffer* cmd,
                                           VkBuffer src,
                                           VkImage dst,
                                           uint32_t region_count,
                                           const VkBufferImageCopy* regions)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdCopyBufferToImage(cmd->buffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region_count, regions);
}

void ve_command_buffer_blit_image(ve_command_buffer* cmd,
                                 VkImage src,
                                 VkImageLayout src_layout,
                                 VkImage dst,
                                 VkImageLayout dst_layout,
                                 uint32_t region_count,
                                 const VkImageBlit* regions,
                                 VkFilter filter)
{
    VE_ASSERT(cmd && cmd->is_recording);
    vkCmdBlitImage(cmd->buffer, src, src_layout, dst, dst_layout, region_count, regions, filter);
}

ve_command_buffer* ve_command_buffer_get_current(ve_command_buffer_type type) {
    if (!g_initialized || type >= 3) {
        return NULL;
    }

    uint32_t current_frame = ve_vulkan_get_current_frame();
    return g_frame_command_buffers[current_frame][type];
}

ve_command_buffer* ve_command_buffer_begin_frame(ve_command_buffer_type type) {
    ve_command_buffer* cmd = ve_command_buffer_get_current(type);
    if (!cmd) {
        return NULL;
    }

    if (cmd->is_recording) {
        VE_LOG_WARN("Command buffer already recording");
        return cmd;
    }

    VkResult result = ve_command_buffer_begin(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    if (result != VK_SUCCESS) {
        VE_LOG_ERROR("Failed to begin frame command buffer: %d", result);
        return NULL;
    }

    return cmd;
}

VkResult ve_command_buffer_end_frame(ve_command_buffer* cmd) {
    if (!cmd || !cmd->is_recording) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return ve_command_buffer_end(cmd);
}
