#include <shared/system.h>
#include "BeebWindows.h"
#include <shared/debug.h>
#include <SDL.h>
#include <algorithm>
#include "keymap.h"
#include "BeebWindow.h"
#include "BeebState.h"
#include "misc.h"
#include "BeebConfig.h"
#include "load_save.h"
#include "b2.h"
#include "BeebKeymap.h"
#include <Remotery.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(OUTPUT);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebWindowsState {
    std::vector<uint8_t> last_window_placement_data;

    JobQueue job_queue;

    // This mutex must be taken when manipulating saved_states.
    Mutex saved_states_mutex;
    std::vector<std::shared_ptr<const BeebState>> saved_states;
};

static BeebWindowsState *g_;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebWindowSettings BeebWindows::defaults;
std::string BeebWindows::default_config_name;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebWindows::Init() {
    ASSERT(!g_);

    g_=new BeebWindowsState;

    MUTEX_SET_NAME(g_->saved_states_mutex,"BeebWindows saved states mutex");

    if(!g_->job_queue.Init()) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::Shutdown() {
    {
        std::lock_guard<Mutex> lock(g_->saved_states_mutex);

        g_->saved_states.clear();
    }

    delete g_;
    g_=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::AddJob(std::shared_ptr<JobQueue::Job> job) {
    g_->job_queue.AddJob(std::move(job));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<JobQueue::Job>> BeebWindows::GetJobs() {
    return g_->job_queue.GetJobs();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<uint8_t> &BeebWindows::GetLastWindowPlacementData() {
    return g_->last_window_placement_data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::SetLastWindowPlacementData(std::vector<uint8_t> placement_data) {
    g_->last_window_placement_data=std::move(placement_data);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BeebWindows::GetNumSavedStates() {
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    return g_->saved_states.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<const BeebState>> BeebWindows::GetSavedStates(size_t begin_index,
                                                                          size_t end_index)
{
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    ASSERT(begin_index<=PTRDIFF_MAX);
    ASSERT(begin_index<=g_->saved_states.size());
    ASSERT(end_index<=PTRDIFF_MAX);
    ASSERT(end_index<=g_->saved_states.size());
    ASSERT(begin_index<=end_index);

    std::vector<std::shared_ptr<const BeebState>> result(g_->saved_states.begin()+(ptrdiff_t)begin_index,
                                                         g_->saved_states.begin()+(ptrdiff_t)end_index);
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::AddSavedState(std::shared_ptr<const BeebState> saved_state) {
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    g_->saved_states.push_back(std::move(saved_state));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebWindows::DeleteSavedState(std::shared_ptr<const BeebState> saved_state) {
    std::lock_guard<Mutex> lock(g_->saved_states_mutex);

    g_->saved_states.erase(std::remove(g_->saved_states.begin(),
                                       g_->saved_states.end(),
                                       saved_state),
                           g_->saved_states.end());
}
