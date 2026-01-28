/**
 * @file logger.h
 * @brief Logging system with spdlog-style API
 */

#ifndef VE_LOGGER_H
#define VE_LOGGER_H

#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log levels
 */
typedef enum ve_log_level {
    VE_LOG_TRACE = 0,
    VE_LOG_DEBUG,
    VE_LOG_INFO,
    VE_LOG_WARN,
    VE_LOG_ERROR,
    VE_LOG_FATAL,
    VE_LOG_OFF
} ve_log_level;

/**
 * @brief Log output targets
 */
typedef enum ve_log_target {
    VE_LOG_TARGET_CONSOLE = 1 << 0,
    VE_LOG_TARGET_FILE    = 1 << 1,
    VE_LOG_TARGET_DEBUGGER = 1 << 2,  /* Windows debugger output */
} ve_log_target;

/**
 * @brief Color codes for terminal output
 */
typedef enum ve_log_color {
    VE_COLOR_RESET   = 0,
    VE_COLOR_DEFAULT = 1,
    VE_COLOR_BLACK   = 30,
    VE_COLOR_RED     = 31,
    VE_COLOR_GREEN   = 32,
    VE_COLOR_YELLOW  = 33,
    VE_COLOR_BLUE    = 34,
    VE_COLOR_MAGENTA = 35,
    VE_COLOR_CYAN    = 36,
    VE_COLOR_WHITE   = 37,
    VE_COLOR_BRIGHT_RED     = 91,
    VE_COLOR_BRIGHT_GREEN   = 92,
    VE_COLOR_BRIGHT_YELLOW  = 93,
    VE_COLOR_BRIGHT_BLUE    = 94,
    VE_COLOR_BRIGHT_MAGENTA = 95,
    VE_COLOR_BRIGHT_CYAN    = 96,
    VE_COLOR_BRIGHT_WHITE   = 97,
} ve_log_color;

/**
 * @brief Logger configuration
 */
typedef struct ve_logger_config {
    ve_log_level level;           /* Minimum log level */
    int targets;                  /* Bitmask of ve_log_target */
    bool color_output;            /* Enable colored console output */
    bool timestamps;              /* Include timestamps in logs */
    bool thread_ids;              /* Include thread IDs in logs */
    const char* file_pattern;     /* Log file pattern (e.g., "logs/engine_%Y%m%d.log") */
    size_t max_file_size;         /* Max size per log file in bytes (0 = unlimited) */
    int max_files;                /* Max number of rotated log files (0 = no rotation) */
} ve_logger_config;

/**
 * @brief Initialize the logging system
 *
 * @param config Logger configuration (NULL for defaults)
 * @return true on success, false on failure
 */
bool ve_logger_init(const ve_logger_config* config);

/**
 * @brief Shutdown the logging system
 */
void ve_logger_shutdown(void);

/**
 * @brief Get the current log level
 *
 * @return Current log level
 */
ve_log_level ve_logger_get_level(void);

/**
 * @brief Set the current log level
 *
 * @param level New log level
 */
void ve_logger_set_level(ve_log_level level);

/**
 * @brief Check if a log level is enabled
 *
 * @param level Level to check
 * @return true if the level is enabled
 */
bool ve_logger_should_log(ve_log_level level);

/**
 * @brief Core logging function
 *
 * @param level Log level
 * @param file Source file path
 * @param line Source line number
 * @param func Function name
 * @param fmt Format string
 * @param ... Format arguments
 */
void ve_logger_log(ve_log_level level, const char* file, int line, const char* func, const char* fmt, ...);

/**
 * @brief Core logging function with va_list
 *
 * @param level Log level
 * @param file Source file path
 * @param line Source line number
 * @param func Function name
 * @param fmt Format string
 * @param args Format arguments
 */
void ve_logger_log_va(ve_log_level level, const char* file, int line, const char* func, const char* fmt, va_list args);

/**
 * @brief Flush any buffered log output
 */
void ve_logger_flush(void);

/* Convenience macros with file/line/function info */
#define VE_LOG_HELPER(level, ...) \
    do { \
        if (ve_logger_should_log(level)) { \
            ve_logger_log(level, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    } while (0)

#define VE_LOG_TRACE(...) VE_LOG_HELPER(VE_LOG_TRACE, __VA_ARGS__)
#define VE_LOG_DEBUG(...) VE_LOG_HELPER(VE_LOG_DEBUG, __VA_ARGS__)
#define VE_LOG_INFO(...)  VE_LOG_HELPER(VE_LOG_INFO, __VA_ARGS__)
#define VE_LOG_WARN(...)  VE_LOG_HELPER(VE_LOG_WARN, __VA_ARGS__)
#define VE_LOG_ERROR(...) VE_LOG_HELPER(VE_LOG_ERROR, __VA_ARGS__)
#define VE_LOG_FATAL(...) VE_LOG_HELPER(VE_LOG_FATAL, __VA_ARGS__)

/* Conditional logging (only if level is enabled) */
#define VE_LOG_TRACE_IF(cond, ...) do { if ((cond) && ve_logger_should_log(VE_LOG_TRACE)) VE_LOG_TRACE(__VA_ARGS__); } while (0)
#define VE_LOG_DEBUG_IF(cond, ...) do { if ((cond) && ve_logger_should_log(VE_LOG_DEBUG)) VE_LOG_DEBUG(__VA_ARGS__); } while (0)
#define VE_LOG_INFO_IF(cond, ...)  do { if ((cond) && ve_logger_should_log(VE_LOG_INFO))  VE_LOG_INFO(__VA_ARGS__); } while (0)
#define VE_LOG_WARN_IF(cond, ...)  do { if ((cond) && ve_logger_should_log(VE_LOG_WARN))  VE_LOG_WARN(__VA_ARGS__); } while (0)
#define VE_LOG_ERROR_IF(cond, ...) do { if ((cond) && ve_logger_should_log(VE_LOG_ERROR)) VE_LOG_ERROR(__VA_ARGS__); } while (0)

/* Log with error code */
#define VE_LOG_ERRNO(level, errno_val, ...) \
    do { \
        VE_LOG_HELPER(level, __VA_ARGS__); \
        VE_LOG_HELPER(level, "Error code: %d (%s)", errno_val, ve_strerror(errno_val)); \
    } while (0)

/* Platform-specific error string */
const char* ve_strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif /* VE_LOGGER_H */
