/**
 * @file thread.c
 * @brief Thread utilities implementation
 */

#include "thread.h"
#include "memory.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define VE_PLATFORM_WINDOWS
    #include <windows.h>

    typedef HANDLE thread_handle_t;
    typedef CRITICAL_SECTION mutex_handle_t;
    typedef CONDITION_VARIABLE condvar_handle_t;
    typedef HANDLE semaphore_handle_t;
    typedef DWORD thread_id_t;

    #define THREAD_CALL_CONV WINAPI
#else
    #define VE_PLATFORM_POSIX
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <errno.h>

    typedef pthread_t thread_handle_t;
    typedef struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
    } condvar_handle_t;
    typedef struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        uint32_t count;
        uint32_t max_count;
    } semaphore_handle_t;
    typedef pthread_t thread_id_t;

    #define THREAD_CALL_CONV
#endif

/* Thread implementation */
struct ve_thread {
    thread_handle_t handle;
    ve_thread_id id;
    char name[64];
};

/* Mutex implementation */
struct ve_mutex {
    mutex_handle_t handle;
};

/* Read-write lock implementation */
struct ve_rwlock {
#if defined(VE_PLATFORM_WINDOWS)
    SRWLOCK lock;
#else
    pthread_rwlock_t lock;
#endif
};

/* Condition variable implementation */
struct ve_condvar {
    condvar_handle_t handle;
};

/* Semaphore implementation */
struct ve_semaphore {
    semaphore_handle_t* handle;
};

/* Thread pool implementation */
struct ve_thread_pool {
    ve_thread** threads;
    uint32_t thread_count;
    ve_mutex* queue_mutex;
    ve_condvar* queue_condvar;
    ve_semaphore* task_semaphore;

    struct {
        ve_task_fn func;
        void* user_data;
    }* task_queue;

    size_t queue_capacity;
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
    bool shutdown;
};

/* Platform-specific implementations */

#if defined(VE_PLATFORM_WINDOWS)

/* Thread functions */
static DWORD WINAPI thread_wrapper(LPVOID param) {
    ve_thread* thread = (ve_thread*)param;
    return 0;
}

ve_thread* ve_thread_create(ve_thread_fn func, void* user_data, const char* name) {
    ve_thread* thread = (ve_thread*)VE_ALLOCATE_TAG(sizeof(ve_thread), VE_MEMORY_TAG_CORE);
    if (!thread) {
        return NULL;
    }

    memset(thread, 0, sizeof(ve_thread));

    /* Set thread name */
    if (name) {
        strncpy(thread->name, name, sizeof(thread->name) - 1);
    }

    /* Create thread */
    thread->handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, user_data, 0, NULL);
    if (!thread->handle) {
        VE_FREE(thread);
        return NULL;
    }

    return thread;
}

void* ve_thread_join(ve_thread* thread) {
    if (!thread) {
        return NULL;
    }

    WaitForSingleObject(thread->handle, INFINITE);
    DWORD result;
    GetExitCodeThread(thread->handle, &result);
    CloseHandle(thread->handle);
    VE_FREE(thread);

    return (void*)(intptr_t)result;
}

void ve_thread_detach(ve_thread* thread) {
    if (thread) {
        CloseHandle(thread->handle);
        VE_FREE(thread);
    }
}

ve_thread_id ve_thread_get_current_id(void) {
    return (ve_thread_id)GetCurrentThreadId();
}

bool ve_thread_get_name(char* buffer, size_t size) {
    /* Windows doesn't have a simple way to get thread name */
    return false;
}

void ve_thread_set_name(const char* name) {
    /* Set thread name for debugger */
    typedef struct {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
    } THREADNAME_INFO;

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;

    __try {
        RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD), (ULONG_PTR*)&info);
    } __except (EXCEPTION_CONTINUE_EXECUTION) {
    }
}

void ve_thread_yield(void) {
    SwitchToThread();
}

void ve_thread_sleep_ms(uint32_t milliseconds) {
    Sleep(milliseconds);
}

/* Mutex functions */
ve_mutex* ve_mutex_create(void) {
    ve_mutex* mutex = (ve_mutex*)VE_ALLOCATE_TAG(sizeof(ve_mutex), VE_MEMORY_TAG_CORE);
    if (!mutex) {
        return NULL;
    }

    InitializeCriticalSection(&mutex->handle);
    return mutex;
}

void ve_mutex_destroy(ve_mutex* mutex) {
    if (mutex) {
        DeleteCriticalSection(&mutex->handle);
        VE_FREE(mutex);
    }
}

void ve_mutex_lock(ve_mutex* mutex) {
    if (mutex) {
        EnterCriticalSection(&mutex->handle);
    }
}

bool ve_mutex_try_lock(ve_mutex* mutex) {
    if (!mutex) {
        return false;
    }
    return TryEnterCriticalSection(&mutex->handle) != 0;
}

void ve_mutex_unlock(ve_mutex* mutex) {
    if (mutex) {
        LeaveCriticalSection(&mutex->handle);
    }
}

/* RW lock functions */
ve_rwlock* ve_rwlock_create(void) {
    ve_rwlock* lock = (ve_rwlock*)VE_ALLOCATE_TAG(sizeof(ve_rwlock), VE_MEMORY_TAG_CORE);
    if (!lock) {
        return NULL;
    }

    InitializeSRWLock(&lock->lock);
    return lock;
}

void ve_rwlock_destroy(ve_rwlock* lock) {
    VE_FREE(lock);
}

void ve_rwlock_read_lock(ve_rwlock* lock) {
    if (lock) {
        AcquireSRWLockShared(&lock->lock);
    }
}

bool ve_rwlock_try_read_lock(ve_rwlock* lock) {
    if (!lock) {
        return false;
    }
    return TryAcquireSRWLockShared(&lock->lock) != 0;
}

void ve_rwlock_write_lock(ve_rwlock* lock) {
    if (lock) {
        AcquireSRWLockExclusive(&lock->lock);
    }
}

bool ve_rwlock_try_write_lock(ve_rwlock* lock) {
    if (!lock) {
        return false;
    }
    return TryAcquireSRWLockExclusive(&lock->lock) != 0;
}

void ve_rwlock_unlock(ve_rwlock* lock) {
    if (lock) {
        /* Windows doesn't distinguish between read and write unlock */
        ReleaseSRWLockExclusive(&lock->lock);
    }
}

/* Condition variable functions */
ve_condvar* ve_condvar_create(void) {
    ve_condvar* cv = (ve_condvar*)VE_ALLOCATE_TAG(sizeof(ve_condvar), VE_MEMORY_TAG_CORE);
    if (!cv) {
        return NULL;
    }

    InitializeConditionVariable(&cv->handle);
    return cv;
}

void ve_condvar_destroy(ve_condvar* cv) {
    VE_FREE(cv);
}

void ve_condvar_signal(ve_condvar* cv) {
    if (cv) {
        WakeConditionVariable(&cv->handle);
    }
}

void ve_condvar_broadcast(ve_condvar* cv) {
    if (cv) {
        WakeAllConditionVariable(&cv->handle);
    }
}

bool ve_condvar_wait(ve_condvar* cv, ve_mutex* mutex, uint32_t timeout_ms) {
    if (!cv || !mutex) {
        return false;
    }

    DWORD timeout = (timeout_ms == UINT32_MAX) ? INFINITE : timeout_ms;
    return SleepConditionVariableCS(&cv->handle, &mutex->handle, timeout) != 0;
}

/* Semaphore functions */
ve_semaphore* ve_semaphore_create(uint32_t initial_count, uint32_t max_count) {
    ve_semaphore* sem = (ve_semaphore*)VE_ALLOCATE_TAG(sizeof(ve_semaphore), VE_MEMORY_TAG_CORE);
    if (!sem) {
        return NULL;
    }

    sem->handle = (semaphore_handle_t*)VE_ALLOCATE_TAG(sizeof(HANDLE), VE_MEMORY_TAG_CORE);
    if (!sem->handle) {
        VE_FREE(sem);
        return NULL;
    }

    HANDLE h = CreateSemaphoreA(NULL, initial_count, max_count, NULL);
    if (!h) {
        VE_FREE(sem->handle);
        VE_FREE(sem);
        return NULL;
    }

    *(HANDLE*)sem->handle = h;
    return sem;
}

void ve_semaphore_destroy(ve_semaphore* sem) {
    if (sem && sem->handle) {
        CloseHandle(*(HANDLE*)sem->handle);
        VE_FREE(sem->handle);
        VE_FREE(sem);
    }
}

bool ve_semaphore_wait(ve_semaphore* sem, uint32_t timeout_ms) {
    if (!sem || !sem->handle) {
        return false;
    }

    DWORD timeout = (timeout_ms == UINT32_MAX) ? INFINITE : timeout_ms;
    return WaitForSingleObject(*(HANDLE*)sem->handle, timeout) == WAIT_OBJECT_0;
}

bool ve_semaphore_signal(ve_semaphore* sem) {
    if (!sem || !sem->handle) {
        return false;
    }
    return ReleaseSemaphore(*(HANDLE*)sem->handle, 1, NULL) != 0;
}

#else /* POSIX implementation */

/* Thread functions */
struct thread_wrapper_data {
    ve_thread_fn func;
    void* user_data;
    ve_thread* thread;
};

static void* thread_wrapper(void* param) {
    struct thread_wrapper_data* data = (struct thread_wrapper_data*)param;
    ve_thread* thread = data->thread;

    if (thread->name[0] != '\0') {
#if defined(__linux__)
        pthread_setname_np(pthread_self(), thread->name);
#elif defined(__APPLE__)
        pthread_setname_np(thread->name);
#endif
    }

    void* result = data->func(data->user_data);
    VE_FREE(data);

    return result;
}

ve_thread* ve_thread_create(ve_thread_fn func, void* user_data, const char* name) {
    ve_thread* thread = (ve_thread*)VE_ALLOCATE_TAG(sizeof(ve_thread), VE_MEMORY_TAG_CORE);
    if (!thread) {
        return NULL;
    }

    memset(thread, 0, sizeof(ve_thread));

    if (name) {
        strncpy(thread->name, name, sizeof(thread->name) - 1);
    }

    struct thread_wrapper_data* wrapper_data = (struct thread_wrapper_data*)VE_ALLOCATE_TAG(
        sizeof(struct thread_wrapper_data), VE_MEMORY_TAG_CORE);
    if (!wrapper_data) {
        VE_FREE(thread);
        return NULL;
    }

    wrapper_data->func = func;
    wrapper_data->user_data = user_data;
    wrapper_data->thread = thread;

    if (pthread_create(&thread->handle, NULL, thread_wrapper, wrapper_data) != 0) {
        VE_FREE(wrapper_data);
        VE_FREE(thread);
        return NULL;
    }

    return thread;
}

void* ve_thread_join(ve_thread* thread) {
    if (!thread) {
        return NULL;
    }

    void* result = NULL;
    pthread_join(thread->handle, &result);
    VE_FREE(thread);
    return result;
}

void ve_thread_detach(ve_thread* thread) {
    if (thread) {
        pthread_detach(thread->handle);
        VE_FREE(thread);
    }
}

ve_thread_id ve_thread_get_current_id(void) {
    return (ve_thread_id)pthread_self();
}

bool ve_thread_get_name(char* buffer, size_t size) {
#if defined(__linux__) || defined(__APPLE__)
    return pthread_getname_np(pthread_self(), buffer, size) == 0;
#else
    return false;
#endif
}

void ve_thread_set_name(const char* name) {
#if defined(__linux__)
    pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
    pthread_setname_np(name);
#endif
}

void ve_thread_yield(void) {
    sched_yield();
}

void ve_thread_sleep_ms(uint32_t milliseconds) {
    usleep(milliseconds * 1000);
}

/* Mutex functions */
ve_mutex* ve_mutex_create(void) {
    ve_mutex* mutex = (ve_mutex*)VE_ALLOCATE_TAG(sizeof(ve_mutex), VE_MEMORY_TAG_CORE);
    if (!mutex) {
        return NULL;
    }

    pthread_mutex_init(&mutex->handle, NULL);
    return mutex;
}

void ve_mutex_destroy(ve_mutex* mutex) {
    if (mutex) {
        pthread_mutex_destroy(&mutex->handle);
        VE_FREE(mutex);
    }
}

void ve_mutex_lock(ve_mutex* mutex) {
    if (mutex) {
        pthread_mutex_lock(&mutex->handle);
    }
}

bool ve_mutex_try_lock(ve_mutex* mutex) {
    if (!mutex) {
        return false;
    }
    return pthread_mutex_trylock(&mutex->handle) == 0;
}

void ve_mutex_unlock(ve_mutex* mutex) {
    if (mutex) {
        pthread_mutex_unlock(&mutex->handle);
    }
}

/* RW lock functions */
ve_rwlock* ve_rwlock_create(void) {
    ve_rwlock* lock = (ve_rwlock*)VE_ALLOCATE_TAG(sizeof(ve_rwlock), VE_MEMORY_TAG_CORE);
    if (!lock) {
        return NULL;
    }

    pthread_rwlock_init(&lock->lock, NULL);
    return lock;
}

void ve_rwlock_destroy(ve_rwlock* lock) {
    if (lock) {
        pthread_rwlock_destroy(&lock->lock);
        VE_FREE(lock);
    }
}

void ve_rwlock_read_lock(ve_rwlock* lock) {
    if (lock) {
        pthread_rwlock_rdlock(&lock->lock);
    }
}

bool ve_rwlock_try_read_lock(ve_rwlock* lock) {
    if (!lock) {
        return false;
    }
    return pthread_rwlock_tryrdlock(&lock->lock) == 0;
}

void ve_rwlock_write_lock(ve_rwlock* lock) {
    if (lock) {
        pthread_rwlock_wrlock(&lock->lock);
    }
}

bool ve_rwlock_try_write_lock(ve_rwlock* lock) {
    if (!lock) {
        return false;
    }
    return pthread_rwlock_trywrlock(&lock->lock) == 0;
}

void ve_rwlock_unlock(ve_rwlock* lock) {
    if (lock) {
        pthread_rwlock_unlock(&lock->lock);
    }
}

/* Condition variable functions */
ve_condvar* ve_condvar_create(void) {
    ve_condvar* cv = (ve_condvar*)VE_ALLOCATE_TAG(sizeof(ve_condvar), VE_MEMORY_TAG_CORE);
    if (!cv) {
        return NULL;
    }

    pthread_mutex_init(&cv->handle.mutex, NULL);
    pthread_cond_init(&cv->handle.cond, NULL);
    return cv;
}

void ve_condvar_destroy(ve_condvar* cv) {
    if (cv) {
        pthread_mutex_destroy(&cv->handle.mutex);
        pthread_cond_destroy(&cv->handle.cond);
        VE_FREE(cv);
    }
}

void ve_condvar_signal(ve_condvar* cv) {
    if (cv) {
        pthread_mutex_lock(&cv->handle.mutex);
        pthread_cond_signal(&cv->handle.cond);
        pthread_mutex_unlock(&cv->handle.mutex);
    }
}

void ve_condvar_broadcast(ve_condvar* cv) {
    if (cv) {
        pthread_mutex_lock(&cv->handle.mutex);
        pthread_cond_broadcast(&cv->handle.cond);
        pthread_mutex_unlock(&cv->handle.mutex);
    }
}

bool ve_condvar_wait(ve_condvar* cv, ve_mutex* mutex, uint32_t timeout_ms) {
    if (!cv || !mutex) {
        return false;
    }

    struct timespec ts;
    if (timeout_ms != UINT32_MAX) {
        struct timeval tv;
        gettimeofday(&tv, NULL);

        uint64_t nsec = tv.tv_usec * 1000 + timeout_ms * 1000000;
        ts.tv_sec = tv.tv_sec + nsec / 1000000000;
        ts.tv_nsec = nsec % 1000000000;
    }

    int result;
    if (timeout_ms == UINT32_MAX) {
        result = pthread_cond_wait(&cv->handle.cond, &mutex->handle);
    } else {
        result = pthread_cond_timedwait(&cv->handle.cond, &mutex->handle, &ts);
    }

    return result == 0;
}

/* Semaphore functions */
ve_semaphore* ve_semaphore_create(uint32_t initial_count, uint32_t max_count) {
    (void)max_count;  /* Not used in POSIX semaphores */
    ve_semaphore* sem = (ve_semaphore*)VE_ALLOCATE_TAG(sizeof(ve_semaphore), VE_MEMORY_TAG_CORE);
    if (!sem) {
        return NULL;
    }

    sem->handle = (semaphore_handle_t*)VE_ALLOCATE_TAG(sizeof(semaphore_handle_t), VE_MEMORY_TAG_CORE);
    if (!sem->handle) {
        VE_FREE(sem);
        return NULL;
    }

    sem->handle->count = initial_count;
    sem->handle->max_count = max_count;
    pthread_mutex_init(&sem->handle->mutex, NULL);
    pthread_cond_init(&sem->handle->cond, NULL);

    return sem;
}

void ve_semaphore_destroy(ve_semaphore* sem) {
    if (sem && sem->handle) {
        pthread_mutex_destroy(&sem->handle->mutex);
        pthread_cond_destroy(&sem->handle->cond);
        VE_FREE(sem->handle);
        VE_FREE(sem);
    }
}

bool ve_semaphore_wait(ve_semaphore* sem, uint32_t timeout_ms) {
    if (!sem || !sem->handle) {
        return false;
    }

    pthread_mutex_lock(&sem->handle->mutex);

    while (sem->handle->count == 0) {
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&sem->handle->mutex);
            return false;
        }

        if (timeout_ms == UINT32_MAX) {
            pthread_cond_wait(&sem->handle->cond, &sem->handle->mutex);
        } else {
            struct timespec ts;
            struct timeval tv;
            gettimeofday(&tv, NULL);

            uint64_t nsec = tv.tv_usec * 1000 + timeout_ms * 1000000;
            ts.tv_sec = tv.tv_sec + nsec / 1000000000;
            ts.tv_nsec = nsec % 1000000000;

            if (pthread_cond_timedwait(&sem->handle->cond, &sem->handle->mutex, &ts) != 0) {
                pthread_mutex_unlock(&sem->handle->mutex);
                return false;
            }
        }
    }

    sem->handle->count--;
    pthread_mutex_unlock(&sem->handle->mutex);
    return true;
}

bool ve_semaphore_signal(ve_semaphore* sem) {
    if (!sem || !sem->handle) {
        return false;
    }

    pthread_mutex_lock(&sem->handle->mutex);

    if (sem->handle->count < sem->handle->max_count) {
        sem->handle->count++;
        pthread_cond_signal(&sem->handle->cond);
    }

    pthread_mutex_unlock(&sem->handle->mutex);
    return true;
}

#endif /* Platform selection */

/* Atomic operations - portable implementation */

int32_t ve_atomic_load32(const volatile ve_atomic_int32* atomic) {
#if defined(_MSC_VER)
    return atomic->value;
#else
    return __atomic_load_n(&atomic->value, __ATOMIC_SEQ_CST);
#endif
}

void ve_atomic_store32(volatile ve_atomic_int32* atomic, int32_t value) {
#if defined(_MSC_VER)
    InterlockedExchange((volatile LONG*)&atomic->value, value);
#else
    __atomic_store_n(&atomic->value, value, __ATOMIC_SEQ_CST);
#endif
}

int32_t ve_atomic_fetch_add32(volatile ve_atomic_int32* atomic, int32_t operand) {
#if defined(_MSC_VER)
    return InterlockedExchangeAdd((volatile LONG*)&atomic->value, operand);
#else
    return __atomic_fetch_add(&atomic->value, operand, __ATOMIC_SEQ_CST);
#endif
}

int32_t ve_atomic_fetch_sub32(volatile ve_atomic_int32* atomic, int32_t operand) {
#if defined(_MSC_VER)
    return InterlockedExchangeAdd((volatile LONG*)&atomic->value, -operand);
#else
    return __atomic_fetch_sub(&atomic->value, operand, __ATOMIC_SEQ_CST);
#endif
}

bool ve_atomic_compare_exchange32(volatile ve_atomic_int32* atomic, int32_t* expected, int32_t desired) {
#if defined(_MSC_VER)
    int32_t actual = InterlockedCompareExchange((volatile LONG*)&atomic->value, desired, *expected);
    if (actual == *expected) {
        return true;
    }
    *expected = actual;
    return false;
#else
    return __atomic_compare_exchange_n(&atomic->value, expected, desired, false,
                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

int32_t ve_atomic_increment32(volatile ve_atomic_int32* atomic) {
#if defined(_MSC_VER)
    return InterlockedIncrement((volatile LONG*)&atomic->value);
#else
    return __atomic_add_fetch(&atomic->value, 1, __ATOMIC_SEQ_CST);
#endif
}

int32_t ve_atomic_decrement32(volatile ve_atomic_int32* atomic) {
#if defined(_MSC_VER)
    return InterlockedDecrement((volatile LONG*)&atomic->value);
#else
    return __atomic_sub_fetch(&atomic->value, 1, __ATOMIC_SEQ_CST);
#endif
}

int64_t ve_atomic_load64(const volatile ve_atomic_int64* atomic) {
#if defined(_MSC_VER)
    return atomic->value;
#else
    return __atomic_load_n(&atomic->value, __ATOMIC_SEQ_CST);
#endif
}

void ve_atomic_store64(volatile ve_atomic_int64* atomic, int64_t value) {
#if defined(_MSC_VER)
    InterlockedExchange64((volatile LONGLONG*)&atomic->value, value);
#else
    __atomic_store_n(&atomic->value, value, __ATOMIC_SEQ_CST);
#endif
}

int64_t ve_atomic_fetch_add64(volatile ve_atomic_int64* atomic, int64_t operand) {
#if defined(_MSC_VER)
    return InterlockedExchangeAdd64((volatile LONGLONG*)&atomic->value, operand);
#else
    return __atomic_fetch_add(&atomic->value, operand, __ATOMIC_SEQ_CST);
#endif
}

void* ve_atomic_load_ptr(const volatile ve_atomic_ptr* atomic) {
#if defined(_MSC_VER)
    return (void*)atomic->value;
#else
    return __atomic_load_n(&atomic->value, __ATOMIC_SEQ_CST);
#endif
}

void ve_atomic_store_ptr(volatile ve_atomic_ptr* atomic, void* value) {
#if defined(_MSC_VER)
    InterlockedExchangePointer((void* volatile*)&atomic->value, value);
#else
    __atomic_store_n(&atomic->value, value, __ATOMIC_SEQ_CST);
#endif
}

bool ve_atomic_compare_exchange_ptr(volatile ve_atomic_ptr* atomic, void** expected, void* desired) {
#if defined(_MSC_VER)
    void* actual = InterlockedCompareExchangePointer((void* volatile*)&atomic->value, desired, *expected);
    if (actual == *expected) {
        return true;
    }
    *expected = actual;
    return false;
#else
    return __atomic_compare_exchange_n(&atomic->value, expected, desired, false,
                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

void ve_memory_barrier(void) {
#if defined(_MSC_VER)
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

void ve_read_memory_barrier(void) {
#if defined(_MSC_VER)
    _ReadBarrier();
#else
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif
}

void ve_write_memory_barrier(void) {
#if defined(_MSC_VER)
    _WriteBarrier();
#else
    __atomic_thread_fence(__ATOMIC_RELEASE);
#endif
}

/* TLS implementation */
struct ve_tls_key {
#if defined(VE_PLATFORM_WINDOWS)
    DWORD key;
#else
    pthread_key_t key;
#endif
};

typedef void (*tls_destructor_fn)(void*);

static void tls_wrapper_destructor(void* value) {
    /* Placeholder - actual destructor is stored in TLS key creation */
    (void)value;
}

ve_tls_key* ve_tls_create(void (*destructor)(void*)) {
    ve_tls_key* key = (ve_tls_key*)VE_ALLOCATE_TAG(sizeof(ve_tls_key), VE_MEMORY_TAG_CORE);
    if (!key) {
        return NULL;
    }

#if defined(VE_PLATFORM_WINDOWS)
    key->key = TlsAlloc();
    if (key->key == TLS_OUT_OF_INDEXES) {
        VE_FREE(key);
        return NULL;
    }
#else
    if (pthread_key_create(&key->key, destructor) != 0) {
        VE_FREE(key);
        return NULL;
    }
#endif

    return key;
}

void ve_tls_destroy(ve_tls_key* key) {
    if (!key) {
        return;
    }

#if defined(VE_PLATFORM_WINDOWS)
    TlsFree(key->key);
#else
    pthread_key_delete(key->key);
#endif

    VE_FREE(key);
}

bool ve_tls_set(ve_tls_key* key, void* value) {
    if (!key) {
        return false;
    }

#if defined(VE_PLATFORM_WINDOWS)
    return TlsSetValue(key->key, value) != 0;
#else
    return pthread_setspecific(key->key, value) == 0;
#endif
}

void* ve_tls_get(ve_tls_key* key) {
    if (!key) {
        return NULL;
    }

#if defined(VE_PLATFORM_WINDOWS)
    return TlsGetValue(key->key);
#else
    return pthread_getspecific(key->key);
#endif
}

/* Thread pool implementation */
static void* worker_thread(void* user_data) {
    ve_thread_pool* pool = (ve_thread_pool*)user_data;

    while (!pool->shutdown) {
        if (!ve_semaphore_wait(pool->task_semaphore, 100)) {
            continue;
        }

        ve_mutex_lock(pool->queue_mutex);

        if (pool->queue_count == 0) {
            ve_mutex_unlock(pool->queue_mutex);
            continue;
        }

        ve_task_fn task = pool->task_queue[pool->queue_head].func;
        void* task_data = pool->task_queue[pool->queue_head].user_data;

        pool->queue_head = (pool->queue_head + 1) % pool->queue_capacity;
        pool->queue_count--;

        ve_mutex_unlock(pool->queue_mutex);

        if (task) {
            task(task_data);
        }
    }

    return NULL;
}

ve_thread_pool* ve_thread_pool_create(uint32_t num_threads) {
    if (num_threads == 0) {
#if defined(VE_PLATFORM_WINDOWS)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        num_threads = sysinfo.dwNumberOfProcessors;
#else
        num_threads = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
#endif
    }

    ve_thread_pool* pool = (ve_thread_pool*)VE_ALLOCATE_TAG(sizeof(ve_thread_pool), VE_MEMORY_TAG_CORE);
    if (!pool) {
        return NULL;
    }

    memset(pool, 0, sizeof(ve_thread_pool));

    pool->thread_count = num_threads;
    pool->queue_capacity = 1024;
    pool->shutdown = false;

    pool->queue_mutex = ve_mutex_create();
    pool->queue_condvar = ve_condvar_create();
    pool->task_semaphore = ve_semaphore_create(0, UINT32_MAX);

    if (!pool->queue_mutex || !pool->queue_condvar || !pool->task_semaphore) {
        ve_thread_pool_destroy(pool);
        return NULL;
    }

    pool->task_queue = (void*)VE_ALLOCATE_TAG(pool->queue_capacity * sizeof(pool->task_queue[0]),
                                               VE_MEMORY_TAG_CORE);
    if (!pool->task_queue) {
        ve_thread_pool_destroy(pool);
        return NULL;
    }

    pool->threads = (ve_thread**)VE_ALLOCATE_TAG(num_threads * sizeof(ve_thread*),
                                                  VE_MEMORY_TAG_CORE);
    if (!pool->threads) {
        ve_thread_pool_destroy(pool);
        return NULL;
    }

    for (uint32_t i = 0; i < num_threads; i++) {
        char name[64];
        snprintf(name, sizeof(name), "Worker_%u", i);
        pool->threads[i] = ve_thread_create(worker_thread, pool, name);
        if (!pool->threads[i]) {
            pool->thread_count = i;
            ve_thread_pool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}

void ve_thread_pool_destroy(ve_thread_pool* pool) {
    if (!pool) {
        return;
    }

    pool->shutdown = true;

    if (pool->task_semaphore) {
        for (uint32_t i = 0; i < pool->thread_count; i++) {
            ve_semaphore_signal(pool->task_semaphore);
        }
    }

    for (uint32_t i = 0; i < pool->thread_count; i++) {
        if (pool->threads[i]) {
            ve_thread_join(pool->threads[i]);
        }
    }

    if (pool->threads) {
        VE_FREE(pool->threads);
    }

    if (pool->task_queue) {
        VE_FREE(pool->task_queue);
    }

    if (pool->queue_mutex) {
        ve_mutex_destroy(pool->queue_mutex);
    }

    if (pool->queue_condvar) {
        ve_condvar_destroy(pool->queue_condvar);
    }

    if (pool->task_semaphore) {
        ve_semaphore_destroy(pool->task_semaphore);
    }

    VE_FREE(pool);
}

bool ve_thread_pool_submit(ve_thread_pool* pool, ve_task_fn task, void* user_data) {
    if (!pool || !task) {
        return false;
    }

    ve_mutex_lock(pool->queue_mutex);

    if (pool->queue_count >= pool->queue_capacity) {
        ve_mutex_unlock(pool->queue_mutex);
        return false;
    }

    size_t tail = (pool->queue_head + pool->queue_count) % pool->queue_capacity;
    pool->task_queue[tail].func = task;
    pool->task_queue[tail].user_data = user_data;
    pool->queue_count++;

    ve_mutex_unlock(pool->queue_mutex);

    ve_semaphore_signal(pool->task_semaphore);
    return true;
}

void ve_thread_pool_wait(ve_thread_pool* pool) {
    if (!pool) {
        return;
    }

    while (pool->queue_count > 0) {
        ve_thread_sleep_ms(1);
    }
}

uint32_t ve_thread_pool_get_thread_count(const ve_thread_pool* pool) {
    return pool ? pool->thread_count : 0;
}

size_t ve_thread_pool_get_pending_count(const ve_thread_pool* pool) {
    if (!pool) {
        return 0;
    }

    ve_mutex_lock((ve_mutex*)pool->queue_mutex);
    size_t count = pool->queue_count;
    ve_mutex_unlock((ve_mutex*)pool->queue_mutex);

    return count;
}
