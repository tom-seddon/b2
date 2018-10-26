#include <shared/system.h>
#include <shared/mutex.h>

#if MUTEX_DEBUGGING

#include <shared/debug.h>
#include <vector>
#include <set>
#include <atomic>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MutexFullMetadata:
    public std::enable_shared_from_this<MutexFullMetadata>
{
    MutexFullMetadata *next=nullptr,*prev=nullptr;
    Mutex *mutex=nullptr;
    MutexMetadata meta;

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
    g_mutex_metadata_list_mutex=std::make_shared<std::mutex>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CheckMetadataList() {
    if(MutexFullMetadata *m=g_mutex_metadata_head) {
        do {
            ASSERT(m->prev->next==m);
            ASSERT(m->next->prev==m);

            m=m->next;
        } while(m!=g_mutex_metadata_head);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Mutex::Mutex():
    m_metadata(std::make_shared<MutexFullMetadata>())
{
    std::call_once(g_mutex_metadata_list_mutex_initialise_once_flag,&InitMutexMetadataListMutex);

    m_meta=&m_metadata.get()->meta;

    m_metadata->next=m_metadata.get();
    m_metadata->prev=m_metadata.get();
    m_metadata->mutex=this;
    m_metadata->metadata_list_mutex=g_mutex_metadata_list_mutex;

    std::lock_guard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    if(!g_mutex_metadata_head) {
        g_mutex_metadata_head=m_metadata.get();
    } else {
        m_metadata->prev=g_mutex_metadata_head->prev;
        m_metadata->next=g_mutex_metadata_head;

        m_metadata->prev->next=m_metadata.get();
        m_metadata->next->prev=m_metadata.get();
    }

    CheckMetadataList();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Mutex::~Mutex() {
    {
        std::lock_guard<std::mutex> lock(*m_metadata->metadata_list_mutex);

        if(m_metadata.get()==g_mutex_metadata_head) {
            g_mutex_metadata_head=m_metadata->next;

            if(m_metadata.get()==g_mutex_metadata_head) {
                // this was the last one in the list...
                g_mutex_metadata_head=nullptr;
            }
        }

        m_metadata->next->prev=m_metadata->prev;
        m_metadata->prev->next=m_metadata->next;

        m_metadata->mutex=nullptr;

        CheckMetadataList();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Mutex::SetName(std::string name) {
    std::lock_guard<std::mutex> lock(*m_metadata->metadata_list_mutex);

    m_metadata->meta.name=std::move(name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const MutexMetadata *Mutex::GetMetadata() const {
    return m_meta;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void Mutex::lock() {
//    uint64_t a_ticks=GetCurrentTickCount();
//
//    if(!m_mutex.try_lock()) {
//        m_mutex.lock();
//        ++m_meta->num_contended_locks;
//    }
//
//    ++m_meta->num_locks;
//    m_meta->total_lock_wait_ticks+=GetCurrentTickCount()-a_ticks;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool Mutex::try_lock() {
//    bool succeeded=m_mutex.try_lock();
//
//    if(succeeded) {
//        ++m_meta->num_successful_try_locks;
//    }
//
//    ++m_meta->num_try_locks;
//
//    return succeeded;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void Mutex::unlock() {
//    m_mutex.unlock();
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<const MutexMetadata>> Mutex::GetAllMetadata() {
    std::vector<std::shared_ptr<const MutexMetadata>> list;

    std::call_once(g_mutex_metadata_list_mutex_initialise_once_flag,&InitMutexMetadataListMutex);

    std::lock_guard<std::mutex> lock(*g_mutex_metadata_list_mutex);

    if(MutexFullMetadata *m=g_mutex_metadata_head) {
        do {
            list.push_back(std::shared_ptr<const MutexMetadata>(m->shared_from_this(),&m->meta));

            m=m->next;
        } while(m!=g_mutex_metadata_head);
    }

    return list;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
