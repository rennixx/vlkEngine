/**
 * @file linux.c
 * @brief Linux-specific platform code
 */

#include "platform.h"

#if defined(VE_PLATFORM_LINUX)

#include "../core/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <execinfo.h>

/* Stack trace for debugging */
void ve_linux_print_stack_trace(void) {
    void* array[64];
    size_t size;

    size = backtrace(array, sizeof(array) / sizeof(array[0]));
    char** strings = backtrace_symbols(array, size);

    VE_LOG_ERROR("=== Stack Trace ===");
    if (strings) {
        for (size_t i = 0; i < size; i++) {
            VE_LOG_ERROR("%zu: %s", size - i - 1, strings[i]);
        }
        free(strings);
    }
}

/* Signal handler for crashes */
static void crash_handler(int sig, siginfo_t* info, void* context) {
    VE_LOG_FATAL("=== Crash Signal Received ===");
    VE_LOG_FATAL("Signal: %d (%s)", sig, strsignal(sig));

    if (info) {
        switch (sig) {
            case SIGSEGV:
                VE_LOG_FATAL("Segmentation fault at address: %p", info->si_addr);
                break;
            case SIGFPE:
                VE_LOG_FATAL("Floating point exception");
                break;
            case SIGILL:
                VE_LOG_FATAL("Illegal instruction");
                break;
            case SIGABRT:
                VE_LOG_FATAL("Abort called");
                break;
            default:
                break;
        }
    }

    ve_linux_print_stack_trace();
    ve_logger_flush();

    /* Reset signal handler and re-raise */
    signal(sig, SIG_DFL);
    raise(sig);
}

void ve_linux_set_exception_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

/* Memory allocation helpers */
void* ve_linux_allocate_aligned(size_t size, size_t alignment) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

void ve_linux_free_aligned(void* ptr) {
    free(ptr);
}

/* Get home directory */
bool ve_linux_get_home_path(char* buffer, size_t size) {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (!pw) {
            return false;
        }
        home = pw->pw_dir;
    }

    if (strlen(home) >= size) {
        return false;
    }

    strcpy(buffer, home);
    return true;
}

/* Get config directory (XDG_CONFIG_HOME) */
bool ve_linux_get_config_path(char* buffer, size_t size) {
    const char* config = getenv("XDG_CONFIG_HOME");
    if (config) {
        if (strlen(config) >= size) {
            return false;
        }
        strcpy(buffer, config);
        return true;
    }

    /* Fall back to ~/.config */
    char home[PATH_MAX];
    if (!ve_linux_get_home_path(home, sizeof(home))) {
        return false;
    }

    snprintf(buffer, size, "%s/.config", home);
    return true;
}

/* Get cache directory (XDG_CACHE_HOME) */
bool ve_linux_get_cache_path(char* buffer, size_t size) {
    const char* cache = getenv("XDG_CACHE_HOME");
    if (cache) {
        if (strlen(cache) >= size) {
            return false;
        }
        strcpy(buffer, cache);
        return true;
    }

    /* Fall back to ~/.cache */
    char home[PATH_MAX];
    if (!ve_linux_get_home_path(home, sizeof(home))) {
        return false;
    }

    snprintf(buffer, size, "%s/.cache", home);
    return true;
}

/* Get temp directory */
bool ve_linux_get_temp_path(char* buffer, size_t size) {
    const char* temp = getenv("TMPDIR");
    if (!temp) {
        temp = "/tmp";
    }

    if (strlen(temp) >= size) {
        return false;
    }

    strcpy(buffer, temp);
    return true;
}

/* Get executable path */
bool ve_linux_get_executable_path(char* buffer, size_t size) {
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len < 0) {
        return false;
    }
    buffer[len] = '\0';
    return true;
}

/* Create directory recursively */
bool ve_linux_create_directory(const char* path) {
    char temp[PATH_MAX];
    char* p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);

    if (temp[len - 1] == '/') {
        temp[len - 1] = 0;
    }

    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }

    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}

/* Dynamic library helpers */
void* ve_linux_load_library(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* ve_linux_get_symbol(void* handle, const char* name) {
    return dlsym(handle, name);
}

void ve_linux_unload_library(void* handle) {
    if (handle) {
        dlclose(handle);
    }
}

const char* ve_linux_get_library_error(void) {
    return dlerror();
}

/* Affinity helpers */
bool ve_linux_set_thread_affinity(pthread_t thread, int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) == 0;
}

bool ve_linux_get_thread_affinity(pthread_t thread, int* core) {
    cpu_set_t cpuset;
    if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return false;
    }

    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            if (core) {
                *core = i;
            }
            return true;
        }
    }

    return false;
}

/* Performance counter helpers */
uint64_t ve_linux_get_rdtsc(void) {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

/* High-resolution timer */
uint64_t ve_linux_get_clock_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* CPU usage */
typedef struct ve_linux_cpu_stats {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
    uint64_t guest;
} ve_linux_cpu_stats;

bool ve_linux_get_cpu_stats(ve_linux_cpu_stats* stats) {
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) {
        return false;
    }

    if (fscanf(fp, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                &stats->user, &stats->nice, &stats->system, &stats->idle,
                &stats->iowait, &stats->irq, &stats->softirq,
                &stats->steal, &stats->guest) != 9) {
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

double ve_linux_get_cpu_usage(void) {
    static ve_linux_cpu_stats prev_stats = {0};
    ve_linux_cpu_stats curr_stats;

    if (!ve_linux_get_cpu_stats(&curr_stats)) {
        return 0.0;
    }

    uint64_t prev_total = prev_stats.user + prev_stats.nice + prev_stats.system +
                         prev_stats.idle + prev_stats.iowait + prev_stats.irq +
                         prev_stats.softirq + prev_stats.steal;
    uint64_t curr_total = curr_stats.user + curr_stats.nice + curr_stats.system +
                         curr_stats.idle + curr_stats.iowait + curr_stats.irq +
                         curr_stats.softirq + curr_stats.steal;

    uint64_t prev_idle = prev_stats.idle + prev_stats.iowait;
    uint64_t curr_idle = curr_stats.idle + curr_stats.iowait;

    uint64_t total_diff = curr_total - prev_total;
    uint64_t idle_diff = curr_idle - prev_idle;

    prev_stats = curr_stats;

    if (total_diff == 0) {
        return 0.0;
    }

    return 100.0 * (1.0 - (double)idle_diff / total_diff);
}

/* Memory usage */
bool ve_linux_get_memory_usage(size_t* total, size_t* available, size_t* used, size_t* buffers, size_t* cached) {
    FILE* fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return false;
    }

    unsigned long mem_total = 0, mem_free = 0, mem_available = 0;
    unsigned long buffers_val = 0, cached_val = 0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long value;
        if (sscanf(line, "MemTotal: %lu kB", &value) == 1) {
            mem_total = value;
        } else if (sscanf(line, "MemFree: %lu kB", &value) == 1) {
            mem_free = value;
        } else if (sscanf(line, "MemAvailable: %lu kB", &value) == 1) {
            mem_available = value;
        } else if (sscanf(line, "Buffers: %lu kB", &value) == 1) {
            buffers_val = value;
        } else if (sscanf(line, "Cached: %lu kB", &value) == 1) {
            cached_val = value;
        }
    }

    fclose(fp);

    if (total) *total = mem_total * 1024;
    if (available) *available = mem_available * 1024;
    if (used) *used = (mem_total - mem_free) * 1024;
    if (buffers) *buffers = buffers_val * 1024;
    if (cached) *cached = cached_val * 1024;

    return true;
}

/* Process info */
bool ve_linux_get_process_info(size_t* vm_size, size_t* vm_rss, size_t* vm_shared) {
    FILE* fp = fopen("/proc/self/status", "r");
    if (!fp) {
        return false;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long value;
        if (sscanf(line, "VmSize: %lu kB", &value) == 1) {
            if (vm_size) *vm_size = value * 1024;
        } else if (sscanf(line, "VmRSS: %lu kB", &value) == 1) {
            if (vm_rss) *vm_rss = value * 1024;
        } else if (sscanf(line, "VmShared: %lu kB", &value) == 1) {
            if (vm_shared) *vm_shared = value * 1024;
        }
    }

    fclose(fp);
    return true;
}

/* Set thread name */
bool ve_linux_set_thread_name(pthread_t thread, const char* name) {
#ifdef __linux__
    return pthread_setname_np(thread, name) == 0;
#else
    (void)thread;
    (void)name;
    return false;
#endif
}

bool ve_linux_get_thread_name(pthread_t thread, char* buffer, size_t size) {
#ifdef __linux__
    return pthread_getname_np(thread, buffer, size) == 0;
#else
    (void)thread;
    (void)buffer;
    (void)size;
    return false;
#endif
}

#endif /* VE_PLATFORM_LINUX */
