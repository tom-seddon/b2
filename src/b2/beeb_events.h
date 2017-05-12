#ifndef HEADER_6D8602C8A2F449719A044C321D08D23A// -*- mode:c++ -*-
#define HEADER_6D8602C8A2F449719A044C321D08D23A

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A BeebEvent holds some info about something that happened, plus a
// timestamp - in some vaguely memory-efficient way, at least for
// common events, so there's some mildly fiddly data+handler-type
// system so that each event is a 17 byte value struct rather than
// some heap-allocated thing with virtuals. The official approved
// event list data structure is a vector<BeebEvent>.
//
// Compared to the beeb_lib Trace system, it plays more nicely with
// C++, but it isn't suitable for high-frequency events.
//
// (Each BeebEvent is just a record, and doesn't do much useful on its
// own. Maybe some serialization/UI/stuff later? - however the
// intention is that there'll always be a switch/case in the
// BeebThread to interpret them when replaying, rather than that being
// something the BeebEvent does itself.)
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DiscImage;
class BeebState;
class BeebWindow;
class Log;

namespace Timeline {
    class Node;
}

#include <beeb/conf.h>
#include <memory>
#include <string>
#include "BeebConfig.h"

// X.h nonsense.
#ifdef None
#undef None
#endif

#include <shared/enum_decl.h>
#include "beeb_events.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct BeebEventKeyState {
    uint8_t key,state;
};
#include <shared/poppack.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct BeebEventSetReset {
    uint8_t level;
};
#include <shared/poppack.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebEventLoadDiscImageData {
    uint8_t drive=0;

    std::shared_ptr<const DiscImage> disc_image;

    // This is here purely as a sanity check...
    std::string hash;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebEventConfigData {
    BeebLoadedConfig config;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct BeebEventHardReset {
    uint32_t flags;
};
#include <shared/poppack.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TURBO_DISC
#include <shared/pshpack1.h>
struct BeebEventSetTurboDisc {
    uint8_t turbo:1;
};
#include <shared/poppack.h>
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebEventStateData {
    std::shared_ptr<BeebState> state;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct BeebEventWindowProxy {
    uint32_t sdl_window_id;
};
#include <shared/poppack.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
union BeebEventData {
    BeebEventKeyState key_state;
    BeebEventSetReset set_reset;
    BeebEventLoadDiscImageData *load_disc_image;
    BeebEventConfigData *config;
    BeebEventHardReset hard_reset;
#if BBCMICRO_TURBO_DISC
    BeebEventSetTurboDisc set_turbo_disc;
#endif
    BeebEventStateData *state;
    BeebEventWindowProxy window_proxy;
};
#include <shared/poppack.h>


CHECK_SIZEOF(BeebEventData,sizeof(void *));

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
class BeebEvent {
public:
    // This is a BeebEventType. Need to fix the enum macros so they
    // can define C++11 struct enums with an underlying type...
    uint8_t type=BeebEventType_None;

    uint64_t time_2MHz_cycles=0;
    BeebEventData data={};

    static BeebEvent MakeNone(uint64_t time_2MHz_cycles);

    static BeebEvent MakeKeyState(uint64_t time_2MHz_cycles,uint8_t key,uint8_t state);

    static BeebEvent MakeSetReset(uint64_t time_2MHz_cycles,uint8_t level);

    static BeebEvent MakeLoadDiscImage(uint64_t time_2MHz_cycles,int drive,std::shared_ptr<const DiscImage> disc_image);

    static BeebEvent MakeRoot(BeebLoadedConfig config);
    static BeebEvent MakeChangeConfig(uint64_t time_2MHz_cycles,BeebLoadedConfig config);

    // FLAGS is a combination of BeebThreadReplaceFlag
    static BeebEvent MakeHardReset(uint64_t time_2MHz_cycles,uint32_t flags);

#if BBCMICRO_TURBO_DISC
    static BeebEvent MakeSetTurboDisc(uint64_t time_2MHz_cycles,bool turbo);
#endif

    static BeebEvent MakeLoadState(std::shared_ptr<BeebState> state);
    static BeebEvent MakeSaveState(uint64_t time_2MHz_cycles,std::shared_ptr<BeebState> state);

    //static BeebEvent MakeSetReplayProgress(uint64_t time_2MHz_cycles,const std::shared_ptr<Timeline::Node> &node,size_t index);

    static BeebEvent MakeWindowProxy(BeebWindow *window);

    ~BeebEvent();

    BeebEvent(BeebEvent &&oth);
    BeebEvent &operator=(BeebEvent &&oth);

    BeebEvent(const BeebEvent &oth);
    BeebEvent &operator=(const BeebEvent &oth);

    void Dump(Log *log) const;
    void DumpSummary(Log *log) const;//all on one line, no newline.

    uint32_t GetTypeFlags() const;
protected:
private:
    BeebEvent(uint8_t type,uint64_t time_2MHz_cycles,BeebEventData data);

    BeebEvent()=default;

    void Move(BeebEvent *oth);
    void Copy(const BeebEvent &src);
    static BeebEvent MakeLoadOrSaveStateEvent(BeebEventType type,uint64_t time,std::shared_ptr<BeebState> state);
    static BeebEvent MakeConfigEvent(BeebEventType type,uint64_t time,BeebLoadedConfig &&config);
};
#include <shared/poppack.h>

CHECK_SIZEOF(BeebEvent,1+sizeof(uint64_t)+sizeof(BeebEventData));

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
