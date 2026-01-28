/**
 * @file memory.c
 * @brief Memory allocation and tracking implementation
 */

#define _CRT_SECURE_NO_WARNINGS
#include "memory.h"
#include "logger.h"
#include "assert.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#define VE_MEMORY_ALIGNMENT 16
#define VE_THREAD_ARENA_SIZE (1024 * 1024)  /* 1MB per thread */

/* Allocation header for tracking */
typedef struct ve_allocation_header {
    size_t size;
    ve_memory_tag tag;
    const char* file;
    int line;
    struct ve_allocation_header* next;
    struct ve_allocation_header* prev;
} ve_allocation_header;

#define HEADER_TO_USER(hdr) ((void*)((char*)(hdr) + sizeof(ve_allocation_header)))
#define USER_TO_HEADER(ptr) ((ve_allocation_header*)((char*)(ptr) - sizeof(ve_allocation_header)))

/* Memory system state */
typedef struct ve_memory_state {
    ve_memory_stats stats;
    ve_allocation_header* allocation_list;
    bool initialized;
    bool track_allocations;

    /* Thread-local arena support */
#if defined(_WIN32) || defined(_WIN64)
    DWORD thread_arena_tls;
#else
    pthread_key_t thread_arena_tls;
#endif
} ve_memory_state;

static ve_memory_state g_memory = {0};

/* Platform-specific TLS functions */
static void init_tls(void) {
#if defined(_WIN32) || defined(_WIN64)
    g_memory.thread_arena_tls = TlsAlloc();
#else
    pthread_key_create(&g_memory.thread_arena_tls, NULL);
#endif
}

static ve_arena* get_thread_arena(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (ve_arena*)TlsGetValue(g_memory.thread_arena_tls);
#else
    return (ve_arena*)pthread_getspecific(g_memory.thread_arena_tls);
#endif
}

static void set_thread_arena(ve_arena* arena) {
#if defined(_WIN32) || defined(_WIN64)
    TlsSetValue(g_memory.thread_arena_tls, arena);
#else
    pthread_setspecific(g_memory.thread_arena_tls, arena);
#endif
}

/* Arena TLS cleanup */
#if !defined(_WIN32) && !defined(_WIN64)
static void arena_cleanup(void* ptr) {
    if (ptr) {
        ve_arena_destroy((ve_arena*)ptr);
    }
}
#endif

bool ve_memory_init(void) {
    if (g_memory.initialized) {
        return true;
    }

    memset(&g_memory, 0, sizeof(ve_memory_state));
    g_memory.track_allocations = true;

    init_tls();

    g_memory.initialized = true;

    VE_LOG_INFO("Memory system initialized");
    return true;
}

void ve_memory_shutdown(void) {
    if (!g_memory.initialized) {
        return;
    }

    /* Clean up thread arena if exists */
    ve_arena* arena = get_thread_arena();
    if (arena) {
        ve_arena_destroy(arena);
        set_thread_arena(NULL);
    }

    /* Check for leaks */
    if (g_memory.track_allocations && g_memory.allocation_list != NULL) {
        VE_LOG_WARN("Memory leaks detected:");
        ve_allocation_header* hdr = g_memory.allocation_list;
        size_t leak_count = 0;
        size_t leak_size = 0;

        while (hdr) {
            VE_LOG_WARN("  Leak: %zu bytes, tag %d, at %s:%d",
                hdr->size, hdr->tag, hdr->file ? hdr->file : "unknown", hdr->line);
            leak_size += hdr->size;
            leak_count++;
            hdr = hdr->next;
        }

        VE_LOG_WARN("Total leaks: %zu allocations, %zu bytes", leak_count, leak_size);
    }

    g_memory.initialized = false;

    /* Cleanup TLS */
#if !defined(_WIN32) && !defined(_WIN64)
    pthread_key_delete(g_memory.thread_arena_tls);
#endif

    VE_LOG_INFO("Memory system shutdown");
}

void* ve_allocate(size_t size, ve_memory_tag tag) {
    if (size == 0) {
        return NULL;
    }

    /* Align size */
    size = ve_align_size(size, VE_MEMORY_ALIGNMENT);

    /* Allocate with header */
    ve_allocation_header* hdr = (ve_allocation_header*)malloc(sizeof(ve_allocation_header) + size);
    if (!hdr) {
        VE_LOG_ERROR("Failed to allocate %zu bytes", size);
        return NULL;
    }

    hdr->size = size;
    hdr->tag = tag;
    hdr->file = NULL;
    hdr->line = 0;

    /* Track allocation */
    if (g_memory.track_allocations) {
        hdr->next = g_memory.allocation_list;
        hdr->prev = NULL;

        if (g_memory.allocation_list) {
            g_memory.allocation_list->prev = hdr;
        }
        g_memory.allocation_list = hdr;

        /* Update stats */
        g_memory.stats.total_allocated += size;
        g_memory.stats.allocation_count++;
        if (tag < VE_MEMORY_TAG_MAX) {
            g_memory.stats.tag_usage[tag] += size;
        }
    }

    return HEADER_TO_USER(hdr);
}

void ve_free(void* memory) {
    if (!memory) {
        return;
    }

    ve_allocation_header* hdr = USER_TO_HEADER(memory);

    /* Untrack allocation */
    if (g_memory.track_allocations) {
        g_memory.stats.total_freed += hdr->size;
        g_memory.stats.allocation_count--;

        if (hdr->tag < VE_MEMORY_TAG_MAX) {
            g_memory.stats.tag_usage[hdr->tag] -= hdr->size;
        }

        /* Remove from list */
        if (hdr->prev) {
            hdr->prev->next = hdr->next;
        } else {
            g_memory.allocation_list = hdr->next;
        }

        if (hdr->next) {
            hdr->next->prev = hdr->prev;
        }
    }

    free(hdr);
}

void* ve_reallocate(void* memory, size_t new_size, ve_memory_tag tag) {
    if (!memory) {
        return ve_allocate(new_size, tag);
    }

    if (new_size == 0) {
        ve_free(memory);
        return NULL;
    }

    ve_allocation_header* old_hdr = USER_TO_HEADER(memory);
    size_t old_size = old_hdr->size;
    new_size = ve_align_size(new_size, VE_MEMORY_ALIGNMENT);

    /* Allocate new block */
    void* new_memory = ve_allocate(new_size, tag);
    if (!new_memory) {
        return NULL;
    }

    /* Copy old data */
    size_t copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_memory, memory, copy_size);

    /* Free old block */
    ve_free(memory);

    return new_memory;
}

void* ve_allocate_cleared(size_t count, size_t size, ve_memory_tag tag) {
    void* memory = ve_allocate(count * size, tag);
    if (memory) {
        memset(memory, 0, count * size);
    }
    return memory;
}

char* ve_string_duplicate(const char* str, ve_memory_tag tag) {
    if (!str) {
        return NULL;
    }

    size_t len = strlen(str) + 1;
    char* duplicate = (char*)ve_allocate(len, tag);
    if (duplicate) {
        memcpy(duplicate, str, len);
    }
    return duplicate;
}

ve_memory_stats ve_memory_get_stats(void) {
    return g_memory.stats;
}

void ve_memory_reset_stats(void) {
    memset(&g_memory.stats, 0, sizeof(ve_memory_stats));
}

/* Arena allocator implementation */

ve_arena* ve_arena_create(size_t capacity, size_t alignment, ve_arena* parent) {
    if (alignment == 0) {
        alignment = VE_MEMORY_ALIGNMENT;
    }

    ve_arena* arena = (ve_arena*)malloc(sizeof(ve_arena));
    if (!arena) {
        return NULL;
    }

    /* Allocate aligned memory */
#if defined(_WIN32) || defined(_WIN64)
    arena->memory = _aligned_malloc(capacity, alignment);
#else
    posix_memalign(&arena->memory, alignment, capacity);
#endif

    if (!arena->memory) {
        free(arena);
        return NULL;
    }

    arena->capacity = capacity;
    arena->used = 0;
    arena->alignment = alignment;
    arena->parent = parent;

    return arena;
}

void ve_arena_destroy(ve_arena* arena) {
    if (!arena) {
        return;
    }

#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(arena->memory);
#else
    free(arena->memory);
#endif
    free(arena);
}

void* ve_arena_allocate(ve_arena* arena, size_t size) {
    if (!arena || size == 0) {
        return NULL;
    }

    /* Align size */
    size = ve_align_size(size, arena->alignment);

    /* Check capacity */
    if (arena->used + size > arena->capacity) {
        /* Try parent arena */
        if (arena->parent) {
            return ve_arena_allocate(arena->parent, size);
        }
        return NULL;
    }

    void* ptr = (char*)arena->memory + arena->used;
    arena->used += size;

    return ptr;
}

void ve_arena_reset(ve_arena* arena) {
    if (arena) {
        arena->used = 0;
    }
}

size_t ve_arena_get_usage(const ve_arena* arena) {
    return arena ? arena->used : 0;
}

size_t ve_arena_get_position(ve_arena* arena) {
    return arena ? arena->used : 0;
}

void ve_arena_set_position(ve_arena* arena, size_t position) {
    if (arena && position <= arena->used) {
        arena->used = position;
    }
}

ve_arena* ve_arena_get_thread_arena(void) {
    ve_arena* arena = get_thread_arena();
    if (!arena) {
        arena = ve_arena_create(VE_THREAD_ARENA_SIZE, VE_MEMORY_ALIGNMENT, NULL);
        if (arena) {
            set_thread_arena(arena);
        }
    }
    return arena;
}

/* Memory pool implementation */

ve_pool* ve_pool_create(size_t element_size, size_t capacity) {
    /* Align element size */
    element_size = ve_align_size(element_size, VE_MEMORY_ALIGNMENT);

    ve_pool* pool = (ve_pool*)malloc(sizeof(ve_pool));
    if (!pool) {
        return NULL;
    }

    pool->memory = malloc(element_size * capacity);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }

    pool->element_size = element_size;
    pool->capacity = capacity;
    pool->used = 0;
    pool->free_list = NULL;

    return pool;
}

void ve_pool_destroy(ve_pool* pool) {
    if (!pool) {
        return;
    }

    free(pool->memory);
    free(pool);
}

void* ve_pool_allocate(ve_pool* pool) {
    if (!pool) {
        return NULL;
    }

    /* Check free list first */
    if (pool->free_list) {
        void* element = pool->free_list;
        pool->free_list = *(void**)pool->free_list;
        return element;
    }

    /* Allocate from pool */
    if (pool->used >= pool->capacity) {
        return NULL;
    }

    void* element = (char*)pool->memory + (pool->used * pool->element_size);
    pool->used++;

    return element;
}

void ve_pool_free(ve_pool* pool, void* element) {
    if (!pool || !element) {
        return;
    }

    /* Add to free list */
    *(void**)element = pool->free_list;
    pool->free_list = element;
}

void ve_pool_reset(ve_pool* pool) {
    if (pool) {
        pool->used = 0;
        pool->free_list = NULL;
    }
}

bool ve_pool_contains(const ve_pool* pool, const void* ptr) {
    if (!pool || !ptr) {
        return false;
    }

    return (ptr >= pool->memory &&
            (const char*)ptr < (const char*)pool->memory + (pool->capacity * pool->element_size));
}

/* Alignment utilities */

size_t ve_align_size(size_t size, size_t alignment) {
    if (alignment == 0) {
        alignment = VE_MEMORY_ALIGNMENT;
    }
    return (size + alignment - 1) & ~(alignment - 1);
}

bool ve_is_aligned(const void* ptr, size_t alignment) {
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

/* Debug utilities */

bool ve_memory_validate(void) {
    /* Validate all tracked allocations */
    ve_allocation_header* hdr = g_memory.allocation_list;
    while (hdr) {
        /* Basic validation - could add more checks */
        if (hdr->size == 0 || hdr->tag >= VE_MEMORY_TAG_MAX) {
            return false;
        }
        hdr = hdr->next;
    }
    return true;
}

void ve_memory_dump_stats(ve_memory_tag tag) {
    if (tag == VE_MEMORY_TAG_UNKNOWN) {
        VE_LOG_INFO("=== Memory Statistics ===");
        VE_LOG_INFO("Total allocated: %zu bytes", g_memory.stats.total_allocated);
        VE_LOG_INFO("Total freed: %zu bytes", g_memory.stats.total_freed);
        VE_LOG_INFO("Current allocations: %zu", g_memory.stats.allocation_count);

        for (int i = 0; i < VE_MEMORY_TAG_MAX; i++) {
            if (g_memory.stats.tag_usage[i] > 0) {
                VE_LOG_INFO("Tag %d: %zu bytes", i, g_memory.stats.tag_usage[i]);
            }
        }
    } else {
        VE_LOG_INFO("Tag %d usage: %zu bytes", tag, g_memory.stats.tag_usage[tag]);
    }
}

bool ve_memory_check_leaks(void) {
    return g_memory.allocation_list != NULL;
}
