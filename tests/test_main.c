/**
 * @file test_main.c
 * @brief Unit test entry point
 */

#include "core/logger.h"
#include "core/memory.h"
#include "core/assert.h"
#include <stdio.h>

/* Simple test framework */
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s:%d - %s\n", __FILE__, __LINE__, #condition); \
            return false; \
        } \
    } while (0)

typedef bool (*test_fn)(void);

typedef struct {
    const char* name;
    test_fn func;
} test_case;

/* External test functions */
bool test_memory_arena(void);
bool test_memory_pool(void);
bool test_ecs_basic(void);

/* Test implementations */

bool test_memory_arena(void) {
    printf("Running test_memory_arena...\n");

    ve_arena* arena = ve_arena_create(1024, 16, NULL);
    TEST_ASSERT(arena != NULL);
    TEST_ASSERT(ve_arena_get_usage(arena) == 0);

    void* ptr1 = ve_arena_allocate(arena, 100);
    TEST_ASSERT(ptr1 != NULL);
    TEST_ASSERT(ve_arena_get_usage(arena) >= 100);

    void* ptr2 = ve_arena_allocate(arena, 200);
    TEST_ASSERT(ptr2 != NULL);

    ve_arena_reset(arena);
    TEST_ASSERT(ve_arena_get_usage(arena) == 0);

    ve_arena_destroy(arena);
    return true;
}

bool test_memory_pool(void) {
    printf("Running test_memory_pool...\n");

    ve_pool* pool = ve_pool_create(64, 10);
    TEST_ASSERT(pool != NULL);

    void* ptr1 = ve_pool_allocate(pool);
    TEST_ASSERT(ptr1 != NULL);

    void* ptr2 = ve_pool_allocate(pool);
    TEST_ASSERT(ptr2 != NULL);
    TEST_ASSERT(ptr1 != ptr2);

    ve_pool_free(pool, ptr1);
    void* ptr3 = ve_pool_allocate(pool);
    TEST_ASSERT(ptr3 == ptr1);  /* Should reuse freed block */

    ve_pool_destroy(pool);
    return true;
}

bool test_ecs_basic(void) {
    printf("Running test_ecs_basic...\n");
    /* TODO: Implement ECS tests */
    return true;
}

/* Test runner */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Running Tests ===\n");

    /* Initialize core systems */
    if (!ve_memory_init()) {
        fprintf(stderr, "Failed to initialize memory system\n");
        return EXIT_FAILURE;
    }

    ve_logger_config logger_config = {
        .level = VE_LOG_INFO,
        .targets = VE_LOG_TARGET_CONSOLE,
        .color_output = true,
    };
    ve_logger_init(&logger_config);

    /* Test suite */
    test_case tests[] = {
        {"memory_arena", test_memory_arena},
        {"memory_pool", test_memory_pool},
        {"ecs_basic", test_ecs_basic},
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        printf("\n");
        if (tests[i].func()) {
            printf("PASS: %s\n", tests[i].name);
            passed++;
        } else {
            printf("FAIL: %s\n", tests[i].name);
        }
    }

    /* Shutdown */
    ve_logger_shutdown();
    ve_memory_shutdown();

    printf("\n=== Tests Complete ===\n");
    printf("Passed: %d/%d\n", passed, total);

    return (passed == total) ? EXIT_SUCCESS : EXIT_FAILURE;
}
