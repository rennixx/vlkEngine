/**
 * @file logger.c
 * @brief Logging system implementation
 */

#define VE_LOGGER_IMPL
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
    #define VE_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
    #include <fcntl.h>
#elif defined(__linux__) || defined(__APPLE__)
    #define VE_PLATFORM_UNIX
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/syscall.h>
#endif

#define VE_MAX_LOG_MESSAGE_SIZE 4096
#define VE_MAX_LOG_BUFFER_SIZE (1024 * 1024)  /* 1MB buffer */

/* Level names and colors */
static const struct {
    const char* name;
    const char* short_name;
    ve_log_color color;
} LOG_LEVEL_INFO[] = {
    { "TRACE", "T", VE_COLOR_BRIGHT_WHITE },
    { "DEBUG", "D", VE_COLOR_BRIGHT_CYAN },
    { "INFO",  "I", VE_COLOR_BRIGHT_GREEN },
    { "WARN",  "W", VE_COLOR_BRIGHT_YELLOW },
    { "ERROR", "E", VE_COLOR_BRIGHT_RED },
    { "FATAL", "F", VE_COLOR_BRIGHT_MAGENTA },
    { "OFF",   "X", VE_COLOR_DEFAULT },
};

/* Logger state */
static struct {
    ve_logger_config config;
    FILE* log_file;
    char buffer[VE_MAX_LOG_BUFFER_SIZE];
    size_t buffer_pos;
    size_t current_file_size;
    int current_file_index;
    bool initialized;
    bool console_supports_color;

#ifdef VE_PLATFORM_UNIX
    pthread_mutex_t mutex;
#endif
#ifdef VE_PLATFORM_WINDOWS
    CRITICAL_SECTION mutex;
#endif
} g_logger = {0};

/* Forward declarations */
static void lock_logger(void);
static void unlock_logger(void);
static void flush_buffer(void);
static void write_to_buffer(const char* message, size_t len);
static void rotate_log_file(void);
static void set_terminal_color(ve_log_color color);
static void reset_terminal_color(void);
static const char* get_timestamp(char* buffer, size_t size);
static unsigned long get_thread_id(void);
static const char* basename(const char* path);

/* Implementation */
bool ve_logger_init(const ve_logger_config* config) {
    if (g_logger.initialized) {
        return true;  /* Already initialized */
    }

    /* Initialize mutex */
#ifdef VE_PLATFORM_UNIX
    if (pthread_mutex_init(&g_logger.mutex, NULL) != 0) {
        return false;
    }
#endif
#ifdef VE_PLATFORM_WINDOWS
    InitializeCriticalSection(&g_logger.mutex);
#endif

    /* Set default config if none provided */
    if (config) {
        memcpy(&g_logger.config, config, sizeof(ve_logger_config));
    } else {
        /* Default configuration */
        g_logger.config.level = VE_LOG_INFO;
        g_logger.config.targets = VE_LOG_TARGET_CONSOLE;
        g_logger.config.color_output = true;
        g_logger.config.timestamps = true;
        g_logger.config.thread_ids = false;
        g_logger.config.file_pattern = NULL;
        g_logger.config.max_file_size = 10 * 1024 * 1024;  /* 10MB */
        g_logger.config.max_files = 5;
    }

    /* Check if console supports colors */
    g_logger.console_supports_color = false;
#ifdef VE_PLATFORM_UNIX
    g_logger.console_supports_color = isatty(STDOUT_FILENO);
#endif
#ifdef VE_PLATFORM_WINDOWS
    /* Enable ANSI color support on Windows 10+ */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
        g_logger.console_supports_color = true;
    }
#endif

    /* Open log file if configured */
    if (g_logger.config.targets & VE_LOG_TARGET_FILE) {
        if (g_logger.config.file_pattern) {
            char log_path[512];
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);

            strftime(log_path, sizeof(log_path), g_logger.config.file_pattern, tm_info);

            /* Create log directory if needed */
            const char* last_slash = strrchr(log_path, '/');
            const char* last_backslash = strrchr(log_path, '\\');
            const char* dir_end = last_backslash > last_slash ? last_backslash : last_slash;

            if (dir_end) {
                char dir_path[512];
                size_t dir_len = dir_end - log_path;
                strncpy(dir_path, log_path, dir_len);
                dir_path[dir_len] = '\0';

                /* Create directory (platform-specific) */
#ifdef VE_PLATFORM_WINDOWS
                CreateDirectoryA(dir_path, NULL);
#else
                char mkdir_cmd[600];
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", dir_path);
                system(mkdir_cmd);
#endif
            }

            g_logger.log_file = fopen(log_path, "a");
            if (!g_logger.log_file) {
                /* Fallback to console only */
                g_logger.config.targets &= ~VE_LOG_TARGET_FILE;
            } else {
                /* Get current file size */
                fseek(g_logger.log_file, 0, SEEK_END);
                g_logger.current_file_size = ftell(g_logger.log_file);
            }
        }
    }

    g_logger.buffer_pos = 0;
    g_logger.current_file_index = 0;
    g_logger.initialized = true;

    /* Log initialization message */
    ve_logger_log(VE_LOG_INFO, __FILE__, __LINE__, __func__, "Logger initialized");

    return true;
}

void ve_logger_shutdown(void) {
    if (!g_logger.initialized) {
        return;
    }

    lock_logger();
    flush_buffer();

    if (g_logger.log_file) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }

    g_logger.initialized = false;
    unlock_logger();

    /* Destroy mutex */
#ifdef VE_PLATFORM_UNIX
    pthread_mutex_destroy(&g_logger.mutex);
#endif
#ifdef VE_PLATFORM_WINDOWS
    DeleteCriticalSection(&g_logger.mutex);
#endif
}

ve_log_level ve_logger_get_level(void) {
    return g_logger.config.level;
}

void ve_logger_set_level(ve_log_level level) {
    g_logger.config.level = level;
}

bool ve_logger_should_log(ve_log_level level) {
    return level >= g_logger.config.level && level < VE_LOG_OFF;
}

void ve_logger_log(ve_log_level level, const char* file, int line, const char* func, const char* fmt, ...) {
    if (!ve_logger_should_log(level) || !g_logger.initialized) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    ve_logger_log_va(level, file, line, func, fmt, args);
    va_end(args);
}

void ve_logger_log_va(ve_log_level level, const char* file, int line, const char* func, const char* fmt, va_list args) {
    if (!g_logger.initialized) {
        return;
    }

    lock_logger();

    char message[VE_MAX_LOG_MESSAGE_SIZE];
    int pos = 0;

    /* Build message header */
    if (g_logger.config.timestamps) {
        char timestamp[64];
        get_timestamp(timestamp, sizeof(timestamp));
        pos += snprintf(message + pos, VE_MAX_LOG_MESSAGE_SIZE - pos, "[%s] ", timestamp);
    }

    /* Thread ID */
    if (g_logger.config.thread_ids) {
        pos += snprintf(message + pos, VE_MAX_LOG_MESSAGE_SIZE - pos, "[%lu] ", get_thread_id());
    }

    /* Level and color info for console */
    pos += snprintf(message + pos, VE_MAX_LOG_MESSAGE_SIZE - pos,
        "[%s] ", LOG_LEVEL_INFO[level].short_name);

    /* Source location (optional, can be made configurable) */
    if (level >= VE_LOG_ERROR) {
        pos += snprintf(message + pos, VE_MAX_LOG_MESSAGE_SIZE - pos,
            "[%s:%d:%s] ", basename(file), line, func);
    }

    /* User message */
    pos += vsnprintf(message + pos, VE_MAX_LOG_MESSAGE_SIZE - pos, fmt, args);

    /* Add newline */
    if (pos < VE_MAX_LOG_MESSAGE_SIZE - 1) {
        message[pos] = '\n';
        message[pos + 1] = '\0';
    }

    /* Output to console */
    if (g_logger.config.targets & VE_LOG_TARGET_CONSOLE) {
        if (g_logger.config.color_output && g_logger.console_supports_color) {
            set_terminal_color(LOG_LEVEL_INFO[level].color);
        }

        fputs(message, stdout);

        if (g_logger.config.color_output && g_logger.console_supports_color) {
            reset_terminal_color();
        }
    }

    /* Output to file (without colors) */
    if (g_logger.config.targets & VE_LOG_TARGET_FILE && g_logger.log_file) {
        size_t len = strlen(message);
        write_to_buffer(message, len);

        /* Check file rotation */
        if (g_logger.config.max_file_size > 0) {
            if (g_logger.current_file_size + g_logger.buffer_pos > g_logger.config.max_file_size) {
                flush_buffer();
                rotate_log_file();
            }
        }
    }

    /* Output to debugger on Windows */
#ifdef VE_PLATFORM_WINDOWS
    if (g_logger.config.targets & VE_LOG_TARGET_DEBUGGER) {
        /* Remove newline for debugger output */
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') {
            message[len - 1] = '\0';
        }
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }
#endif

    unlock_logger();

    /* Flush for fatal errors */
    if (level >= VE_LOG_ERROR) {
        ve_logger_flush();
    }
}

void ve_logger_flush(void) {
    if (!g_logger.initialized) {
        return;
    }

    lock_logger();
    flush_buffer();

    if (g_logger.log_file) {
        fflush(g_logger.log_file);
    }

    fflush(stdout);
    unlock_logger();
}

/* Helper functions */
static void lock_logger(void) {
#ifdef VE_PLATFORM_UNIX
    pthread_mutex_lock(&g_logger.mutex);
#endif
#ifdef VE_PLATFORM_WINDOWS
    EnterCriticalSection(&g_logger.mutex);
#endif
}

static void unlock_logger(void) {
#ifdef VE_PLATFORM_UNIX
    pthread_mutex_unlock(&g_logger.mutex);
#endif
#ifdef VE_PLATFORM_WINDOWS
    LeaveCriticalSection(&g_logger.mutex);
#endif
}

static void flush_buffer(void) {
    if (g_logger.buffer_pos == 0) {
        return;
    }

    if (g_logger.log_file) {
        fwrite(g_logger.buffer, 1, g_logger.buffer_pos, g_logger.log_file);
        g_logger.current_file_size += g_logger.buffer_pos;
    }

    g_logger.buffer_pos = 0;
}

static void write_to_buffer(const char* message, size_t len) {
    if (len > VE_MAX_LOG_BUFFER_SIZE) {
        /* Message too large, write directly */
        flush_buffer();
        if (g_logger.log_file) {
            fwrite(message, 1, len, g_logger.log_file);
            g_logger.current_file_size += len;
        }
        return;
    }

    if (g_logger.buffer_pos + len > VE_MAX_LOG_BUFFER_SIZE) {
        flush_buffer();
    }

    memcpy(g_logger.buffer + g_logger.buffer_pos, message, len);
    g_logger.buffer_pos += len;
}

static void rotate_log_file(void) {
    if (!g_logger.log_file || g_logger.config.max_files <= 0) {
        return;
    }

    fclose(g_logger.log_file);

    /* Rotate existing files */
    char old_path[512];
    char new_path[512];
    const char* pattern = g_logger.config.file_pattern;

    for (int i = g_logger.config.max_files - 1; i > 0; i--) {
        if (i == 1) {
            snprintf(old_path, sizeof(old_path), "%s", pattern);
        } else {
            snprintf(old_path, sizeof(old_path), "%s.%d", pattern, i - 1);
        }
        snprintf(new_path, sizeof(new_path), "%s.%d", pattern, i);
        remove(new_path);
        rename(old_path, new_path);
    }

    /* Open new log file */
    g_logger.log_file = fopen(pattern, "w");
    g_logger.current_file_size = 0;
}

static void set_terminal_color(ve_log_color color) {
    printf("\033[%dm", (int)color);
}

static void reset_terminal_color(void) {
    printf("\033[0m");
}

static const char* get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

static unsigned long get_thread_id(void) {
#ifdef VE_PLATFORM_WINDOWS
    return (unsigned long)GetCurrentThreadId();
#elif defined(VE_PLATFORM_UNIX)
    return (unsigned long)syscall(SYS_gettid);
#else
    return 0;
#endif
}

static const char* basename(const char* path) {
    const char* sep = strrchr(path, '/');
    const char* sep2 = strrchr(path, '\\');
    sep = sep2 > sep ? sep2 : sep;
    return sep ? sep + 1 : path;
}

const char* ve_strerror(int errnum) {
#ifdef VE_PLATFORM_WINDOWS
    static char buffer[256];
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errnum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer, sizeof(buffer), NULL
    );

    /* Remove trailing newlines */
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n')) {
        buffer[--len] = '\0';
    }

    return buffer;
#else
    return strerror(errnum);
#endif
}
