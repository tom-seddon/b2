#ifndef HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6
#define HEADER_7A14D29E3FA348EC9D24D4E77D8B7AA6

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <mutex>
#include <condition_variable>
#include <string>

#ifndef MUTEX_DEBUGGING
#define MUTEX_DEBUGGING 1
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if MUTEX_DEBUGGING

#include <atomic>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MutexMetadata {
    std::string name;

    uint64_t num_locks=0;
    uint64_t num_contended_locks=0;
    uint64_t total_lock_wait_ticks=0;

    std::atomic<uint64_t> num_try_locks{0};
    uint64_t num_successful_try_locks=0;
};

struct MutexFullMetadata;

class Mutex
{
public:
    Mutex();
    ~Mutex();

    Mutex(const Mutex &)=delete;
    Mutex &operator=(const Mutex &)=delete;

    Mutex(Mutex &&)=delete;
    Mutex &operator=(Mutex &&)=delete;

    void SetName(std::string name);

    // the returned value isn't protected by any kind of mutex or
    // anything, so...
    const MutexMetadata *GetMetadata() const;

    void lock() {
        uint64_t a_ticks=GetCurrentTickCount();

        if(!m_mutex.try_lock()) {
            m_mutex.lock();
            ++m_meta->num_contended_locks;
        }

        ++m_meta->num_locks;
        m_meta->total_lock_wait_ticks+=GetCurrentTickCount()-a_ticks;
    }

    bool try_lock() {
        bool succeeded=m_mutex.try_lock();

        if(succeeded) {
            ++m_meta->num_successful_try_locks;
        }

        ++m_meta->num_try_locks;

        return succeeded;
    }

    void unlock() {
        m_mutex.unlock();
    }

    static std::vector<std::shared_ptr<const MutexMetadata>> GetAllMetadata();
protected:
private:
    std::mutex m_mutex;

    // The mutex has a shared_ptr to its MutexFullMetadata, so there's
    // no problem if the mutex goes away with a pointer to its
    // metadata still in a list returned by GetAllMetadata.
    std::shared_ptr<MutexFullMetadata> m_metadata;

    // This is just the value of &m_metadata_shared_ptr.get()->meta,
    // in an attempt to avoid atrocious debug build performance.
    MutexMetadata *m_meta=nullptr;
};

#define MUTEX_SET_NAME(MUTEX,NAME) ((MUTEX).SetName((NAME)))

typedef std::condition_variable_any ConditionVariable;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#else

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef std::mutex Mutex;
typedef std::condition_variable ConditionVariable;

#define MUTEX_SET_NAME(MUTEX,NAME) ((void)0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
