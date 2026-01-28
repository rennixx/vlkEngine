/**
 * @file command_buffer.h
 * @brief Vulkan command buffer management
 */

#ifndef VE_COMMAND_BUFFER_H
#define VE_COMMAND_BUFFER_H

#include "vulkan_core.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Command buffer types
 */
typedef enum ve_command_buffer_type {
    VE_COMMAND_BUFFER_GRAPHICS,
    VE_COMMAND_BUFFER_COMPUTE,
    VE_COMMAND_BUFFER_TRANSFER,
} ve_command_buffer_type;

/**
 * @brief Command pool state
 */
typedef struct ve_command_pool {
    VkCommandPool pool;
    ve_command_buffer_type type;
    uint32_t queue_family_index;
} ve_command_pool;

/**
 * @brief Command buffer state
 */
typedef struct ve_command_buffer {
    VkCommandBuffer buffer;
    ve_command_buffer_type type;
    bool is_recording;
    bool is_submitting;
} ve_command_buffer;

/**
 * @brief Initialize command pools
 *
 * @return VK_SUCCESS on success
 */
VkResult ve_command_buffer_init(void);

/**
 * @brief Shutdown command pools
 */
void ve_command_buffer_shutdown(void);

/**
 * @brief Get command pool by type
 *
 * @param type Pool type
 * @return Command pool or NULL
 */
ve_command_pool* ve_command_buffer_get_pool(ve_command_buffer_type type);

/**
 * @brief Allocate a command buffer
 *
 * @param type Command buffer type
 * @param level Command buffer level
 * @return Command buffer or NULL
 */
ve_command_buffer* ve_command_buffer_allocate(ve_command_buffer_type type, VkCommandBufferLevel level);

/**
 * @brief Free a command buffer
 *
 * @param cmd Command buffer to free
 */
void ve_command_buffer_free(ve_command_buffer* cmd);

/**
 * @brief Begin recording a command buffer
 *
 * @param cmd Command buffer
 * @param flags Usage flags
 * @return VK_SUCCESS on success
 */
VkResult ve_command_buffer_begin(ve_command_buffer* cmd, VkCommandBufferUsageFlags flags);

/**
 * @brief End recording a command buffer
 *
 * @param cmd Command buffer
 * @return VK_SUCCESS on success
 */
VkResult ve_command_buffer_end(ve_command_buffer* cmd);

/**
 * @brief Reset a command buffer
 *
 * @param cmd Command buffer
 * @param flags Reset flags
 * @return VK_SUCCESS on success
 */
VkResult ve_command_buffer_reset(ve_command_buffer* cmd, VkCommandBufferResetFlags flags);

/**
 * @brief Submit a command buffer
 *
 * @param cmd Command buffer to submit
 * @param wait_semaphores Semaphores to wait on
 * @param wait_stages Pipeline stages for waits
 * @param wait_count Number of wait semaphores
 * @param signal_semaphores Semaphores to signal
 * @param signal_count Number of signal semaphores
 * @param fence Optional fence to signal
 * @return VK_SUCCESS on success
 */
VkResult ve_command_buffer_submit(ve_command_buffer* cmd,
                                  const VkSemaphore* wait_semaphores,
                                  const VkPipelineStageFlags* wait_stages,
                                  uint32_t wait_count,
                                  const VkSemaphore* signal_semaphores,
                                  uint32_t signal_count,
                                  VkFence fence);

/**
 * @brief Submit command buffer to queue
 *
 * @param cmd Command buffer
 * @param queue Queue to submit to
 * @param fence Optional fence
 * @return VK_SUCCESS on success
 */
VkResult ve_command_buffer_submit_simple(ve_command_buffer* cmd, VkQueue queue, VkFence fence);

/**
 * @brief Begin a render pass
 *
 * @param cmd Command buffer
 * @param render_pass Render pass
 * @param framebuffer Framebuffer
 * @param offset Render area offset
 * @param extent Render area extent
 * @param clear_values Clear values
 * @param clear_count Number of clear values
 */
void ve_command_buffer_begin_render_pass(ve_command_buffer* cmd,
                                        VkRenderPass render_pass,
                                        VkFramebuffer framebuffer,
                                        VkOffset2D offset,
                                        VkExtent2D extent,
                                        const VkClearValue* clear_values,
                                        uint32_t clear_count);

/**
 * @brief End a render pass
 *
 * @param cmd Command buffer
 */
void ve_command_buffer_end_render_pass(ve_command_buffer* cmd);

/**
 * @brief Set viewport
 *
 * @param cmd Command buffer
 * @param viewport Viewport
 */
void ve_command_buffer_set_viewport(ve_command_buffer* cmd, const VkViewport* viewport);

/**
 * @brief Set scissor
 *
 * @param cmd Command buffer
 * @param scissor Scissor rect
 */
void ve_command_buffer_set_scissor(ve_command_buffer* cmd, const VkRect2D* scissor);

/**
 * @brief Set multiple viewports
 *
 * @param cmd Command buffer
 * @param first_viewport First viewport index
 * @param viewport_count Number of viewports
 * @param viewports Viewport array
 */
void ve_command_buffer_set_viewports(ve_command_buffer* cmd,
                                    uint32_t first_viewport,
                                    uint32_t viewport_count,
                                    const VkViewport* viewports);

/**
 * @brief Set multiple scissors
 *
 * @param cmd Command buffer
 * @param first_scissor First scissor index
 * @param scissor_count Number of scissors
 * @param scissors Scissor array
 */
void ve_command_buffer_set_scissors(ve_command_buffer* cmd,
                                   uint32_t first_scissor,
                                   uint32_t scissor_count,
                                   const VkRect2D* scissors);

/**
 * @brief Bind graphics pipeline
 *
 * @param cmd Command buffer
 * @param pipeline Pipeline to bind
 */
void ve_command_buffer_bind_pipeline(ve_command_buffer* cmd, VkPipeline pipeline);

/**
 * @brief Bind vertex buffers
 *
 * @param cmd Command buffer
 * @param first_binding First binding index
 * @param binding_count Number of bindings
 * @param buffers Buffer array
 * @param offsets Offset array
 */
void ve_command_buffer_bind_vertex_buffers(ve_command_buffer* cmd,
                                          uint32_t first_binding,
                                          uint32_t binding_count,
                                          const VkBuffer* buffers,
                                          const VkDeviceSize* offsets);

/**
 * @brief Bind index buffer
 *
 * @param cmd Command buffer
 * @param buffer Index buffer
 * @param offset Byte offset
 * @param index_type Index type
 */
void ve_command_buffer_bind_index_buffer(ve_command_buffer* cmd,
                                        VkBuffer buffer,
                                        VkDeviceSize offset,
                                        VkIndexType index_type);

/**
 * @brief Draw indexed
 *
 * @param cmd Command buffer
 * @param index_count Number of indices
 * @param instance_count Number of instances
 * @param first_index First index
 * @param vertex_offset Vertex offset
 * @param first_instance First instance
 */
void ve_command_buffer_draw_indexed(ve_command_buffer* cmd,
                                   uint32_t index_count,
                                   uint32_t instance_count,
                                   uint32_t first_index,
                                   int32_t vertex_offset,
                                   uint32_t first_instance);

/**
 * @brief Draw
 *
 * @param cmd Command buffer
 * @param vertex_count Number of vertices
 * @param instance_count Number of instances
 * @param first_vertex First vertex
 * @param first_instance First instance
 */
void ve_command_buffer_draw(ve_command_buffer* cmd,
                           uint32_t vertex_count,
                           uint32_t instance_count,
                           uint32_t first_vertex,
                           uint32_t first_instance);

/**
 * @brief Dispatch compute
 *
 * @param cmd Command buffer
 * @param group_count_x X group count
 * @param group_count_y Y group count
 * @param group_count_z Z group count
 */
void ve_command_buffer_dispatch(ve_command_buffer* cmd,
                               uint32_t group_count_x,
                               uint32_t group_count_y,
                               uint32_t group_count_z);

/**
 * @brief Pipeline barrier
 *
 * @param cmd Command buffer
 * @param src_stage_mask Source pipeline stages
 * @param dst_stage_mask Destination pipeline stages
 * @param dependency_flags Dependency flags
 * @param memory_barrier_count Memory barrier count
 * @param memory_barriers Memory barriers
 * @param buffer_memory_barrier_count Buffer memory barrier count
 * @param buffer_memory_barriers Buffer memory barriers
 * @param image_memory_barrier_count Image memory barrier count
 * @param image_memory_barriers Image memory barriers
 */
void ve_command_buffer_pipeline_barrier(ve_command_buffer* cmd,
                                       VkPipelineStageFlags src_stage_mask,
                                       VkPipelineStageFlags dst_stage_mask,
                                       VkDependencyFlags dependency_flags,
                                       uint32_t memory_barrier_count,
                                       const VkMemoryBarrier* memory_barriers,
                                       uint32_t buffer_memory_barrier_count,
                                       const VkBufferMemoryBarrier* buffer_memory_barriers,
                                       uint32_t image_memory_barrier_count,
                                       const VkImageMemoryBarrier* image_memory_barriers);

/**
 * @brief Image memory barrier (convenience)
 *
 * @param cmd Command buffer
 * @param image Image
 * @param old_layout Old layout
 * @param new_layout New layout
 * @param src_access_mask Source access mask
 * @param dst_access_mask Destination access mask
 * @param src_stage_mask Source stage mask
 * @param dst_stage_mask Destination stage mask
 * @param subresource_range Subresource range
 */
void ve_command_buffer_image_barrier(ve_command_buffer* cmd,
                                    VkImage image,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout,
                                    VkAccessFlags src_access_mask,
                                    VkAccessFlags dst_access_mask,
                                    VkPipelineStageFlags src_stage_mask,
                                    VkPipelineStageFlags dst_stage_mask,
                                    const VkImageSubresourceRange* subresource_range);

/**
 * @brief Copy buffer
 *
 * @param cmd Command buffer
 * @param src Source buffer
 * @param dst Destination buffer
 * @param region_count Number of regions
 * @param regions Copy regions
 */
void ve_command_buffer_copy_buffer(ve_command_buffer* cmd,
                                  VkBuffer src,
                                  VkBuffer dst,
                                  uint32_t region_count,
                                  const VkBufferCopy* regions);

/**
 * @brief Copy buffer to image
 *
 * @param cmd Command buffer
 * @param src Source buffer
 * @param dst Destination image
 * @param region_count Number of regions
 * @param regions Copy regions
 */
void ve_command_buffer_copy_buffer_to_image(ve_command_buffer* cmd,
                                           VkBuffer src,
                                           VkImage dst,
                                           uint32_t region_count,
                                           const VkBufferImageCopy* regions);

/**
 * @brief Blit image
 *
 * @param cmd Command buffer
 * @param src Source image
 * @param src_layout Source layout
 * @param dst Destination image
 * @param dst_layout Destination layout
 * @param region_count Number of regions
 * @param regions Blit regions
 * @param filter Filter mode
 */
void ve_command_buffer_blit_image(ve_command_buffer* cmd,
                                 VkImage src,
                                 VkImageLayout src_layout,
                                 VkImage dst,
                                 VkImageLayout dst_layout,
                                 uint32_t region_count,
                                 const VkImageBlit* regions,
                                 VkFilter filter);

/**
 * @brief Get current frame command buffer
 *
 * @param type Command buffer type
 * @return Command buffer or NULL
 */
ve_command_buffer* ve_command_buffer_get_current(ve_command_buffer_type type);

/**
 * @brief Begin frame command buffer
 *
 * @param type Command buffer type
 * @return Command buffer or NULL
 */
ve_command_buffer* ve_command_buffer_begin_frame(ve_command_buffer_type type);

/**
 * @brief End and submit frame command buffer
 *
 * @param cmd Command buffer
 * @return VK_SUCCESS on success
 */
VkResult ve_command_buffer_end_frame(ve_command_buffer* cmd);

#ifdef __cplusplus
}
#endif

#endif /* VE_COMMAND_BUFFER_H */
