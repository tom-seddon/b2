#ifndef HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6
#define HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <mutex>

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

    virtual const char *GetName() const = 0;
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

    // May only set the name before the first time the mutex is locked.
    void SetName(const char *name);

    // the returned value isn't protected by any kind of mutex or
    // anything, so...
    const MutexMetadata *GetMetadata() const;

    static std::vector<std::shared_ptr<MutexMetadata>> GetAllMetadata();

    void lock();
    bool try_lock();
    void unlock();

  protected:
  private:
    std::mutex m_mutex;

    std::shared_ptr<MutexFullMetadata> m_metadata;

    // This is just the value of &m_metadata_shared_ptr.get()->meta,
    // in an attempt to avoid atrocious debug build performance.
    MutexMetadataImpl *m_meta = nullptr;

    void OnInterestingEvents(uint8_t interesting_events);
};

#define MUTEX_SET_NAME(MUTEX, NAME) ((MUTEX).SetName((NAME)))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#else

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef std::mutex Mutex;

#define MUTEX_SET_NAME(MUTEX, NAME) ((void)0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
