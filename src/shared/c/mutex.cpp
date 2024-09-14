#include <shared/system.h>
#include <shared/mutex.h>

#if MUTEX_DEBUGGING

#include <shared/debug.h>
#include <vector>
#include <set>
#include <atomic>
#include <mutex>

#include <shared/enum_def.h>
#include <shared/mutex.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MutexMetadataImpl : public MutexMetadata {
    MutexStats stats;
    std::atomic<bool> reset{false};

    // A try_lock needs accounting for even if the mutex ends up not taken.
    std::atomic<uint64_t> num_try_locks{0};

    std::atomic<uint8_t> interesting_events{0};

    char *name = nullptr;

    ~MutexMetadataImpl();

    void RequestReset() override;
    const char *GetName() const override;
    void GetStats(MutexStats *stats) const override;
    uint8_t GetInterestingEvents() const;
    void SetInterestingEvents(uint8_t events);

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
    free(this->name), this->name = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::RequestReset() {
    this->stats = {};
    this->reset.store(true, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *MutexMetadataImpl::GetName() const {
    return this->name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MutexMetadataImpl::GetStats(MutexStats *stats_) const {
    *stats_ = this->stats;
    stats_->num_try_locks = this->num_try_locks.load(std::memory_order_acquire);
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

void Mutex::SetName(const char *name) {
    LockGuard<std::mutex> lock(*m_metadata->metadata_list_mutex);

    ASSERT(!m_meta->name);
    if (name) {
        m_meta->name = strdup(name);
    }
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

    if (m_mutex.try_lock()) {
        interesting_events &= ~MutexInterestingEvent_ContendedLock;
    } else {
#if MUTEX_ASSUME_UNCONTENDED_LOCKS_ARE_FREE
        uint64_t lock_start_ticks = GetCurrentTickCount();
#endif
        m_mutex.lock();
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

#ifdef _DEBUG
    if (!m_meta->name) {
        __nop();
    }
#endif

    if (interesting_events != 0) {
        this->OnInterestingEvents(interesting_events);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Mutex::try_lock() {
    bool succeeded = m_mutex.try_lock();

    if (succeeded) {
        ++m_meta->stats.num_successful_try_locks;

        // no need so set ever_locked - the successful lock that's blocking this
        // one already did it
    }

    ++m_meta->num_try_locks;

    if (m_meta->reset.exchange(false)) {
        m_meta->Reset();
    }

#ifdef _DEBUG
    if (!m_meta->name) {
        __nop();
    }
#endif

    return succeeded;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::unlock() {
    m_mutex.unlock();
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
