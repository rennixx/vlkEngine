/**
 * @file thread.h
 * @brief Thread utilities and synchronization primitives
 */

#ifndef VE_THREAD_H
#define VE_THREAD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread handle
 */
typedef struct ve_thread ve_thread;

/**
 * @brief Thread function type
 *
 * @param user_data User data pointer
 * @return Thread return value
 */
typedef void* (*ve_thread_fn)(void* user_data);

/**
 * @brief Mutex handle
 */
typedef struct ve_mutex ve_mutex;

/**
 * @brief Read-write mutex handle
 */
typedef struct ve_rwlock ve_rwlock;

/**
 * @brief Condition variable
 */
typedef struct ve_condvar ve_condvar;

/**
 * @brief Semaphore
 */
typedef struct ve_semaphore ve_semaphore;

/**
 * @brief Atomic 32-bit integer
 */
typedef struct ve_atomic_int32 {
    volatile int32_t value;
} ve_atomic_int32;

/**
 * @brief Atomic 64-bit integer
 */
typedef struct ve_atomic_int64 {
    volatile int64_t value;
} ve_atomic_int64;

/**
 * @brief Atomic pointer
 */
typedef struct ve_atomic_ptr {
    volatile void* value;
} ve_atomic_ptr;

/**
 * @brief Thread ID type
 */
typedef uint64_t ve_thread_id;

/* Thread functions */

/**
 * @brief Create a new thread
 *
 * @param func Thread function
 * @param user_data User data to pass to thread function
 * @param name Optional thread name (for debugging)
 * @return Thread handle, or NULL on failure
 */
ve_thread* ve_thread_create(ve_thread_fn func, void* user_data, const char* name);

/**
 * @brief Wait for a thread to finish and destroy it
 *
 * @param thread Thread to join
 * @return Thread return value
 */
void* ve_thread_join(ve_thread* thread);

/**
 * @brief Detach a thread (runs independently)
 *
 * @param thread Thread to detach
 */
void ve_thread_detach(ve_thread* thread);

/**
 * @brief Get the current thread ID
 *
 * @return Current thread ID
 */
ve_thread_id ve_thread_get_current_id(void);

/**
 * @brief Get the current thread's name
 *
 * @param buffer Buffer to store name
 * @param size Buffer size
 * @return true on success
 */
bool ve_thread_get_name(char* buffer, size_t size);

/**
 * @brief Set the current thread's name
 *
 * @param name Thread name
 */
void ve_thread_set_name(const char* name);

/**
 * @brief Yield execution to another thread
 */
void ve_thread_yield(void);

/**
 * @brief Suggested time slice for other threads
 *
 * @param milliseconds Milliseconds to sleep (0 = yield)
 */
void ve_thread_sleep_ms(uint32_t milliseconds);

/* Mutex functions */

/**
 * @brief Create a new mutex
 *
 * @return Mutex handle, or NULL on failure
 */
ve_mutex* ve_mutex_create(void);

/**
 * @brief Destroy a mutex
 *
 * @param mutex Mutex to destroy
 */
void ve_mutex_destroy(ve_mutex* mutex);

/**
 * @brief Lock a mutex (blocking)
 *
 * @param mutex Mutex to lock
 */
void ve_mutex_lock(ve_mutex* mutex);

/**
 * @brief Try to lock a mutex (non-blocking)
 *
 * @param mutex Mutex to try locking
 * @return true if lock acquired, false otherwise
 */
bool ve_mutex_try_lock(ve_mutex* mutex);

/**
 * @brief Unlock a mutex
 *
 * @param mutex Mutex to unlock
 */
void ve_mutex_unlock(ve_mutex* mutex);

/* Read-write lock functions */

/**
 * @brief Create a new read-write lock
 *
 * @return RW lock handle, or NULL on failure
 */
ve_rwlock* ve_rwlock_create(void);

/**
 * @brief Destroy a read-write lock
 *
 * @param lock RW lock to destroy
 */
void ve_rwlock_destroy(ve_rwlock* lock);

/**
 * @brief Acquire read lock (multiple readers allowed)
 *
 * @param lock RW lock to lock for reading
 */
void ve_rwlock_read_lock(ve_rwlock* lock);

/**
 * @brief Try to acquire read lock
 *
 * @param lock RW lock to try locking for reading
 * @return true if lock acquired
 */
bool ve_rwlock_try_read_lock(ve_rwlock* lock);

/**
 * @brief Acquire write lock (exclusive access)
 *
 * @param lock RW lock to lock for writing
 */
void ve_rwlock_write_lock(ve_rwlock* lock);

/**
 * @brief Try to acquire write lock
 *
 * @param lock RW lock to try locking for writing
 * @return true if lock acquired
 */
bool ve_rwlock_try_write_lock(ve_rwlock* lock);

/**
 * @brief Release read-write lock
 *
 * @param lock RW lock to unlock
 */
void ve_rwlock_unlock(ve_rwlock* lock);

/* Condition variable functions */

/**
 * @brief Create a new condition variable
 *
 * @return Condition variable, or NULL on failure
 */
ve_condvar* ve_condvar_create(void);

/**
 * @brief Destroy a condition variable
 *
 * @param cv Condition variable to destroy
 */
void ve_condvar_destroy(ve_condvar* cv);

/**
 * @brief Signal one waiting thread
 *
 * @param cv Condition variable to signal
 */
void ve_condvar_signal(ve_condvar* cv);

/**
 * @brief Signal all waiting threads
 *
 * @param cv Condition variable to signal
 */
void ve_condvar_broadcast(ve_condvar* cv);

/**
 * @brief Wait for condition with timeout
 *
 * @param cv Condition variable to wait on
 * @param mutex Mutex to lock during wait
 * @param timeout_ms Timeout in milliseconds (UINT32_MAX = infinite)
 * @return true if signaled, false if timeout
 */
bool ve_condvar_wait(ve_condvar* cv, ve_mutex* mutex, uint32_t timeout_ms);

/* Semaphore functions */

/**
 * @brief Create a new semaphore
 *
 * @param initial_count Initial count value
 * @param max_count Maximum count value
 * @return Semaphore, or NULL on failure
 */
ve_semaphore* ve_semaphore_create(uint32_t initial_count, uint32_t max_count);

/**
 * @brief Destroy a semaphore
 *
 * @param sem Semaphore to destroy
 */
void ve_semaphore_destroy(ve_semaphore* sem);

/**
 * @brief Wait for semaphore with timeout
 *
 * @param sem Semaphore to wait on
 * @param timeout_ms Timeout in milliseconds (UINT32_MAX = infinite)
 * @return true if acquired, false if timeout
 */
bool ve_semaphore_wait(ve_semaphore* sem, uint32_t timeout_ms);

/**
 * @brief Signal semaphore (increment count)
 *
 * @param sem Semaphore to signal
 * @return true on success
 */
bool ve_semaphore_signal(ve_semaphore* sem);

/* Atomic operations */

/**
 * @brief Atomic load 32-bit integer
 */
int32_t ve_atomic_load32(const volatile ve_atomic_int32* atomic);

/**
 * @brief Atomic store 32-bit integer
 */
void ve_atomic_store32(volatile ve_atomic_int32* atomic, int32_t value);

/**
 * @brief Atomic add to 32-bit integer (returns old value)
 */
int32_t ve_atomic_fetch_add32(volatile ve_atomic_int32* atomic, int32_t operand);

/**
 * @brief Atomic subtract from 32-bit integer (returns old value)
 */
int32_t ve_atomic_fetch_sub32(volatile ve_atomic_int32* atomic, int32_t operand);

/**
 * @brief Atomic compare and exchange 32-bit integer
 *
 * @return true if exchange occurred
 */
bool ve_atomic_compare_exchange32(volatile ve_atomic_int32* atomic, int32_t* expected, int32_t desired);

/**
 * @brief Atomic increment 32-bit integer (returns new value)
 */
int32_t ve_atomic_increment32(volatile ve_atomic_int32* atomic);

/**
 * @brief Atomic decrement 32-bit integer (returns new value)
 */
int32_t ve_atomic_decrement32(volatile ve_atomic_int32* atomic);

/**
 * @brief Atomic load 64-bit integer
 */
int64_t ve_atomic_load64(const volatile ve_atomic_int64* atomic);

/**
 * @brief Atomic store 64-bit integer
 */
void ve_atomic_store64(volatile ve_atomic_int64* atomic, int64_t value);

/**
 * @brief Atomic add to 64-bit integer (returns old value)
 */
int64_t ve_atomic_fetch_add64(volatile ve_atomic_int64* atomic, int64_t operand);

/**
 * @brief Atomic load pointer
 */
void* ve_atomic_load_ptr(const volatile ve_atomic_ptr* atomic);

/**
 * @brief Atomic store pointer
 */
void ve_atomic_store_ptr(volatile ve_atomic_ptr* atomic, void* value);

/**
 * @brief Atomic compare and exchange pointer
 *
 * @return true if exchange occurred
 */
bool ve_atomic_compare_exchange_ptr(volatile ve_atomic_ptr* atomic, void** expected, void* desired);

/**
 * @brief Memory barrier
 */
void ve_memory_barrier(void);

/**
 * @brief Read memory barrier
 */
void ve_read_memory_barrier(void);

/**
 * @brief Write memory barrier
 */
void ve_write_memory_barrier(void);

/* Thread-local storage */

/**
 * @brief TLS key type
 */
typedef struct ve_tls_key ve_tls_key;

/**
 * @brief Create a TLS key
 *
 * @param destructor Optional destructor function for stored values
 * @return TLS key, or NULL on failure
 */
ve_tls_key* ve_tls_create(void (*destructor)(void*));

/**
 * @brief Destroy a TLS key
 *
 * @param key TLS key to destroy
 */
void ve_tls_destroy(ve_tls_key* key);

/**
 * @brief Set TLS value
 *
 * @param key TLS key
 * @param value Value to store
 * @return true on success
 */
bool ve_tls_set(ve_tls_key* key, void* value);

/**
 * @brief Get TLS value
 *
 * @param key TLS key
 * @return Stored value
 */
void* ve_tls_get(ve_tls_key* key);

/* RAII-style helpers */

/**
 * @brief Scope-based mutex lock
 */
#define VE_MUTEX_LOCK(mutex) \
    for (int _ve_i_ = ((ve_mutex_lock(mutex), 0); _ve_i_ < 1; _ve_i_++, ve_mutex_unlock(mutex))

/**
 * @brief Scope-based read lock
 */
#define VE_RWLOCK_READ_LOCK(lock) \
    for (int _ve_i_ = ((ve_rwlock_read_lock(lock), 0); _ve_i_ < 1; _ve_i_++, ve_rwlock_unlock(lock))

/**
 * @brief Scope-based write lock
 */
#define VE_RWLOCK_WRITE_LOCK(lock) \
    for (int _ve_i_ = ((ve_rwlock_write_lock(lock), 0); _ve_i_ < 1; _ve_i_++, ve_rwlock_unlock(lock)))

/* Thread pool */

/**
 * @brief Thread pool handle
 */
typedef struct ve_thread_pool ve_thread_pool;

/**
 * @brief Task function type
 */
typedef void (*ve_task_fn)(void* user_data);

/**
 * @brief Create a thread pool
 *
 * @param num_threads Number of worker threads (0 = hardware concurrency)
 * @return Thread pool, or NULL on failure
 */
ve_thread_pool* ve_thread_pool_create(uint32_t num_threads);

/**
 * @brief Destroy a thread pool
 *
 * @param pool Thread pool to destroy
 */
void ve_thread_pool_destroy(ve_thread_pool* pool);

/**
 * @brief Submit a task to the thread pool
 *
 * @param pool Thread pool
 * @param task Task function
 * @param user_data User data for task
 * @return true if task was submitted
 */
bool ve_thread_pool_submit(ve_thread_pool* pool, ve_task_fn task, void* user_data);

/**
 * @brief Wait for all tasks to complete
 *
 * @param pool Thread pool
 */
void ve_thread_pool_wait(ve_thread_pool* pool);

/**
 * @brief Get the number of worker threads
 *
 * @param pool Thread pool
 * @return Number of threads
 */
uint32_t ve_thread_pool_get_thread_count(const ve_thread_pool* pool);

/**
 * @brief Get approximate number of pending tasks
 *
 * @param pool Thread pool
 * @return Number of pending tasks
 */
size_t ve_thread_pool_get_pending_count(const ve_thread_pool* pool);

#ifdef __cplusplus
}
#endif

#endif /* VE_THREAD_H */
