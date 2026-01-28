/**
 * @file platform.h
 * @brief Platform detection and abstraction layer
 */

#ifndef VE_PLATFORM_H
#define VE_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection macros */
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    #define VE_PLATFORM_WINDOWS 1
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>

#elif defined(__linux__) || defined(__linux)
    #define VE_PLATFORM_LINUX 1
    #include <unistd.h>
    #include <sys/types.h>

#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if defined(TARGET_OS_MAC) && TARGET_OS_MAC
        #define VE_PLATFORM_MACOS 1
    #elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        #define VE_PLATFORM_IOS 1
    #endif

#else
    #define VE_PLATFORM_UNKNOWN 1
    #warning "Unknown platform detected"
#endif

/* Compiler detection */
#if defined(_MSC_VER)
    #define VE_COMPILER_MSVC _MSC_VER
    #define VE_COMPILER_MSVC_VERSION _MSC_VER

#elif defined(__clang__)
    #define VE_COMPILER_CLANG 1
    #define VE_COMPILER_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)

#elif defined(__GNUC__) || defined(__GNUG__)
    #define VE_COMPILER_GCC 1
    #define VE_COMPILER_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)

#else
    #define VE_COMPILER_UNKNOWN 1
    #warning "Unknown compiler detected"
#endif

/* Architecture detection */
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    #define VE_ARCH_X64 1
    #define VE_ARCH_64BIT 1
#elif defined(_M_IX86) || defined(__i386__) || defined(_X86_)
    #define VE_ARCH_X86 1
    #define VE_ARCH_32BIT 1
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
    #define VE_ARCH_ARM64 1
    #define VE_ARCH_64BIT 1
#elif defined(_M_ARM) || defined(__arm__) || defined(_ARM)
    #define VE_ARCH_ARM 1
    #define VE_ARCH_32BIT 1
#else
    #define VE_ARCH_UNKNOWN 1
    #warning "Unknown architecture detected"
#endif

/* Build configuration */
#if defined(_DEBUG) || defined(DEBUG)
    #define VE_BUILD_DEBUG 1
#else
    #define VE_BUILD_RELEASE 1
#endif

/* Platform-specific attributes */
#if defined(VE_COMPILER_GCC) || defined(VE_COMPILER_CLANG)
    #define VE_FORCEINLINE __attribute__((always_inline)) inline
    #define VE_INLINE __inline__
    #define VE_NOINLINE __attribute__((noinline))
    #define VE_NORETURN __attribute__((noreturn))
    #define VE_USED __attribute__((used))
    #define VE_UNUSED __attribute__((unused))
    #define VE_PACKED __attribute__((packed))
    #define VE_ALIGN(x) __attribute__((aligned(x)))
    #define VE_COLD __attribute__((cold))
    #define VE_HOT __attribute__((hot))
    #define VE_MALLOC __attribute__((malloc))
    #define VE_PURE __attribute__((pure))
    #define VE_CONST __attribute__((const))
    #define VE_WEAK __attribute__((weak))
    #define VE_ALIAS(x) __attribute__((alias(x)))
    #define VE_DEPRECATED __attribute__((deprecated))
    #define VE_EXPORT __attribute__((visibility("default")))
    #define VE_IMPORT __attribute__((visibility("default")))
    #define VE_LOCAL __attribute__((visibility("hidden")))
    #define VE_THREAD_LOCAL __thread

#elif defined(VE_COMPILER_MSVC)
    #define VE_FORCEINLINE __forceinline
    #define VE_INLINE __inline
    #define VE_NOINLINE __declspec(noinline)
    #define VE_NORETURN __declspec(noreturn)
    #define VE_USED
    #define VE_UNUSED
    #define VE_PACKED
    #define VE_ALIGN(x) __declspec(align(x))
    #define VE_COLD
    #define VE_HOT
    #define VE_MALLOC
    #define VE_PURE
    #define VE_CONST
    #define VE_WEAK
    #define VE_ALIAS(x)
    #define VE_DEPRECATED __declspec(deprecated)
    #define VE_EXPORT __declspec(dllexport)
    #define VE_IMPORT __declspec(dllimport)
    #define VE_LOCAL
    #define VE_THREAD_LOCAL __declspec(thread)

#else
    #define VE_FORCEINLINE inline
    #define VE_INLINE inline
    #define VE_NOINLINE
    #define VE_NORETURN
    #define VE_USED
    #define VE_UNUSED
    #define VE_PACKED
    #define VE_ALIGN(x)
    #define VE_COLD
    #define VE_HOT
    #define VE_MALLOC
    #define VE_PURE
    #define VE_CONST
    #define VE_WEAK
    #define VE_ALIAS(x)
    #define VE_DEPRECATED
    #define VE_EXPORT
    #define VE_IMPORT
    #define VE_LOCAL
    #define VE_THREAD_LOCAL __thread
#endif

/* Debug break */
#if defined(VE_PLATFORM_WINDOWS)
    #define VE_DEBUG_BREAK() __debugbreak()
#elif defined(VE_COMPILER_GCC) || defined(VE_COMPILER_CLANG)
    #if defined(VE_ARCH_X86)
        #define VE_DEBUG_BREAK() __asm__ volatile("int $3")
    #else
        #define VE_DEBUG_BREAK() __builtin_trap()
    #endif
#else
    #define VE_DEBUG_BREAK() *(volatile int*)0 = 0
#endif

/* Assert macro - use engine assert.h instead */
#ifndef VE_ASSERT
    #ifdef VE_BUILD_DEBUG
        #define VE_DEBUG_BREAK() VE_DEBUG_BREAK()
    #else
        #define VE_DEBUG_BREAK() ((void)0)
    #endif
#endif

/* Currency type (for file sizes, timestamps, etc.) */
#ifdef VE_ARCH_64BIT
    typedef int64_t ve_currency;
#else
    typedef int64_t ve_currency;
#endif

/* Size types */
#include <stdint.h>
typedef uint64_t ve_uint64;
typedef int64_t ve_int64;
typedef uint32_t ve_uint32;
typedef int32_t ve_int32;
typedef uint16_t ve_uint16;
typedef int16_t ve_int16;
typedef uint8_t ve_uint8;
typedef int8_t ve_int8;

/* Pointer size type */
#ifdef VE_ARCH_64BIT
    typedef uint64_t ve_uintptr;
    typedef int64_t ve_intptr;
#else
    typedef uint32_t ve_uintptr;
    typedef int32_t ve_intptr;
#endif

/* Size type */
typedef size_t ve_size;

/* Floating-point types */
typedef float ve_float32;
typedef double ve_float64;

/* String functions */
#ifdef VE_PLATFORM_WINDOWS
    #define ve_strcasecmp _stricmp
    #define ve_strncasecmp _strnicmp
    #define ve_strdup _strdup
    #define ve_copyfile CopyFileA
    #define ve_movefile MoveFileA
    #define ve_deletefile DeleteFileA
    #define ve_fileexists(path) (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
#else
    #define ve_strcasecmp strcasecmp
    #define ve_strncasecmp strncasecmp
    #define ve_strdup strdup
    #define ve_copyfile(src, dst) (rename(src, dst) == 0)
    #define ve_movefile(src, dst) (rename(src, dst) == 0)
    #define ve_deletefile unlink
    #define ve_fileexists(path) (access(path, F_OK) == 0)
#endif

/* Path separator */
#ifdef VE_PLATFORM_WINDOWS
    #define VE_PATH_SEPARATOR ';'
    #define VE_PATH_SEPARATOR_STR ";"
    #define VE_DIR_SEPARATOR '\\'
    #define VE_DIR_SEPARATOR_STR "\\"
#else
    #define VE_PATH_SEPARATOR ':'
    #define VE_PATH_SEPARATOR_STR ":"
    #define VE_DIR_SEPARATOR '/'
    #define VE_DIR_SEPARATOR_STR "/"
#endif

/* Dynamic library loading */
#ifdef VE_PLATFORM_WINDOWS
    typedef HMODULE ve_lib_handle;
    #define VE_LIB_LOAD(path) LoadLibraryA(path)
    #define VE_LIB_GET_SYM(handle, name) GetProcAddress(handle, name)
    #define VE_LIB_FREE(handle) FreeLibrary(handle)
#else
    #include <dlfcn.h>
    typedef void* ve_lib_handle;
    #define VE_LIB_LOAD(path) dlopen(path, RTLD_NOW | RTLD_LOCAL)
    #define VE_LIB_GET_SYM(handle, name) dlsym(handle, name)
    #define VE_LIB_FREE(handle) dlclose(handle)
#endif

/* Thread-local storage */
#ifdef VE_PLATFORM_WINDOWS
    #define VE_TLS_GET(key) TlsGetValue(key)
    #define VE_TLS_SET(key, value) TlsSetValue(key, value)
#else
    #include <pthread.h>
    #define VE_TLS_GET(key) pthread_getspecific(key)
    #define VE_TLS_SET(key, value) pthread_setspecific(key, value)
#endif

/* Atomic operations */
#ifdef VE_PLATFORM_WINDOWS
    #define VE_ATOMIC_READ(ptr) (*(volatile typeof(ptr))(ptr))
    #define VE_ATOMIC_WRITE(ptr, value) (*(volatile typeof(ptr))(ptr) = (value))
#else
    #define VE_ATOMIC_READ(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
    #define VE_ATOMIC_WRITE(ptr, value) __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST)
#endif

/* Memory barriers */
#ifdef VE_PLATFORM_WINDOWS
    #include <intrin.h>
    #define VE_MEMORY_BARRIER() MemoryBarrier()
#else
    #define VE_MEMORY_BARRIER() __sync_synchronize()
#endif

/* Cache line size */
#define VE_CACHE_LINE_SIZE 64

/* Align to cache line */
#define VE_ALIGN_CACHE VE_ALIGN(VE_CACHE_LINE_SIZE)

/* Page size (query at runtime) */
#ifdef VE_PLATFORM_WINDOWS
    #define VE_PAGE_SIZE_MASK 0xFFFF  /* Will be initialized at runtime */
#else
    #include <unistd.h>
    #define VE_PAGE_SIZE (sysconf(_SC_PAGESIZE))
#endif

/* Endianness */
#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define VE_LITTLE_ENDIAN 1
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define VE_BIG_ENDIAN 1
    #endif
#elif defined(_WIN32)
    #define VE_LITTLE_ENDIAN 1
#else
    #error "Endianness detection failed"
#endif

/* Byte swap macros */
#ifdef VE_LITTLE_ENDIAN
    #define VE_TO_LE16(x) (x)
    #define VE_TO_LE32(x) (x)
    #define VE_TO_LE64(x) (x)
    #define VE_FROM_LE16(x) (x)
    #define VE_FROM_LE32(x) (x)
    #define VE_FROM_LE64(x) (x)
#elif defined(VE_BIG_ENDIAN)
    #define VE_TO_LE16(x) (((x) >> 8) | ((x) << 8))
    #define VE_TO_LE32(x) (((x) >> 24) | (((x) & 0xFF0000) >> 8) | (((x) & 0xFF00) << 8) | ((x) << 24))
    #define VE_TO_LE64(x) ... /* Complex implementation */
    #define VE_FROM_LE16(x) VE_TO_LE16(x)
    #define VE_FROM_LE32(x) VE_TO_LE32(x)
    #define VE_FROM_LE64(x) VE_TO_LE64(x)
#endif

/* Function calling conventions */
#ifdef VE_PLATFORM_WINDOWS
    #define VE_CDECL __cdecl
    #define VE_STDCALL __stdcall
    #define VE_FASTCALL __fastcall
    #define VE_THISCALL __thiscall
    #define VE_VECTORCALL __vectorcall
#else
    #define VE_CDECL
    #define VE_STDCALL
    #define VE_FASTCALL
    #define VE_THISCALL
    #define VE_VECTORCALL
#endif

/* Export/import macros for shared libraries */
#ifdef VE_PLATFORM_WINDOWS
    #ifdef VE_BUILD_DLL
        #define VE_API VE_EXPORT
    #else
        #define VE_API VE_IMPORT
    #endif
#else
    #define VE_API
#endif

/* Debug tracing */
#ifdef VE_BUILD_DEBUG
    #define VE_TRACE_ENABLED 1
    #define VE_TRACE(msg, ...) printf("[TRACE] " msg "\n", ##__VA_ARGS__)
#else
    #define VE_TRACE_ENABLED 0
    #define VE_TRACE(msg, ...) ((void)0)
#endif

/* Performance counters */
#ifdef VE_PLATFORM_WINDOWS
    #include <intrin.h>
    #define VE_RDTSC() __rdtsc()
#else
    VE_INLINE uint64_t VE_RDTSC(void) {
        uint32_t lo, hi;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    }
#endif

/* CPU feature detection */
typedef struct ve_cpu_features {
    bool sse;
    bool sse2;
    bool sse3;
    bool ssse3;
    bool sse4_1;
    bool sse4_2;
    bool avx;
    bool avx2;
    bool avx512f;
    bool aes_ni;
    bool popcnt;
    bool bmi1;
    bool bmi2;
    bool fma;
    bool fma4;
    bool rdtsc;
    bool rdtscp;
    bool hypervisor;
    int cpu_count;
    int cache_line_size;
} ve_cpu_features;

/**
 * @brief Get CPU features
 *
 * @return CPU feature set
 */
const ve_cpu_features* ve_get_cpu_features(void);

/**
 * @brief Initialize platform layer
 *
 * @return true on success
 */
bool ve_platform_init(void);

/**
 * @brief Shutdown platform layer
 */
void ve_platform_shutdown(void);

/**
 * @brief Get system page size
 *
 * @return Page size in bytes
 */
size_t ve_get_page_size(void);

/**
 * @brief Get number of CPU cores
 *
 * @return Core count
 */
int ve_get_cpu_count(void);

/**
 * @brief Get total system memory
 *
 * @return Memory in bytes
 */
size_t ve_get_total_memory(void);

/**
 * @brief Get available system memory
 *
 * @return Memory in bytes
 */
size_t ve_get_available_memory(void);

/**
 * @brief Get current executable path
 *
 * @param buffer Output buffer
 * @param size Buffer size
 * @return true on success
 */
bool ve_get_executable_path(char* buffer, size_t size);

/**
 * @brief Get current working directory
 *
 * @param buffer Output buffer
 * @param size Buffer size
 * @return true on success
 */
bool ve_get_current_directory(char* buffer, size_t size);

/**
 * @brief Set current working directory
 *
 * @param path Directory path
 * @return true on success
 */
bool ve_set_current_directory(const char* path);

/**
 * @brief Get environment variable
 *
 * @param name Variable name
 * @param buffer Output buffer
 * @param size Buffer size
 * @return true on success
 */
bool ve_get_env_var(const char* name, char* buffer, size_t size);

/**
 * @brief Set environment variable
 *
 * @param name Variable name
 * @param value Variable value
 * @return true on success
 */
bool ve_set_env_var(const char* name, const char* value);

/**
 * @brief Get clipboard text
 *
 * @return Clipboard text (must be freed), or NULL on failure
 */
char* ve_get_clipboard_text(void);

/**
 * @brief Set clipboard text
 *
 * @param text Text to set
 * @return true on success
 */
bool ve_set_clipboard_text(const char* text);

#ifdef __cplusplus
}
#endif

#endif /* VE_PLATFORM_H */
