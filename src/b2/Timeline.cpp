#include <shared/system.h>
#include "Timeline.h"
#include <set>
#include <map>
#include <shared/debug.h>
#include "BeebWindow.h"
#include "BeebState.h"
#include "beeb_events.h"
#include "BeebWindows.h"
#include "BeebThread.h"
#include <shared/log.h>
#include <inttypes.h>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct StoredEvent {
    // Unique ID.
    uint64_t id=0;

    // Index of previous event.
    uint32_t prev_idx=UINT32_MAX;

    // Index of dominating significant event.
    uint32_t dom_idx=UINT32_MAX;

    BeebEvent be;

    StoredEvent(uint64_t id,uint32_t prev_idx,uint32_t dom_idx,BeebEvent be);
};

StoredEvent::StoredEvent(uint64_t id_,uint32_t prev_idx_,uint32_t dom_idx_,BeebEvent be_):
    id(id_),
    prev_idx(prev_idx_),
    dom_idx(dom_idx_),
    be(std::move(be_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TimelineState {
    std::vector<StoredEvent> events;

    uint64_t version=1;

    uint64_t next_event_id=1;

    // the global timeline mutex.
    std::recursive_mutex mutex;

    size_t num_adds=0;
    size_t num_add_fast_path_hits=0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static TimelineState *g_timeline_state=nullptr;

// This is supposed to cut down on clutter...
#define G (g_timeline_state)
#define LOCK std::unique_lock<std::recursive_mutex> timeline_lock(G->mutex)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct LessThanEventIdAndId {
    bool operator()(const StoredEvent &e,uint64_t id) {
        return e.id<id;
    }
};

static uint32_t LockedFindEventIndexById(uint64_t id) {
    if(id==0) {
        return UINT32_MAX;
    }

    auto it=std::lower_bound(G->events.begin(),G->events.end(),id,LessThanEventIdAndId());
    if(it==G->events.end()) {
        return UINT32_MAX;
    }

    if(it->id!=id) {
        return UINT32_MAX;
    }

    auto index=it-G->events.begin();
    ASSERT(index<UINT32_MAX);
    return (uint32_t)index;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void LockedCheck() {
#if ASSERT_ENABLED

    std::vector<uint64_t> all_ids;

    ASSERT(G->events.size()<UINT32_MAX);

    for(size_t i=1;i<G->events.size();++i) {
        const StoredEvent *e=&G->events[i];

        // Event ids must be valid.
        ASSERT(e->id!=0);

        // Synthetic events may not appear on the timeline.
        ASSERT(!(e->be.GetTypeFlags()&BeebEventTypeFlag_Synthetic));

        // The events must be in id order.
        ASSERT((e-1)->id<e->id);

        all_ids.push_back(e->id);

        // The previous event must be earlier than this one.
        if(e->prev_idx==UINT32_MAX) {
            ASSERT(e->prev_idx<i);
        }

        // The dominating interesting event must be earlier than this one.
        if(e->dom_idx!=UINT32_MAX) {
            ASSERT(e->dom_idx<i);

            // The dominating interesting event must be interesting.
            const StoredEvent *de=&G->events[e->dom_idx];
            ASSERT(de->be.GetTypeFlags()&BeebEventTypeFlag_ShownInUI);

            // The dominating interesting event must be the immediate
            // dominator.
            uint32_t oth_i=e->prev_idx;
            while(oth_i!=e->dom_idx) {
                const StoredEvent *oth_e=&G->events[oth_i];

                ASSERT(!(oth_e->be.GetTypeFlags()&BeebEventTypeFlag_ShownInUI));

                oth_i=oth_e->prev_idx;
            }
        }
    }

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Timeline::ReplayData::Event::Event(uint64_t id_,BeebEvent be_):
    id(id_),
    be(std::move(be_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Timeline::Tree::Event::Event(uint64_t id_,size_t prev_index_,BeebEvent be_):
    id(id_),
    prev_index(prev_index_),
    be(std::move(be_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Timeline::Init() {
    ASSERT(!G);
    G=new TimelineState;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Timeline::Shutdown() {
    delete G;
    G=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Timeline::DidChange() {
    LOCK;

    ++G->version;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t Timeline::GetVersion() {
    LOCK;

    return G->version;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t Timeline::AddEvent(uint64_t prev_id,BeebEvent event) {
    LOCK;

    ++G->num_adds;

    uint32_t type_flags=event.GetTypeFlags();

    // There's a good chance events will be added in order. There
    // could be multiple windows, but in general only one is
    // interacted with at a time.
    uint32_t prev_idx=UINT32_MAX;
    uint32_t dom_idx=UINT32_MAX;

    if(G->events.empty()) {
        ASSERT(prev_id==0);
    } else {
        if(G->events.back().id==prev_id) {
            ASSERT(G->events.size()<=UINT32_MAX);
            prev_idx=(uint32_t)(G->events.size()-1);
            ++G->num_add_fast_path_hits;
        } else {
            prev_idx=LockedFindEventIndexById(prev_id);
        }

        if(prev_idx!=UINT32_MAX) {
            const StoredEvent *prev_se=&G->events[prev_idx];

            if(prev_se->be.GetTypeFlags()&BeebEventTypeFlag_ShownInUI) {
                dom_idx=prev_idx;
            } else {
                dom_idx=prev_se->dom_idx;
            }
        }
    }

    uint64_t id=G->next_event_id++;
    G->events.emplace_back(id,prev_idx,dom_idx,std::move(event));

    if(type_flags&BeebEventTypeFlag_ChangesTimeline) {
        DidChange();
    }

    return id;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t Timeline::GetNumEvents() {
    LOCK;

    return G->events.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Timeline::Tree Timeline::GetTree() {
    LOCK;

    Tree tree;

    std::map<uint64_t,size_t> e_index_by_se_index;

    for(size_t se_index=0;se_index<G->events.size();++se_index) {
        const StoredEvent *se=&G->events[se_index];

        if(se->be.GetTypeFlags()&BeebEventTypeFlag_ShownInUI) {
            size_t e_index=tree.events.size();
            e_index_by_se_index[se_index]=e_index;

            size_t prev_index;
            if(se->dom_idx==UINT32_MAX) {
                prev_index=SIZE_MAX;
                tree.root_idxs.push_back(e_index);
            } else {
                ASSERT(e_index_by_se_index.count(se->dom_idx)==1);
                prev_index=e_index_by_se_index[se->dom_idx];
            }

            tree.events.emplace_back(se->id,prev_index,se->be);
        }
    }

    for(size_t i=0;i<BeebWindows::GetNumWindows();++i) {
        BeebWindow *window=BeebWindows::GetWindowByIndex(i);
        std::shared_ptr<BeebThread> thread=window->GetBeebThread();

        uint64_t id=thread->GetParentTimelineEventId();
        uint32_t se_index=LockedFindEventIndexById(id);

        size_t prev_index;
        if(se_index==UINT32_MAX) {
        no_dominator:
            // no event dominates window, or it was missing.
            prev_index=SIZE_MAX;
            tree.root_idxs.push_back(tree.events.size());
        } else {
            const StoredEvent *se=&G->events[se_index];

            if(se->be.GetTypeFlags()&BeebEventTypeFlag_ShownInUI) {
                // parent event dominates window.
            } else if(se->dom_idx==UINT32_MAX) {
                // no appropriate dominating node.
                ASSERT(false);
                goto no_dominator;
            } else {
                // parent event's dominator dominates window.
                se_index=se->dom_idx;
            }

            ASSERT(e_index_by_se_index.count(se_index)==1);
            prev_index=e_index_by_se_index[se_index];
        }

        tree.events.emplace_back(0,prev_index,BeebEvent::MakeWindowProxy(window));
    }

    return tree;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Timeline::ReplayData Timeline::CreateReplay(uint64_t start_id,uint64_t finish_id,Messages *msg) {
    LOCK;

    ASSERT(finish_id!=0);
    ASSERT(start_id!=0);

    uint32_t se_idx=LockedFindEventIndexById(start_id);
    if(se_idx==UINT32_MAX) {
        msg->e.f("first event doesn't exist: %" PRIu64 "\n",start_id);
        // Start event doesn't exist.
        return ReplayData();
    }

    // Handle SaveState specially - it isn't a start event itself, but
    // it will get replaced with a LoadState one later, so it's OK.
    if(!(G->events[se_idx].be.GetTypeFlags()&BeebEventTypeFlag_Start)&&G->events[se_idx].be.type!=BeebEventType_SaveState) {
        msg->e.f("first event is unsuitable\n");
        msg->i.f("(must be SaveState/SetConfig, not: %s)\n",GetBeebEventTypeEnumName(G->events[se_idx].be.type));
        // Start event isn't a start event.
        return ReplayData();
    }

    se_idx=LockedFindEventIndexById(finish_id);
    if(se_idx==UINT32_MAX) {
        // Finish event doesn't exist.
        msg->e.f("last event doesn't exist: %" PRIu64 "\n",finish_id);
        return ReplayData();
    }

    ReplayData replay;

    const StoredEvent *se;
    for(;;) {
        se=&G->events[se_idx];

        replay.events.emplace_back(se->id,se->be);

        if(se->id==start_id) {
            break;
        }

        se_idx=se->prev_idx;
        if(se_idx==UINT32_MAX) {
            // This is currently impossible, but it won't stay that
            // way forever.
            msg->e.f("internal error: invalid replay.\n");
            return ReplayData();
        }
    }

    std::reverse(replay.events.begin(),replay.events.end());

    // If event 0 is a SaveState, replace it with an equivalent
    // LoadState. SaveState events are generally ignored when
    // replaying, but the initial one's state wants to actually be
    // used, of course; rather than have special logic in the playback
    // code to handle this, it's easier just to fix up the first event
    // if necessary.
    if(replay.events[0].be.type==BeebEventType_SaveState) {
        replay.events[0].be=BeebEvent::MakeLoadState(replay.events[0].be.data.state->state);
    }

    ASSERT(replay.events[0].be.GetTypeFlags()&BeebEventTypeFlag_Start);

    return replay;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Timeline::DeleteEvent(uint64_t id) {
    LOCK;

    uint32_t se_idx=LockedFindEventIndexById(id);
    if(se_idx==UINT32_MAX) {
        return;
    }

    StoredEvent *se=&G->events[se_idx];
    ASSERT(se->be.GetTypeFlags()&BeebEventTypeFlag_CanDelete);
    if(!(se->be.GetTypeFlags()&BeebEventTypeFlag_CanDelete)) {
        return;
    }

    // This is a bit of a cheeky way of doing it.
    se->be=BeebEvent::MakeNone(se->be.time_2MHz_cycles);

    // Any events dominated by this one must now be dominated by its
    // dominator instead.
    for(size_t i=se_idx+1;i<G->events.size();++i) {
        StoredEvent *oth_se=&G->events[i];

        if(oth_se->dom_idx==se_idx) {
            oth_se->dom_idx=se->dom_idx;
        }
    }
    
    DidChange();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Timeline::Dump(Log *log) {
    LOCK;

    log->f("Full: ");
    {
        LogIndenter indent(log);

        for(size_t i=0;i<G->events.size();++i) {
            StoredEvent *se=&G->events[i];

            log->f("#%-9zu id=%-10" PRIu64 " prev=",i,se->id);

            if(se->prev_idx==UINT32_MAX) {
                log->f("%-11s","(none)");
            } else {
                log->f("#%-10" PRIu32,se->prev_idx);
            }

            log->f(" dom=");

            if(se->dom_idx==UINT32_MAX) {
                log->f("%-11s","(none)");
            } else {
                log->f("#%-10" PRIu32,se->dom_idx);
            }

            log->f(" time=%-10" PRIu64,se->be.time_2MHz_cycles);
            log->f(" type=%s (",GetBeebEventTypeEnumName(se->be.type));
            se->be.DumpSummary(log);
            log->f(")");
            log->f("\n");
        }
    }

    log->f("Windows: ");
    {
        LogIndenter indent(log);

        for(size_t i=0;i<BeebWindows::GetNumWindows();++i) {
            BeebWindow *window=BeebWindows::GetWindowByIndex(i);
            std::shared_ptr<BeebThread> thread=window->GetBeebThread();

            uint64_t parent_id=thread->GetParentTimelineEventId();

            log->f("#%-3zu parent id=%-10" PRIu64 " (%s; SDL Window ID=%" PRIu32 ")\n",i,parent_id,window->GetName().c_str(),window->GetSDLWindowID());
        }
    }

    log->f("Tree: ");
    {
        LogIndenter indent(log);

        {
            Tree tree=GetTree();

            for(size_t i=0;i<tree.events.size();++i) {
                const Tree::Event *te=&tree.events[i];

                log->f("#%-9zu id=%-10" PRIu64,i,te->id);

                log->f(" prev=");
                if(te->prev_index==SIZE_MAX) {
                    log->f("%-11s","(none)");
                } else {
                    log->f("#%-10zu",te->prev_index);
                }

                log->f(" time=%-10" PRIu64,te->be.time_2MHz_cycles);
                log->f(" type=%s (",GetBeebEventTypeEnumName(te->be.type));
                te->be.DumpSummary(log);
                log->f(")");
                log->f("\n");
            }

            log->f("Roots:");
            for(size_t i=0;i<tree.root_idxs.size();++i) {
                log->f(" %zu",tree.root_idxs[i]);
            }
            log->f("\n");
        }
    }

    log->f("Timeline::Add fast path hit rate: %zu/%zu (%.1f%%)\n",
           G->num_add_fast_path_hits,G->num_adds,
           (double)G->num_add_fast_path_hits/G->num_adds*100.);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Timeline::Check() {
    LOCK;

    LockedCheck();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
