/**
 * @file timer.h
 * @brief High-resolution timing utilities
 */

#ifndef VE_TIMER_H
#define VE_TIMER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief High-resolution timestamp
 */
typedef uint64_t ve_timestamp;

/**
 * @brief Timer state
 */
typedef struct ve_timer {
    ve_timestamp start_time;
    ve_timestamp end_time;
    bool running;
} ve_timer;

/**
 * @brief Frame timing data
 */
typedef struct ve_frame_time {
    double delta_time;       /* Time since last frame in seconds */
    double total_time;       /* Total time since start in seconds */
    double frame_rate;       /* Current FPS */
    uint32_t frame_count;    /* Total frames since start */
} ve_frame_time;

/**
 * @brief Initialize the timer system
 *
 * @return true on success, false on failure
 */
bool ve_timer_init(void);

/**
 * @brief Shutdown the timer system
 */
void ve_timer_shutdown(void);

/**
 * @brief Get the current high-resolution timestamp
 *
 * @return Current timestamp
 */
ve_timestamp ve_timer_now(void);

/**
 * @brief Convert timestamp to seconds
 *
 * @param timestamp Timestamp to convert
 * @return Time in seconds
 */
double ve_timer_to_seconds(ve_timestamp timestamp);

/**
 * @brief Convert timestamp to milliseconds
 *
 * @param timestamp Timestamp to convert
 * @return Time in milliseconds
 */
double ve_timer_to_milliseconds(ve_timestamp timestamp);

/**
 * @brief Calculate the difference between two timestamps in seconds
 *
 * @param start Start timestamp
 * @param end End timestamp
 * @return Duration in seconds
 */
double ve_timer_elapsed(ve_timestamp start, ve_timestamp end);

/**
 * @brief Create and start a new timer
 *
 * @return New timer
 */
ve_timer ve_timer_start(void);

/**
 * @brief Stop a timer
 *
 * @param timer Timer to stop
 * @return Elapsed time in seconds
 */
double ve_timer_stop(ve_timer* timer);

/**
 * @brief Restart a timer
 *
 * @param timer Timer to restart
 * @return Elapsed time before restart in seconds
 */
double ve_timer_restart(ve_timer* timer);

/**
 * @brief Get the elapsed time of a running timer
 *
 * @param timer Timer to query
 * @return Elapsed time in seconds
 */
double ve_timer_get_elapsed(const ve_timer* timer);

/**
 * @brief Check if a timer is still running
 *
 * @param timer Timer to check
 * @return true if running, false if stopped
 */
bool ve_timer_is_running(const ve_timer* timer);

/**
 * @brief Sleep for a specified duration
 *
 * @param seconds Duration to sleep in seconds
 */
void ve_timer_sleep(double seconds);

/**
 * @brief Get the timer resolution
 *
 * @return Timer resolution in seconds
 */
double ve_timer_get_resolution(void);

/* Frame timing */

/**
 * @brief Initialize frame timing
 *
 * @param frame_time Frame time structure to initialize
 */
void ve_frame_time_init(ve_frame_time* ft);

/**
 * @brief Update frame timing (call once per frame)
 *
 * @param frame_time Frame time structure to update
 */
void ve_frame_time_update(ve_frame_time* ft);

/**
 * @brief Reset frame timing
 *
 * @param frame_time Frame time structure to reset
 */
void ve_frame_time_reset(ve_frame_time* ft);

/**
 * @brief Get target FPS for fixed timestep
 *
 * @return Target FPS (default 60)
 */
double ve_frame_time_get_target_fps(void);

/**
 * @brief Set target FPS for fixed timestep
 *
 * @param fps Target FPS
 */
void ve_frame_time_set_target_fps(double fps);

/**
 * @brief Get the fixed timestep duration
 *
 * @return Timestep in seconds
 */
double ve_frame_time_get_fixed_timestep(void);

/**
 * @brief Check if it's time for a fixed update
 *
 * @param frame_time Frame time structure
 * @return true if fixed update should run
 */
bool ve_frame_time_should_update(const ve_frame_time* ft);

/**
 * @brief Consume fixed timestep accumulator
 *
 * @param frame_time Frame time structure
 */
void ve_frame_time_consume_update(ve_frame_time* ft);

/**
 * @brief Get the alpha value for interpolation
 *
 * @param frame_time Frame time structure
 * @return Alpha value [0, 1]
 */
double ve_frame_time_get_alpha(const ve_frame_time* ft);

/* Utility macros */

/**
 * @brief Scope-based timing
 *
 * Usage:
 * @code
 * VE_TIMED_SCOPE("MyFunction") {
 *     // Code to time
 * }
 * @endcode
 */
#define VE_TIMED_SCOPE(name) \
    ve_timer VE_CAT(_timer_, __LINE__) = ve_timer_start(); \
    if (0)

/**
 * @brief Function timing
 *
 * Usage:
 * @code
 * void my_function(void) {
 *     VE_TIMED_FUNCTION();
 *     // Function code
 * }
 * @endcode
 */
#define VE_TIMED_FUNCTION() \
    ve_timer VE_CAT(_timer_, __LINE__) = ve_timer_start(); \
    if (0)

/**
 * @brief Measure elapsed time at end of scope
 */
#define VE_TIME_SCOPE(name) \
    ve_timer VE_CAT(_timer_, __LINE__) = ve_timer_start(); \
    if (0)

/* Helper macros */
#define VE_CAT_IMPL(a, b) a##b
#define VE_CAT(a, b) VE_CAT_IMPL(a, b)

/**
 * @brief RAII-style timer that logs on scope exit
 */
#ifdef __GNUC__
    #define VE_SCOPED_TIMER(name) \
        __attribute__((cleanup(_ve ScopedTimer_cleanup))) \
        ve_timer VE_CAT(_scoped_timer_, __LINE__) = ve_timer_start(); \
        const char* VE_CAT(_scoped_name_, __LINE__) = name

    static inline void _ve_ScopedTimer_cleanup(ve_timer* timer) {
        double elapsed = ve_timer_get_elapsed(timer);
        VE_LOG_DEBUG("Timer: %s took %.6f seconds", "scope", elapsed);
    }
#else
    #define VE_SCOPED_TIMER(name) \
        ve_timer VE_CAT(_scoped_timer_, __LINE__) = ve_timer_start(); \
        if (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* VE_TIMER_H */
