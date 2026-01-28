/**
 * @file assert.c
 * @brief Debug assertion system implementation
 */

#include "assert.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

static ve_assert_mode g_assert_mode = VE_ASSERT_MODE_DEBUG_BREAK;
static ve_assert_callback_fn g_assert_callback = NULL;

#ifdef _WIN32
    #include <windows.h>
#else
    #include <signal.h>
#endif

void ve_assert_set_mode(ve_assert_mode mode) {
    g_assert_mode = mode;
}

ve_assert_mode ve_assert_get_mode(void) {
    return g_assert_mode;
}

void ve_assert_set_callback(ve_assert_callback_fn callback) {
    g_assert_callback = callback;
}

static void debug_break(void) {
#ifdef _WIN32
    if (IsDebuggerPresent()) {
        DebugBreak();
    } else {
        /* If no debugger is attached, just terminate */
        TerminateProcess(GetCurrentProcess(), (UINT)1);
    }
#else
    raise(SIGTRAP);
#endif
}

bool ve_assert_handle(const ve_assert_info* info) {
    /* Log the assertion failure */
    VE_LOG_ERROR("=== ASSERTION FAILED ===");
    VE_LOG_ERROR("Expression: %s", info->expression);
    VE_LOG_ERROR("File: %s:%d", info->file, info->line);
    VE_LOG_ERROR("Function: %s", info->function);
    if (info->message) {
        VE_LOG_ERROR("Message: %s", info->message);
    }

    /* Handle based on mode */
    bool should_continue = false;

    switch (g_assert_mode) {
        case VE_ASSERT_MODE_DEBUG_BREAK:
            debug_break();
            should_continue = false;
            break;

        case VE_ASSERT_MODE_LOG_AND_CONTINUE:
            should_continue = true;
            break;

        case VE_ASSERT_MODE_LOG_AND_EXIT:
            should_continue = false;
            break;

        case VE_ASSERT_MODE_CALLBACK:
            if (g_assert_callback) {
                should_continue = g_assert_callback(info);
            } else {
                /* Fallback to debug break if no callback set */
                debug_break();
                should_continue = false;
            }
            break;
    }

    return should_continue;
}

void ve_assert_fatal(const char* expression, const char* file, int line,
                     const char* function, const char* message) {
    VE_LOG_FATAL("=== FATAL ASSERTION ===");
    VE_LOG_FATAL("Expression: %s", expression);
    VE_LOG_FATAL("File: %s:%d", file, line);
    VE_LOG_FATAL("Function: %s", function);
    if (message) {
        VE_LOG_FATAL("Message: %s", message);
    }

    /* Flush logs before terminating */
    ve_logger_flush();

    /* Terminate the application */
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 1);
#else
    abort();
#endif
}
