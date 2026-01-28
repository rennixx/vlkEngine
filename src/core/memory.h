/**
 * @file memory.h
 * @brief Memory allocation and tracking system
 */

#ifndef VE_MEMORY_H
#define VE_MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory tag for tracking allocations
 */
typedef enum ve_memory_tag {
    VE_MEMORY_TAG_UNKNOWN      = 0,
    VE_MEMORY_TAG_CORE         = 1 << 0,
    VE_MEMORY_TAG_RENDERER     = 1 << 1,
    VE_MEMORY_TAG_VULKAN       = 1 << 2,
    VE_MEMORY_TAG_ECS          = 1 << 3,
    VE_MEMORY_TAG_SCENE        = 1 << 4,
    VE_MEMORY_TAG_ASSET        = 1 << 5,
    VE_MEMORY_TAG_TEXTURE      = 1 << 6,
    VE_MEMORY_TAG_MESH         = 1 << 7,
    VE_MEMORY_TAG_SHADER       = 1 << 8,
    VE_MEMORY_TAG_STRING       = 1 << 9,
    VE_MEMORY_TAG_APPLICATION  = 1 << 10,
    VE_MEMORY_TAG_MAX
} ve_memory_tag;

/**
 * @brief Arena allocator for temporary allocations
 */
typedef struct ve_arena {
    void* memory;
    size_t capacity;
    size_t used;
    size_t alignment;
    struct ve_arena* parent;  /* For arena hierarchy */
} ve_arena;

/**
 * @brief Memory pool for fixed-size allocations
 */
typedef struct ve_pool {
    void* memory;
    size_t element_size;
    size_t capacity;
    size_t used;
    void* free_list;  /* Linked list of free blocks */
} ve_pool;

/**
 * @brief Memory statistics
 */
typedef struct ve_memory_stats {
    size_t total_allocated;
    size_t total_freed;
    size_t allocation_count;
    size_t tag_usage[VE_MEMORY_TAG_MAX];
} ve_memory_stats;

/**
 * @brief Initialize the memory system
 *
 * @return true on success, false on failure
 */
bool ve_memory_init(void);

/**
 * @brief Shutdown the memory system
 */
void ve_memory_shutdown(void);

/**
 * @brief Allocate memory with tracking
 *
 * @param size Size in bytes
 * @param tag Memory tag for tracking
 * @return Pointer to allocated memory, or NULL on failure
 */
void* ve_allocate(size_t size, ve_memory_tag tag);

/**
 * @brief Free memory with tracking
 *
 * @param memory Pointer to memory to free
 */
void ve_free(void* memory);

/**
 * @brief Reallocate memory with tracking
 *
 * @param memory Original memory pointer
 * @param new_size New size in bytes
 * @param tag Memory tag for tracking
 * @return Pointer to reallocated memory, or NULL on failure
 */
void* ve_reallocate(void* memory, size_t new_size, ve_memory_tag tag);

/**
 * @brief Zero-initialized allocation
 *
 * @param count Number of elements
 * @param size Size of each element
 * @param tag Memory tag for tracking
 * @return Pointer to allocated memory, or NULL on failure
 */
void* ve_allocate_cleared(size_t count, size_t size, ve_memory_tag tag);

/**
 * @brief Duplicate a string
 *
 * @param str String to duplicate
 * @param tag Memory tag for tracking
 * @return Pointer to duplicated string, or NULL on failure
 */
char* ve_string_duplicate(const char* str, ve_memory_tag tag);

/**
 * @brief Get memory statistics
 *
 * @return Memory statistics snapshot
 */
ve_memory_stats ve_memory_get_stats(void);

/**
 * @brief Reset memory statistics
 */
void ve_memory_reset_stats(void);

/* Arena allocator functions */

/**
 * @brief Create a new arena allocator
 *
 * @param capacity Total capacity in bytes
 * @param alignment Memory alignment (default 16)
 * @param parent Optional parent arena
 * @return New arena, or NULL on failure
 */
ve_arena* ve_arena_create(size_t capacity, size_t alignment, ve_arena* parent);

/**
 * @brief Destroy an arena allocator
 *
 * @param arena Arena to destroy
 */
void ve_arena_destroy(ve_arena* arena);

/**
 * @brief Allocate from arena
 *
 * @param arena Arena allocator
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL if full
 */
void* ve_arena_allocate(ve_arena* arena, size_t size);

/**
 * @brief Reset arena (free all allocations)
 *
 * @param arena Arena to reset
 */
void ve_arena_reset(ve_arena* arena);

/**
 * @brief Get arena usage statistics
 *
 * @param arena Arena allocator
 * @return Bytes used out of total capacity
 */
size_t ve_arena_get_usage(const ve_arena* arena);

/**
 * @brief Create a memory snapshot for rollback
 *
 * @param arena Arena allocator
 * @return Current allocation position
 */
size_t ve_arena_get_position(ve_arena* arena);

/**
 * @brief Rollback to a snapshot position
 *
 * @param arena Arena allocator
 * @param position Position from ve_arena_get_position
 */
void ve_arena_set_position(ve_arena* arena, size_t position);

/**
 * @brief Temporary arena allocation scope
 *
 * Usage:
 * @code
 * VE_ARENA_SCOPE(temp_arena) {
 *     void* temp = ve_arena_allocate(temp_arena, 1024);
 *     // Use temp memory...
 * } // Automatically reset
 * @endcode
 */
#define VE_ARENA_SCOPE(arena_var) \
    for (ve_arena* arena_var = ve_arena_get_thread_arena(), \
         *_scope_##arena_var = (ve_arena*)1; \
         _scope_##arena_var != NULL; \
         (ve_arena_reset(arena_var), _scope_##arena_var = NULL))

/**
 * @brief Get thread-local arena for temporary allocations
 *
 * @return Thread-local arena
 */
ve_arena* ve_arena_get_thread_arena(void);

/* Memory pool functions */

/**
 * @brief Create a memory pool
 *
 * @param element_size Size of each element
 * @param capacity Maximum number of elements
 * @return New pool, or NULL on failure
 */
ve_pool* ve_pool_create(size_t element_size, size_t capacity);

/**
 * @brief Destroy a memory pool
 *
 * @param pool Pool to destroy
 */
void ve_pool_destroy(ve_pool* pool);

/**
 * @brief Allocate from pool
 *
 * @param pool Memory pool
 * @return Pointer to element, or NULL if full
 */
void* ve_pool_allocate(ve_pool* pool);

/**
 * @brief Free element back to pool
 *
 * @param pool Memory pool
 * @param element Element to free
 */
void ve_pool_free(ve_pool* pool, void* element);

/**
 * @brief Reset pool (free all elements)
 *
 * @param pool Pool to reset
 */
void ve_pool_reset(ve_pool* pool);

/**
 * @brief Check if pointer belongs to pool
 *
 * @param pool Memory pool
 * @param ptr Pointer to check
 * @return true if pointer is from this pool
 */
bool ve_pool_contains(const ve_pool* pool, const void* ptr);

/* Alignment utilities */

/**
 * @brief Align a size to the next multiple of alignment
 *
 * @param size Size to align
 * @param alignment Alignment requirement
 * @return Aligned size
 */
size_t ve_align_size(size_t size, size_t alignment);

/**
 * @brief Check if pointer is aligned
 *
 * @param ptr Pointer to check
 * @param alignment Alignment requirement
 * @return true if aligned
 */
bool ve_is_aligned(const void* ptr, size_t alignment);

/* Debug utilities */

/**
 * @brief Validate all allocations (debug build only)
 *
 * @return true if all allocations are valid
 */
bool ve_memory_validate(void);

/**
 * @brief Dump memory statistics to log
 *
 * @param tag Filter by tag (VE_MEMORY_TAG_UNKNOWN for all)
 */
void ve_memory_dump_stats(ve_memory_tag tag);

/**
 * @brief Check for memory leaks
 *
 * @return true if leaks detected
 */
bool ve_memory_check_leaks(void);

/* Convenience macros */
#define VE_ALLOCATE(size) ve_allocate(size, VE_MEMORY_TAG_UNKNOWN)
#define VE_ALLOCATE_TAG(size, tag) ve_allocate(size, tag)
#define VE_FREE(ptr) ve_free(ptr)
#define VE_REALLOC(ptr, size) ve_reallocate(ptr, size, VE_MEMORY_TAG_UNKNOWN)
#define VE_CALLOC(count, size) ve_allocate_cleared(count, size, VE_MEMORY_TAG_UNKNOWN)

#ifdef __cplusplus
}
#endif

#endif /* VE_MEMORY_H */
