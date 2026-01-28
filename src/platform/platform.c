/**
 * @file platform.c
 * @brief Platform abstraction implementation
 */

#include "platform.h"
#include "../core/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef VE_PLATFORM_WINDOWS
    #include <psapi.h>
    #include <shellapi.h>
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/sysinfo.h>
    #include <sys/statvfs.h>
    #include <unistd.h>
    #include <limits.h>
#endif

/* CPU features state */
static ve_cpu_features g_cpu_features = {0};
static bool g_cpu_features_initialized = false;

/* CPU feature detection implementation */
static void detect_cpu_features(void) {
    memset(&g_cpu_features, 0, sizeof(ve_cpu_features));

#ifdef VE_PLATFORM_WINDOWS
    /* Windows implementation using __cpuid */
    int regs[4];

    /* Get max leaf */
    __cpuid(regs, 0);
    int max_leaf = regs[0];

    /* Vendor ID */
    __cpuid(regs, 0);

    /* Feature flags */
    if (max_leaf >= 1) {
        __cpuid(regs, 1);
        g_cpu_features.sse3 = (regs[2] & (1 << 0)) != 0;
        g_cpu_features.ssse3 = (regs[2] & (1 << 9)) != 0;
        g_cpu_features.fma = (regs[2] & (1 << 12)) != 0;
        g_cpu_features.sse4_1 = (regs[2] & (1 << 19)) != 0;
        g_cpu_features.sse4_2 = (regs[2] & (1 << 20)) != 0;
        g_cpu_features.popcnt = (regs[2] & (1 << 23)) != 0;
        g_cpu_features.aes_ni = (regs[2] & (1 << 25)) != 0;
        g_cpu_features.rdtsc = (regs[3] & (1 << 4)) != 0;

        g_cpu_features.sse = (regs[3] & (1 << 25)) != 0;
        g_cpu_features.sse2 = (regs[3] & (1 << 26)) != 0;
    }

    /* Extended features */
    if (max_leaf >= 7) {
        __cpuid(regs, 7);
        g_cpu_features.sgx = (regs[1] & (1 << 2)) != 0;
        g_cpu_features.bmi1 = (regs[1] & (1 << 3)) != 0;
        g_cpu_features.hle = (regs[1] & (1 << 4)) != 0;
        g_cpu_features.avx2 = (regs[1] & (1 << 5)) != 0;
        g_cpu_features.bmi2 = (regs[1] & (1 << 8)) != 0;
        g_cpu_features.erms = (regs[1] & (1 << 9)) != 0;
        g_cpu_features.invpcid = (regs[1] & (1 << 10)) != 0;
        g_cpu_features.rtm = (regs[1] & (1 << 11)) != 0;
        g_cpu_features.avx512f = (regs[1] & (1 << 16)) != 0;
    }

    /* Check hypervisor */
    __cpuid(regs, 1);
    g_cpu_features.hypervisor = (regs[2] & (1 << 31)) != 0;

#elif defined(VE_PLATFORM_LINUX) && (defined(VE_ARCH_X86) || defined(VE_ARCH_X64))
    /* Linux x86/x64 implementation using CPUID */
    uint32_t eax, ebx, ecx, edx;

    /* Get max leaf */
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;

    /* Feature flags */
    if (max_leaf >= 1) {
        __get_cpuid(1, &eax, &ebx, &ecx, &edx);
        g_cpu_features.sse3 = (ecx & (1 << 0)) != 0;
        g_cpu_features.ssse3 = (ecx & (1 << 9)) != 0;
        g_cpu_features.fma = (ecx & (1 << 12)) != 0;
        g_cpu_features.sse4_1 = (ecx & (1 << 19)) != 0;
        g_cpu_features.sse4_2 = (ecx & (1 << 20)) != 0;
        g_cpu_features.popcnt = (ecx & (1 << 23)) != 0;
        g_cpu_features.aes_ni = (ecx & (1 << 25)) != 0;
        g_cpu_features.rdtsc = (edx & (1 << 4)) != 0;
        g_cpu_features.sse = (edx & (1 << 25)) != 0;
        g_cpu_features.sse2 = (edx & (1 << 26)) != 0;
    }

    if (max_leaf >= 7) {
        __get_cpuid(7, &eax, &ebx, &ecx, &edx);
        g_cpu_features.bmi1 = (ebx & (1 << 3)) != 0;
        g_cpu_features.avx2 = (ebx & (1 << 5)) != 0;
        g_cpu_features.bmi2 = (ebx & (1 << 8)) != 0;
        g_cpu_features.avx512f = (ebx & (1 << 16)) != 0;
    }
#endif

    /* Get CPU count */
    g_cpu_features.cpu_count = ve_get_cpu_count();

    /* Cache line size is typically 64 bytes on modern CPUs */
    g_cpu_features.cache_line_size = 64;

    g_cpu_features_initialized = true;
}

const ve_cpu_features* ve_get_cpu_features(void) {
    if (!g_cpu_features_initialized) {
        detect_cpu_features();
    }
    return &g_cpu_features;
}

bool ve_platform_init(void) {
    detect_cpu_features();

    VE_LOG_INFO("Platform: %s",
#ifdef VE_PLATFORM_WINDOWS
        "Windows"
#elif defined(VE_PLATFORM_LINUX)
        "Linux"
#elif defined(VE_PLATFORM_MACOS)
        "macOS"
#else
        "Unknown"
#endif
    );

    VE_LOG_INFO("Architecture: %s",
#ifdef VE_ARCH_X64
        "x64"
#elif defined(VE_ARCH_X86)
        "x86"
#elif defined(VE_ARCH_ARM64)
        "ARM64"
#elif defined(VE_ARCH_ARM)
        "ARM"
#else
        "Unknown"
#endif
    );

    VE_LOG_INFO("Compiler: %s",
#ifdef VE_COMPILER_MSVC
        "MSVC"
#elif defined(VE_COMPILER_CLANG)
        "Clang"
#elif defined(VE_COMPILER_GCC)
        "GCC"
#else
        "Unknown"
#endif
    );

    VE_LOG_INFO("CPU Count: %d", g_cpu_features.cpu_count);
    VE_LOG_INFO("Total Memory: %zu MB", ve_get_total_memory() / (1024 * 1024));

    return true;
}

void ve_platform_shutdown(void) {
    VE_LOG_INFO("Platform shutdown");
}

size_t ve_get_page_size(void) {
#ifdef VE_PLATFORM_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwPageSize;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}

int ve_get_cpu_count(void) {
#ifdef VE_PLATFORM_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

size_t ve_get_total_memory(void) {
#ifdef VE_PLATFORM_WINDOWS
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t)status.ullTotalPhys;
#else
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return 0;
    }
    return (size_t)info.totalram * info.mem_unit;
#endif
}

size_t ve_get_available_memory(void) {
#ifdef VE_PLATFORM_WINDOWS
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t)status.ullAvailPhys;
#else
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return 0;
    }
    return (size_t)info.freeram * info.mem_unit;
#endif
}

bool ve_get_executable_path(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }

#ifdef VE_PLATFORM_WINDOWS
    if (GetModuleFileNameA(NULL, buffer, (DWORD)size) == 0) {
        return false;
    }
#else
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len < 0) {
        return false;
    }
    buffer[len] = '\0';
#endif

    return true;
}

bool ve_get_current_directory(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }

#ifdef VE_PLATFORM_WINDOWS
    return GetCurrentDirectoryA((DWORD)size, buffer) != 0;
#else
    return getcwd(buffer, size) != NULL;
#endif
}

bool ve_set_current_directory(const char* path) {
    if (!path) {
        return false;
    }

#ifdef VE_PLATFORM_WINDOWS
    return SetCurrentDirectoryA(path) != 0;
#else
    return chdir(path) == 0;
#endif
}

bool ve_get_env_var(const char* name, char* buffer, size_t size) {
    if (!name || !buffer || size == 0) {
        return false;
    }

#ifdef VE_PLATFORM_WINDOWS
    DWORD len = GetEnvironmentVariableA(name, buffer, (DWORD)size);
    return len > 0 && len < size;
#else
    const char* value = getenv(name);
    if (!value) {
        return false;
    }
    strncpy(buffer, value, size - 1);
    buffer[size - 1] = '\0';
    return true;
#endif
}

bool ve_set_env_var(const char* name, const char* value) {
    if (!name) {
        return false;
    }

#ifdef VE_PLATFORM_WINDOWS
    return SetEnvironmentVariableA(name, value ? value : "") != 0;
#else
    if (value) {
        return setenv(name, value, 1) == 0;
    } else {
        return unsetenv(name) == 0;
    }
#endif
}

char* ve_get_clipboard_text(void) {
#ifdef VE_PLATFORM_WINDOWS
    if (!IsClipboardFormatAvailable(CF_TEXT)) {
        return NULL;
    }

    if (!OpenClipboard(NULL)) {
        return NULL;
    }

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) {
        CloseClipboard();
        return NULL;
    }

    char* text = (char*)GlobalLock(hData);
    if (!text) {
        CloseClipboard();
        return NULL;
    }

    size_t len = strlen(text) + 1;
    char* result = (char*)malloc(len);
    if (result) {
        memcpy(result, text, len);
    }

    GlobalUnlock(hData);
    CloseClipboard();

    return result;
#else
    /* Linux clipboard support requires X11 */
    VE_LOG_WARN("Clipboard not implemented on this platform");
    return NULL;
#endif
}

bool ve_set_clipboard_text(const char* text) {
    if (!text) {
        return false;
    }

#ifdef VE_PLATFORM_WINDOWS
    size_t len = strlen(text) + 1;
    HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hData) {
        return false;
    }

    char* data = (char*)GlobalLock(hData);
    if (!data) {
        GlobalFree(hData);
        return false;
    }

    memcpy(data, text, len);
    GlobalUnlock(hData);

    if (!OpenClipboard(NULL)) {
        GlobalFree(hData);
        return false;
    }

    EmptyClipboard();
    SetClipboardData(CF_TEXT, hData);
    CloseClipboard();

    return true;
#else
    VE_LOG_WARN("Clipboard not implemented on this platform");
    return false;
#endif
}
