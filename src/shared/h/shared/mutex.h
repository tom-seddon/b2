#ifndef HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6
#define HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifndef MUTEX_DEBUGGING
#define MUTEX_DEBUGGING 1
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if MUTEX_DEBUGGING

// If true, assume that try_lock is effectively free when it succeeds.
// Potentially save on some system calls for every lock.
#define MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE 0

#include <vector>
#include <string>
#include <memory>

#include "enum_decl.h"
#include "mutex.inl"
#include "enum_end.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Mutex;

struct MutexStats {
    uint64_t num_locks = 0;
    uint64_t num_contended_locks = 0;
    uint64_t total_lock_wait_ticks = 0;
    uint64_t min_lock_wait_ticks = UINT64_MAX;
    uint64_t max_lock_wait_ticks = 0;
    uint64_t num_successful_try_locks = 0;
    uint64_t start_ticks;
    uint64_t num_try_locks = 0;

    bool ever_locked = false;

    std::string name;

    MutexStats();
};

struct MutexMetadata {
  public:
    MutexMetadata();
    virtual ~MutexMetadata() = 0;

    MutexMetadata(const MutexMetadata &) = delete;
    MutexMetadata &operator=(const MutexMetadata &) = delete;
    MutexMetadata(MutexMetadata &&) = delete;
    MutexMetadata &operator=(MutexMetadata &&) = delete;

    virtual void GetStats(MutexStats *stats) const = 0;
    virtual void RequestReset() = 0;
    virtual uint8_t GetInterestingEvents() const = 0;
    virtual void SetInterestingEvents(uint8_t events) = 0;
};

struct MutexFullMetadata;
struct MutexMetadataImpl;

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

    static std::vector<std::shared_ptr<MutexMetadata>> GetAllMetadata();
    static uint64_t GetNameOverheadTicks();

    void lock();
    bool try_lock();
    void unlock();

  protected:
  private:
    std::shared_ptr<MutexFullMetadata> m_metadata;

    // This is just the value of &m_metadata_shared_ptr.get()->meta,
    // in an attempt to avoid atrocious debug build performance.
    MutexMetadataImpl *m_meta = nullptr;

    void OnInterestingEvents(uint8_t interesting_events);
};

// for use as a global.
class MutexNameSetter {
  public:
    MutexNameSetter(Mutex *mutex, const char *name);

  protected:
  private:
};

#define MUTEX_SET_NAME(MUTEX, NAME) ((MUTEX).SetName((NAME)))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#else

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <mutex> //#if !MUTEX_DEBUGGING

typedef std::mutex Mutex;

class MutexNameSetter {
  public:
    MutexNameSetter(Mutex *, const char *) {
    }

  protected:
  private:
};

#define MUTEX_SET_NAME(MUTEX, NAME) ((void)0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Pound shop equivalent of std::lock_guard, that doesn't need a standard header.
template <class MutexType>
class LockGuard {
  public:
    explicit LockGuard(MutexType &mutex)
        : m_mutex(&mutex) {
        m_mutex->lock();
    }

    ~LockGuard() {
        m_mutex->unlock();
    }

    LockGuard(const LockGuard<MutexType> &) = delete;
    LockGuard<MutexType> &operator=(const LockGuard<MutexType> &) = delete;
    LockGuard(LockGuard<MutexType> &&) = delete;
    LockGuard<MutexType> &operator=(LockGuard<MutexType> &&) = delete;

  protected:
  private:
    MutexType *m_mutex = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Janky std::unique_lock knockoff, ditto. Also forward declaration-friendly.
//
// (This does enough to work with condition_variable_any, but no more.
// std::unique_lock may still prove necessary.)
template <class MutexType>
class UniqueLock {
  public:
    explicit UniqueLock()
        : m_mutex(nullptr) {
    }

    explicit UniqueLock(MutexType &mutex)
        : m_mutex(&mutex) {
        m_mutex->lock();
        m_locked = true;
    }

    ~UniqueLock() {
        this->unlock();
    }

    UniqueLock(const UniqueLock<MutexType> &) = delete;
    UniqueLock<MutexType> &operator=(const UniqueLock<MutexType> &) = delete;

    UniqueLock(UniqueLock<MutexType> &&src)
        : m_mutex(src.m_mutex)
        , m_locked(src.m_locked) {
        src.m_mutex = nullptr;
        src.m_locked = false;
    }

    UniqueLock<MutexType> &operator=(UniqueLock<MutexType> &&other) {
        if (this != &other) {
            this->unlock();
            m_mutex = other.m_mutex;
            m_locked = other.m_locked;
            other.m_mutex = nullptr;
            other.m_locked = false;
        }

        return *this;
    }

    void lock() {
        if (m_mutex) {
            m_mutex->lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_mutex) {
            if (m_locked) {
                m_mutex->unlock();
                m_locked = false;
            }
        }
    }

  protected:
  private:
    MutexType *m_mutex = nullptr;
    bool m_locked = false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
