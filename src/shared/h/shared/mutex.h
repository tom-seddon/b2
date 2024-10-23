#ifndef HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6
#define HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <mutex>
#include <condition_variable>
#include <string>
#ifdef B2_LIBRETRO_CORE
#include "system.h"
#endif

#ifndef MUTEX_DEBUGGING
#define MUTEX_DEBUGGING 1
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if MUTEX_DEBUGGING

// If true, assume that try_lock is effectively free when it succeeds.
// Potentially save on some system calls for every lock.
#define MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE 0

#include <atomic>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Mutex;

struct MutexStats {
    // The mutex itself must be locked to correctly modify any of these.
    //
    // May read, at own risk...
    uint64_t num_locks = 0;
    uint64_t num_contended_locks = 0;
    uint64_t total_lock_wait_ticks = 0;
    uint64_t min_lock_wait_ticks = UINT64_MAX;
    uint64_t max_lock_wait_ticks = 0;
    uint64_t num_successful_try_locks = 0;

    uint64_t start_ticks;

    MutexStats();
};

struct MutexMetadata {
    std::string name;

    MutexStats stats;

    // A try_lock needs accounting for even if the mutex ends up not taken.
    std::atomic<uint64_t> num_try_locks{0};

    std::atomic<bool> reset{false};

    std::atomic<bool> interesting{false};

    Mutex *mutex = nullptr;

    // This is never reset, so that it's possible to distinguish no locks ever
    // from no locks since stats last reset.
    bool ever_locked = false;

    void RequestReset();

  private:
    void Reset();

    friend class Mutex;
};

struct MutexFullMetadata;

class Mutex {
  public:
    Mutex();
    ~Mutex();

    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    Mutex(Mutex &&) = delete;
    Mutex &operator=(Mutex &&) = delete;

    void SetName(std::string name);

    // the returned value isn't protected by any kind of mutex or
    // anything, so...
    const MutexMetadata *GetMetadata() const;

    void lock() {
        bool interesting = false;
#if !MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
        uint64_t lock_start_ticks = GetCurrentTickCount();
#endif
        uint64_t lock_wait_ticks = 0;

        if (!m_mutex.try_lock()) {
#if MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
            uint64_t lock_start_ticks = GetCurrentTickCount();
#endif
            m_mutex.lock();
            ++m_meta->stats.num_contended_locks;
#if MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
            lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
#endif

            if (m_meta->interesting.load(std::memory_order_acquire)) {
                interesting = true;
            }
        }

        ++m_meta->stats.num_locks;
#if !MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
        lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
#endif

        if (m_meta->reset.exchange(false)) {
            m_meta->Reset();
        }

        m_meta->ever_locked = true;
        m_meta->stats.total_lock_wait_ticks += lock_wait_ticks;

        if (lock_wait_ticks < m_meta->stats.min_lock_wait_ticks) {
            m_meta->stats.min_lock_wait_ticks = lock_wait_ticks;
        }

        if (lock_wait_ticks > m_meta->stats.max_lock_wait_ticks) {
            m_meta->stats.max_lock_wait_ticks = lock_wait_ticks;
        }

        if (interesting) {
            this->OnInterestingEvent();
        }
    }

    bool try_lock() {
        bool succeeded = m_mutex.try_lock();

        if (succeeded) {
            ++m_meta->stats.num_successful_try_locks;
            m_meta->ever_locked = true;
        }

        ++m_meta->num_try_locks;

        if (m_meta->reset.exchange(false)) {
            m_meta->Reset();
        }

        return succeeded;
    }

    void unlock() {
        m_mutex.unlock();
    }

    static std::vector<std::shared_ptr<MutexMetadata>> GetAllMetadata();

  protected:
  private:
    std::mutex m_mutex;

    // The mutex has a shared_ptr to its MutexFullMetadata, so there's
    // no problem if the mutex goes away with a pointer to its
    // metadata still in a list returned by GetAllMetadata.
    std::shared_ptr<MutexFullMetadata> m_metadata;

    // This is just the value of &m_metadata_shared_ptr.get()->meta,
    // in an attempt to avoid atrocious debug build performance.
    MutexMetadata *m_meta = nullptr;

    void OnInterestingEvent();
};

#define MUTEX_SET_NAME(MUTEX, NAME) ((MUTEX).SetName((NAME)))

typedef std::condition_variable_any ConditionVariable;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#else

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef std::mutex Mutex;
typedef std::condition_variable ConditionVariable;

#define MUTEX_SET_NAME(MUTEX, NAME) ((void)0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
