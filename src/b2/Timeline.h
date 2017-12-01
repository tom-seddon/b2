#ifndef HEADER_7B6D200A30074F409721F7CA86E434A5// -*- mode:c++ -*-
#define HEADER_7B6D200A30074F409721F7CA86E434A5

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebWindow;
class Messages;
class Log;

#include <memory>
#include <shared/mutex.h>
#include "beeb_events.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The timeline stores events as a sort of branching tree, with each
// event's parent event being the one that immediately precedes it. An
// event may have multiple children if multiple timelines start from
// that event.
//
// Events are uniquely identified by an non-zero 64-bit id assigned
// when they are added to the timeline. Pass an event's id to
// Timeline::AddEvent to add a child event, or pass in zero to add a
// new root event.
//
// Retrieve a list of all the significant events using
// Timeline::GetTree. Adding a significant event increments the
// timeline version counter, which can be retrieved with
// Timeline::GetVersion(). Compare this with the version field in the
// Tree object; if it's the same, the tree hasn't changed
// significantly since the last call.
//
// Two aspects of the timeline's internals can be used from outside:
//
// - call Timeline::DidChange to increment the timeline version number
//   manually
//
// - use Timeline::GetLock to get a lock on the timeline's mutex

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace Timeline {
    class ReplayData {
    public:
#include <shared/pshpack1.h>
        struct Event {
            // This event's timeline id. This can be used to locate
            // the event in the timeline, if it's still got an entry
            // there.
            //
            // Events that were never in the timeline will have an id
            // of 0.
            uint64_t id;

            BeebEvent be;

            Event(uint64_t id,BeebEvent be);
        };
#include <shared/poppack.h>

        // The first event is always SaveState or SetConfig, tagged
        // with the start time of the replay. There is always at least
        // 1 event.
        std::vector<Event> events;
    protected:
    private:
    };

    class Tree {
    public:
#include <shared/pshpack1.h>
        struct Event {
            // The event id may be 0, if this event was synthesized.
            uint64_t id;

            size_t prev_index;

            BeebEvent be;

            Event(uint64_t id,size_t prev_index,BeebEvent be);
        };
#include <shared/poppack.h>

        uint64_t version=0;

        std::vector<Event> events;

        std::vector<size_t> root_idxs;
    protected:
    private:
    };

    bool Init();
    void Shutdown();

    void DidChange();

    uint64_t GetVersion();

    // Adds event EVENT to the timeline. ID refers to the previous
    // event. Return value is the new event's id.
    //
    // If the event is significant, the timeline version is
    // incremented.
    uint64_t AddEvent(uint64_t prev_id,BeebEvent event);

    size_t GetNumEvents();

    Tree GetTree();

    // Create replay. The first event must be a start event.
    //
    // A valid replay always contains at least 1 event; the result
    // will have 0 events if anything went wrong (and a message -
    // though maybe not a very useful one - will have been printed out
    // to the supplied Messages).
    ReplayData CreateReplay(uint64_t start_id,uint64_t finish_id,Messages *msg);

    // Delete event. Only events with the CanDelete flag can be deleted.
    void DeleteEvent(uint64_t id);

    void Dump(Log *log);

    void Check();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
