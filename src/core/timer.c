/**
 * @file timer.c
 * @brief High-resolution timing utilities implementation
 */

#include "timer.h"
#include "logger.h"
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
    #define VE_PLATFORM_WINDOWS
    #include <windows.h>
#elif defined(__linux__)
    #define VE_PLATFORM_LINUX
    #include <time.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #define VE_PLATFORM_MACOS
    #include <mach/mach_time.h>
    #include <unistd.h>
#endif

static double g_timer_frequency = 0.0;
static double g_target_fps = 60.0;
static double g_fixed_timestep = 1.0 / 60.0;

bool ve_timer_init(void) {
#if defined(VE_PLATFORM_WINDOWS)
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        VE_LOG_ERROR("QueryPerformanceFrequency failed");
        return false;
    }
    g_timer_frequency = (double)frequency.QuadPart;
#elif defined(VE_PLATFORM_MACOS)
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    g_timer_frequency = (1e9 / (info.numer / (double)info.denom));
#else
    g_timer_frequency = 1e9;  /* nanoseconds */
#endif

    VE_LOG_INFO("Timer initialized, resolution: %.9f seconds", ve_timer_get_resolution());
    return true;
}

void ve_timer_shutdown(void) {
    VE_LOG_INFO("Timer shutdown");
}

ve_timestamp ve_timer_now(void) {
#if defined(VE_PLATFORM_WINDOWS)
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (ve_timestamp)counter.QuadPart;
#elif defined(VE_PLATFORM_MACOS)
    return (ve_timestamp)mach_absolute_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ve_timestamp)(ts.tv_sec * 1000000000ULL + ts.tv_nsec);
#endif
}

double ve_timer_to_seconds(ve_timestamp timestamp) {
#if defined(VE_PLATFORM_WINDOWS) || defined(VE_PLATFORM_MACOS)
    return (double)timestamp / g_timer_frequency;
#else
    return timestamp / 1e9;
#endif
}

double ve_timer_to_milliseconds(ve_timestamp timestamp) {
    return ve_timer_to_seconds(timestamp) * 1000.0;
}

double ve_timer_elapsed(ve_timestamp start, ve_timestamp end) {
    return ve_timer_to_seconds(end - start);
}

ve_timer ve_timer_start(void) {
    ve_timer timer = {0};
    timer.start_time = ve_timer_now();
    timer.running = true;
    return timer;
}

double ve_timer_stop(ve_timer* timer) {
    if (!timer || !timer->running) {
        return 0.0;
    }

    timer->end_time = ve_timer_now();
    timer->running = false;

    return ve_timer_elapsed(timer->start_time, timer->end_time);
}

double ve_timer_restart(ve_timer* timer) {
    if (!timer) {
        return 0.0;
    }

    double elapsed = timer->running ? ve_timer_get_elapsed(timer) : 0.0;
    timer->start_time = ve_timer_now();
    timer->running = true;
    return elapsed;
}

double ve_timer_get_elapsed(const ve_timer* timer) {
    if (!timer) {
        return 0.0;
    }

    if (timer->running) {
        return ve_timer_elapsed(timer->start_time, ve_timer_now());
    }

    return ve_timer_elapsed(timer->start_time, timer->end_time);
}

bool ve_timer_is_running(const ve_timer* timer) {
    return timer ? timer->running : false;
}

void ve_timer_sleep(double seconds) {
    if (seconds <= 0.0) {
        return;
    }

#if defined(VE_PLATFORM_WINDOWS)
    DWORD milliseconds = (DWORD)(seconds * 1000.0);
    Sleep(milliseconds);
#elif defined(VE_PLATFORM_MACOS)
    usleep((useconds_t)(seconds * 1000000.0));
#else
    usleep((useconds_t)(seconds * 1000000.0));
#endif
}

double ve_timer_get_resolution(void) {
#if defined(VE_PLATFORM_WINDOWS)
    return 1.0 / g_timer_frequency;
#elif defined(VE_PLATFORM_MACOS)
    return 1.0 / g_timer_frequency;
#else
    return 1.0 / g_timer_frequency;
#endif
}

/* Frame timing implementation */

struct ve_frame_time_state {
    ve_timestamp last_frame_time;
    double accumulator;
    double max_frame_time;
};

static struct ve_frame_time_state g_frame_state = {0};

void ve_frame_time_init(ve_frame_time* ft) {
    if (!ft) {
        return;
    }

    ft->delta_time = 0.0;
    ft->total_time = 0.0;
    ft->frame_rate = 0.0;
    ft->frame_count = 0;

    g_frame_state.last_frame_time = ve_timer_now();
    g_frame_state.accumulator = 0.0;
    g_frame_state.max_frame_time = 0.25;  /* Cap at 250ms to prevent spiral of death */
}

void ve_frame_time_update(ve_frame_time* ft) {
    if (!ft) {
        return;
    }

    ve_timestamp current_time = ve_timer_now();
    ft->delta_time = ve_timer_elapsed(g_frame_state.last_frame_time, current_time);
    g_frame_state.last_frame_time = current_time;

    /* Cap delta time to prevent spiral of death */
    if (ft->delta_time > g_frame_state.max_frame_time) {
        ft->delta_time = g_frame_state.max_frame_time;
    }

    ft->total_time += ft->delta_time;
    ft->frame_count++;

    /* Calculate frame rate */
    if (ft->delta_time > 0.0) {
        ft->frame_rate = 1.0 / ft->delta_time;
    } else {
        ft->frame_rate = 0.0;
    }

    /* Accumulate for fixed timestep */
    g_frame_state.accumulator += ft->delta_time;
}

void ve_frame_time_reset(ve_frame_time* ft) {
    if (!ft) {
        return;
    }

    ft->delta_time = 0.0;
    ft->total_time = 0.0;
    ft->frame_rate = 0.0;
    ft->frame_count = 0;

    g_frame_state.last_frame_time = ve_timer_now();
    g_frame_state.accumulator = 0.0;
}

double ve_frame_time_get_target_fps(void) {
    return g_target_fps;
}

void ve_frame_time_set_target_fps(double fps) {
    if (fps > 0.0) {
        g_target_fps = fps;
        g_fixed_timestep = 1.0 / fps;
    }
}

double ve_frame_time_get_fixed_timestep(void) {
    return g_fixed_timestep;
}

bool ve_frame_time_should_update(const ve_frame_time* ft) {
    (void)ft;  /* Unused */
    return g_frame_state.accumulator >= g_fixed_timestep;
}

void ve_frame_time_consume_update(ve_frame_time* ft) {
    (void)ft;  /* Unused */
    g_frame_state.accumulator -= g_fixed_timestep;
}

double ve_frame_time_get_alpha(const ve_frame_time* ft) {
    (void)ft;  /* Unused */
    return g_frame_state.accumulator / g_fixed_timestep;
}
