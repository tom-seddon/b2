#ifndef HEADER_7B6D200A30074F409721F7CA86E434A5// -*- mode:c++ -*-
#define HEADER_7B6D200A30074F409721F7CA86E434A5

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#include "beeb_events.h"
#include <memory>

class BeebLoadedConfig;
class BeebState;
//class BeebEvent;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

//class Timeline {
//public:
//    explicit Timeline(std::shared_ptr<BeebState> initial_state,
//                      uint64_t begin_2MHz_cycles,
//                      uint64_t end_2MHz_cycles,
//                      std::vector<BeebEvent> events);
//
//    uint64_t GetBegin2MHzCycles() const;
//    uint64_t GetEnd2MHzCycles() const;
//
//    std::unique_ptr<BBCMicro> GetInitialBeebState() const;
//
//    size_t GetNumEvents() const;
//    const BeebEvent *GetEventByIndex(size_t index) const;
//protected:
//private:
//    std::shared_ptr<BeebState> m_initial_state;
//    uint64_t m_begin_2MHz_cycles=0;
//    uint64_t m_end_2MHz_cycles=0;
//    std::vector<BeebEvent> m_events;
//};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#endif
