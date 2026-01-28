/**
 * @file assert.h
 * @brief Debug assertion system
 */

#ifndef VE_ASSERT_H
#define VE_ASSERT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Assert behavior modes
 */
typedef enum ve_assert_mode {
    VE_ASSERT_MODE_DEBUG_BREAK,   /* Break into debugger */
    VE_ASSERT_MODE_LOG_AND_CONTINUE,  /* Log but continue */
    VE_ASSERT_MODE_LOG_AND_EXIT,  /* Log and exit application */
    VE_ASSERT_MODE_CALLBACK,      /* Call custom callback */
} ve_assert_mode;

/**
 * @brief Assert info structure
 */
typedef struct ve_assert_info {
    const char* expression;
    const char* file;
    int line;
    const char* function;
    const char* message;
} ve_assert_info;

/**
 * @brief Custom assert callback type
 *
 * @param info Assert information
 * @return true to continue execution, false to terminate
 */
typedef bool (*ve_assert_callback_fn)(const ve_assert_info* info);

/**
 * @brief Set the assert behavior mode
 *
 * @param mode New assert mode
 */
void ve_assert_set_mode(ve_assert_mode mode);

/**
 * @brief Get the current assert mode
 *
 * @return Current assert mode
 */
ve_assert_mode ve_assert_get_mode(void);

/**
 * @brief Set a custom assert callback
 *
 * @param callback Callback function (only used in VE_ASSERT_MODE_CALLBACK)
 */
void ve_assert_set_callback(ve_assert_callback_fn callback);

/**
 * @brief Core assert handler
 *
 * @param info Assert information
 * @return true to continue, false to terminate
 */
bool ve_assert_handle(const ve_assert_info* info);

/**
 * @brief Trigger a fatal assert (always fails)
 *
 * @param expression Failed expression
 * @param file Source file
 * @param line Line number
 * @param function Function name
 * @param message Optional message
 */
void ve_assert_fatal(const char* expression, const char* file, int line,
                     const char* function, const char* message);

/* Assert macros */

/**
 * @brief Debug-only assert (removed in release builds)
 */
#ifdef NDEBUG
    #define VE_ASSERT_DEBUG(expr) ((void)0)
    #define VE_ASSERT_DEBUG_MSG(expr, msg) ((void)0)
#else
    #define VE_ASSERT_DEBUG(expr) \
        do { \
            if (!(expr)) { \
                ve_assert_info info = { #expr, __FILE__, __LINE__, __func__, NULL }; \
                if (!ve_assert_handle(&info)) { \
                    ve_assert_fatal(#expr, __FILE__, __LINE__, __func__, NULL); \
                } \
            } \
        } while (0)

    #define VE_ASSERT_DEBUG_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                ve_assert_info info = { #expr, __FILE__, __LINE__, __func__, msg }; \
                if (!ve_assert_handle(&info)) { \
                    ve_assert_fatal(#expr, __FILE__, __LINE__, __func__, msg); \
                } \
            } \
        } while (0)
#endif

/**
 * @brief Always-on assert (even in release builds)
 */
#define VE_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            ve_assert_info info = { #expr, __FILE__, __LINE__, __func__, NULL }; \
            if (!ve_assert_handle(&info)) { \
                ve_assert_fatal(#expr, __FILE__, __LINE__, __func__, NULL); \
            } \
        } \
    } while (0)

#define VE_ASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            ve_assert_info info = { #expr, __FILE__, __LINE__, __func__, msg }; \
            if (!ve_assert_handle(&info)) { \
                ve_assert_fatal(#expr, __FILE__, __LINE__, __func__, msg); \
            } \
        } \
    } while (0)

/**
 * @brief Static assertion for compile-time checks
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define VE_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#elif defined(_MSC_VER)
    #define VE_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
#else
    /* Fallback using typedef trick */
    #define VE_STATIC_ASSERT(expr, msg) \
        typedef char VE_STATIC_ASSERT_##_##msg[(expr) ? 1 : -1]
#endif

/**
 * @brief Unconditional assert with message
 */
#define VE_ASSERT_FAIL(msg) \
    do { \
        ve_assert_info info = { "false", __FILE__, __LINE__, __func__, msg }; \
        if (!ve_assert_handle(&info)) { \
            ve_assert_fatal("false", __FILE__, __LINE__, __func__, msg); \
        } \
    } while (0)

/**
 * @brief Verify macro - assert in debug, evaluate in release
 */
#ifdef NDEBUG
    #define VE_VERIFY(expr) ((void)(expr))
#else
    #define VE_VERIFY(expr) VE_ASSERT(expr)
#endif

/**
 * @brief Null pointer check
 */
#define VE_ASSERT_NOT_NULL(ptr) \
    VE_ASSERT_MSG((ptr) != NULL, #ptr " is NULL")

/**
 * @brief Range check
 */
#define VE_ASSERT_IN_RANGE(val, min, max) \
    VE_ASSERT_MSG((val) >= (min) && (val) <= (max), \
                  #val " = %d is out of range [%d, %d]", (int)(val), (int)(min), (int)(max))

/**
 * @brief Array bounds check
 */
#define VE_ASSERT_INDEX(index, size) \
    VE_ASSERT_MSG((index) < (size), \
                  "Index %d out of bounds for size %d", (int)(index), (int)(size))

/**
 * @brief Vulkan result check (helper for Vulkan API calls)
 */
#define VE_VK_CHECK(result) \
    do { \
        VkResult _res = (result); \
        if (_res != VK_SUCCESS) { \
            VE_ASSERT_FAIL("Vulkan call failed with error: " #result); \
        } \
    } while (0)

/**
 * @brief Not implemented assert
 */
#define VE_NOT_IMPLEMENTED() \
    VE_ASSERT_FAIL("Not implemented: " __FILE__ ":" VE_STRINGIFY(__LINE__))

#define VE_STRINGIFY(x) VE_STRINGIFY_IMPL(x)
#define VE_STRINGIFY_IMPL(x) #x

/**
 * @brief Unreachable code marker
 */
#if defined(__GNUC__) || defined(__clang__)
    #define VE_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define VE_UNREACHABLE() __assume(0)
#else
    #define VE_UNREACHABLE() VE_ASSERT_FAIL("Unreachable code")
#endif

#ifdef __cplusplus
}
#endif

#endif /* VE_ASSERT_H */
