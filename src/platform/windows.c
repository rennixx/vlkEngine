/**
 * @file windows.c
 * @brief Windows-specific platform code
 */

#include "platform.h"

#if defined(VE_PLATFORM_WINDOWS)

#include "../core/logger.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dbghelp.h>
#include <psapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

/* Windows error handling */
char* ve_win32_get_error_string(DWORD error_code) {
    char* message = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&message,
        0,
        NULL
    );

    return message;
}

/* Stack trace for debugging */
void ve_win32_print_stack_trace(void) {
    unsigned int i;
    void* stack[64];
    unsigned short frames;
    SYMBOL_INFO* symbol;
    HANDLE process;

    process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    frames = CaptureStackBackTrace(0, 64, stack, NULL);
    symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    VE_LOG_ERROR("=== Stack Trace ===");
    for (i = 0; i < frames; i++) {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        VE_LOG_ERROR("%u: %s - 0x%0X", frames - i - 1, symbol->Name, symbol->Address);
    }

    free(symbol);
    SymCleanup(process);
}

/* Console handling */
bool ve_win32_console_init(void) {
    if (!AllocConsole()) {
        return false;
    }

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    SetConsoleOutputCP(CP_UTF8);

    return true;
}

void ve_win32_console_shutdown(void) {
    FreeConsole();
}

/* High-DPI awareness */
void ve_win32_set_dpi_aware(void) {
    typedef enum {
        PROCESS_DPI_UNAWARE = 0,
        PROCESS_SYSTEM_DPI_AWARE = 1,
        PROCESS_PER_MONITOR_DPI_AWARE = 2
    } PROCESS_DPI_AWARENESS;

    typedef BOOL (WINAPI *SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
    typedef BOOL (WINAPI *SetProcessDPIAwareFunc)(void);

    HMODULE shcore = LoadLibraryA("shcore.dll");
    if (shcore) {
        SetProcessDpiAwarenessFunc set_process_dpi_awareness =
            (SetProcessDpiAwarenessFunc)GetProcAddress(shcore, "SetProcessDpiAwareness");

        if (set_process_dpi_awareness) {
            set_process_dpi_awareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shcore);
    } else {
        HMODULE user32 = LoadLibraryA("user32.dll");
        if (user32) {
            SetProcessDPIAwareFunc set_process_dpi_aware =
                (SetProcessDPIAwareFunc)GetProcAddress(user32, "SetProcessDPIAware");

            if (set_process_dpi_aware) {
                set_process_dpi_aware();
            }

            FreeLibrary(user32);
        }
    }
}

/* Memory status */
void ve_win32_get_memory_status(size_t* total, size_t* available, size_t* used) {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);

    if (total) {
        *total = (size_t)status.ullTotalPhys;
    }
    if (available) {
        *available = (size_t)status.ullAvailPhys;
    }
    if (used) {
        *used = (size_t)(status.ullTotalPhys - status.ullAvailPhys);
    }
}

/* System information */
void ve_win32_get_system_info(SYSTEM_INFO* info) {
    if (info) {
        GetSystemInfo(info);
    }
}

/* File system helpers */
bool ve_win32_get_appdata_path(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }

    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, buffer) != S_OK) {
        return false;
    }

    return true;
}

bool ve_win32_get_temp_path(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }

    DWORD len = GetTempPathA((DWORD)size, buffer);
    return len > 0 && len < size;
}

/* Registry helpers */
bool ve_win32_registry_get_string(HKEY root, const char* path, const char* key,
                                  char* buffer, size_t size) {
    HKEY hKey;
    DWORD dwType = REG_SZ;
    DWORD dwSize = (DWORD)size;

    if (RegOpenKeyExA(root, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    bool result = (RegQueryValueExA(hKey, key, NULL, &dwType, (LPBYTE)buffer, &dwSize) == ERROR_SUCCESS);
    RegCloseKey(hKey);

    return result;
}

/* Performance counter helpers */
double ve_win32_get_timer_frequency(void) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (double)frequency.QuadPart;
}

uint64_t ve_win32_get_timer_counter(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

/* Module helpers */
HMODULE ve_win32_get_module_handle(const char* name) {
    return GetModuleHandleA(name);
}

void* ve_win32_get_proc_address(HMODULE module, const char* name) {
    return (void*)GetProcAddress(module, name);
}

/* Crash handler */
LONG WINAPI ve_win32_exception_handler(EXCEPTION_POINTERS* exception_info) {
    VE_LOG_FATAL("=== Unhandled Exception ===");
    VE_LOG_FATAL("Exception Code: 0x%08X", exception_info->ExceptionRecord->ExceptionCode);
    VE_LOG_FATAL("Exception Address: 0x%p", exception_info->ExceptionRecord->ExceptionAddress);

    ve_win32_print_stack_trace();

    return EXCEPTION_EXECUTE_HANDLER;
}

void ve_win32_set_exception_handler(void) {
    SetUnhandledExceptionFilter(ve_win32_exception_handler);
}

/* Watchdog for detecting hangs */
typedef struct ve_win32_watchdog {
    HANDLE thread;
    volatile bool running;
    DWORD timeout_ms;
    HANDLE watchdog_event;
} ve_win32_watchdog;

static DWORD WINAPI watchdog_thread_proc(LPVOID param) {
    ve_win32_watchdog* watchdog = (ve_win32_watchdog*)param;

    while (watchdog->running) {
        if (WaitForSingleObject(watchdog->watchdog_event, watchdog->timeout_ms) == WAIT_TIMEOUT) {
            VE_LOG_ERROR("Watchdog: Application appears to be hung");
            ve_win32_print_stack_trace();
        }
    }

    return 0;
}

ve_win32_watchdog* ve_win32_watchdog_create(DWORD timeout_ms) {
    ve_win32_watchdog* watchdog = (ve_win32_watchdog*)malloc(sizeof(ve_win32_watchdog));
    if (!watchdog) {
        return NULL;
    }

    watchdog->running = true;
    watchdog->timeout_ms = timeout_ms;
    watchdog->watchdog_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    watchdog->thread = CreateThread(NULL, 0, watchdog_thread_proc, watchdog, 0, NULL);
    if (!watchdog->thread) {
        CloseHandle(watchdog->watchdog_event);
        free(watchdog);
        return NULL;
    }

    return watchdog;
}

void ve_win32_watchdog_destroy(ve_win32_watchdog* watchdog) {
    if (!watchdog) {
        return;
    }

    watchdog->running = false;
    SetEvent(watchdog->watchdog_event);

    WaitForSingleObject(watchdog->thread, INFINITE);
    CloseHandle(watchdog->thread);
    CloseHandle(watchdog->watchdog_event);

    free(watchdog);
}

void ve_win32_watchdog_kick(ve_win32_watchdog* watchdog) {
    if (watchdog) {
        SetEvent(watchdog->watchdog_event);
    }
}

#endif /* VE_PLATFORM_WINDOWS */
