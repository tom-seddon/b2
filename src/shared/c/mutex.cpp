#include <shared/system.h>
#include <shared/mutex.h>

#if MUTEX_DEBUGGING

#include <shared/debug.h>
#include <vector>
#include <set>
#include <atomic>
#include <mutex>
#include <string.h>
#include <shared_mutex>

#include <shared/enum_def.h>
#include <shared/mutex.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MutexMetadataImpl : public MutexMetadata {
    std::mutex mutex;

    // this->stats.name is null, and this->stats.num_try_locks is bogus. They're
    // filled out when the data is copied by GetStats.
    MutexStats stats;
    std::atomic<bool> reset{false};

    // A try_lock needs accounting for even if the mutex ends up not taken.
    std::atomic<uint64_t> num_try_locks{0};

    std::atomic<uint8_t> interesting_events{0};

    mutable std::shared_mutex name_mutex;
    std::string name;

    ~MutexMetadataImpl();

    void RequestReset() override;
    void GetStats(MutexStats *stats) const override;
    uint8_t GetInterestingEvents() const override;
    void SetInterestingEvents(uint8_t events) override;

    void Reset();
};

struct MutexFullMetadata : public std::enable_shared_from_this<MutexFullMetadata> {
    MutexFullMetadata *next = nullptr, *prev = nullptr;
    Mutex *mutex = nullptr;
    MutexMetadataImpl meta;

    // this is just here to grab an appropriate extra ref to the
    // metadata list mutex. If a global mutex gets created before the
    // metadata mutex shared_ptr, that mutex would otherwise be
    // destroyed after it...
    std::shared_ptr<std::mutex> metadata_list_mutex;
};

static std::shared_ptr<std::mutex> g_mutex_metadata_list_mutex;
static std::once_flag g_mutex_metadata_list_mutex_initialise_once_flag;
static std::atomic<uint64_t> g_mutex_name_overhead_ticks{0};

static MutexFullMetadata *g_mutex_metadata_head;

static void InitMutexMetadataListMutex() {
    ASSERT(!g_mutex_metadata_list_mutex);
    g_mutex_metadata_list_mutex = std::make_shared<std::mutex>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CheckMetadataList() {
    if (MutexFullMetadata *m = g_mutex_metadata_head) {
        do {
            ASSERT(m->prev->next == m);
            ASSERT(m->next->prev == m);

            m = m->next;
        } while (m != g_mutex_metadata_head);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexStats::MutexStats()
    : start_ticks(GetCurrentTickCount()) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexMetadata::MutexMetadata() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexMetadata::~MutexMetadata() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexMetadataImpl::~MutexMetadataImpl() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::RequestReset() {
    this->stats = {};
    this->reset.store(true, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::GetStats(MutexStats *stats_) const {
    *stats_ = this->stats;

    stats_->num_try_locks = this->num_try_locks.load(std::memory_order_acquire);

    {
        uint64_t start_ticks = GetCurrentTickCount();

        this->name_mutex.lock_shared();
        stats_->name = this->name;
        this->name_mutex.unlock_shared();

        g_mutex_name_overhead_ticks.fetch_add(GetCurrentTickCount() - start_ticks, std::memory_order_acq_rel);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t MutexMetadataImpl::GetInterestingEvents() const {
    return this->interesting_events;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::SetInterestingEvents(uint8_t events) {
    // don't generate an unnecessary write
    if (events != this->interesting_events.load(std::memory_order_relaxed)) {
        this->interesting_events.store(events, std::memory_order_relaxed);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::Reset() {
    this->stats = {};
    this->num_try_locks.store(0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Mutex::Mutex()
    : m_metadata(std::make_shared<MutexFullMetadata>()) {
    std::call_once(g_mutex_metadata_list_mutex_initialise_once_flag, &InitMutexMetadataListMutex);

    m_meta = &m_metadata.get()->meta;

    m_metadata->next = m_metadata.get();
    m_metadata->prev = m_metadata.get();
    m_metadata->mutex = this;
    m_metadata->metadata_list_mutex = g_mutex_metadata_list_mutex;

    LockGuard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    if (!g_mutex_metadata_head) {
        g_mutex_metadata_head = m_metadata.get();
    } else {
        m_metadata->prev = g_mutex_metadata_head->prev;
        m_metadata->next = g_mutex_metadata_head;

        m_metadata->prev->next = m_metadata.get();
        m_metadata->next->prev = m_metadata.get();
    }

    CheckMetadataList();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Mutex::~Mutex() {
    {
        LockGuard<std::mutex> lock(*m_metadata->metadata_list_mutex);

        if (m_metadata.get() == g_mutex_metadata_head) {
            g_mutex_metadata_head = m_metadata->next;

            if (m_metadata.get() == g_mutex_metadata_head) {
                // this was the last one in the list...
                g_mutex_metadata_head = nullptr;
            }
        }

        m_metadata->next->prev = m_metadata->prev;
        m_metadata->prev->next = m_metadata->next;

        m_metadata->mutex = nullptr;

        CheckMetadataList();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::SetName(std::string name) {
    uint64_t start_ticks = GetCurrentTickCount();

    LockGuard<std::shared_mutex> lock(m_meta->name_mutex);
    m_meta->name = std::move(name);

    g_mutex_name_overhead_ticks.fetch_add(GetCurrentTickCount() - start_ticks, std::memory_order_acq_rel);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const MutexMetadata *Mutex::GetMetadata() const {
    return m_meta;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::lock() {
#if !MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
    uint64_t lock_start_ticks = GetCurrentTickCount();
#endif
    uint64_t lock_wait_ticks = 0;

    uint8_t interesting_events = m_meta->interesting_events.load(std::memory_order_relaxed);

    if (m_meta->mutex.try_lock()) {
        interesting_events &= (uint8_t)~MutexInterestingEvent_ContendedLock;
    } else {
#if MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
        uint64_t lock_start_ticks = GetCurrentTickCount();
#endif
        m_meta->mutex.lock();
        ++m_meta->stats.num_contended_locks;
#if MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
        lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
#endif
    }

    ++m_meta->stats.num_locks;
#if !MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
    lock_wait_ticks += GetCurrentTickCount() - lock_start_ticks;
#endif

    if (m_meta->reset.exchange(false)) {
        m_meta->Reset();
    }

    m_meta->stats.ever_locked = true;
    m_meta->stats.total_lock_wait_ticks += lock_wait_ticks;

    if (lock_wait_ticks < m_meta->stats.min_lock_wait_ticks) {
        m_meta->stats.min_lock_wait_ticks = lock_wait_ticks;
    }

    if (lock_wait_ticks > m_meta->stats.max_lock_wait_ticks) {
        m_meta->stats.max_lock_wait_ticks = lock_wait_ticks;
    }

    if (interesting_events != 0) {
        this->OnInterestingEvents(interesting_events);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Mutex::try_lock() {
    bool succeeded = m_meta->mutex.try_lock();

    if (succeeded) {
        ++m_meta->stats.num_successful_try_locks;

        // no need so set ever_locked - the successful lock that's blocking this
        // one already did it
    }

    ++m_meta->num_try_locks;

    if (m_meta->reset.exchange(false)) {
        m_meta->Reset();
    }

    return succeeded;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::unlock() {
    m_meta->mutex.unlock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<MutexMetadata>> Mutex::GetAllMetadata() {
    std::vector<std::shared_ptr<MutexMetadata>> list;

    std::call_once(g_mutex_metadata_list_mutex_initialise_once_flag, &InitMutexMetadataListMutex);

    LockGuard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    if (MutexFullMetadata *m = g_mutex_metadata_head) {
        do {
            list.push_back(std::shared_ptr<MutexMetadata>(m->shared_from_this(), &m->meta));

            m = m->next;
        } while (m != g_mutex_metadata_head);
    }

    return list;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t Mutex::GetNameOverheadTicks() {
    return g_mutex_name_overhead_ticks.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This exists purely as somewhere to put a breakpoint.
void Mutex::OnInterestingEvents(uint8_t interesting_events) {
    if (interesting_events & MutexInterestingEvent_Lock) {
#ifdef _MSC_VER
        __nop();
#endif
    }

    if (interesting_events & MutexInterestingEvent_ContendedLock) {
#ifdef _MSC_VER
        __nop();
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MutexNameSetter::MutexNameSetter(Mutex *mutex, const char *name) {
    MUTEX_SET_NAME(*mutex, name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
