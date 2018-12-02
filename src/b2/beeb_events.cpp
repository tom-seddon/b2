#include <shared/system.h>
#include "beeb_events.h"
//#include <shared/log.h>
//#include "keys.h"
//#include <beeb/DiscImage.h>
//#include <shared/debug.h>
//#include <beeb/conf.h>
//#include <inttypes.h>
//#include "misc.h"
//#include "BeebConfig.h"
//#include "BeebState.h"
//#include "BeebWindow.h"
//#include "BeebThread.h"
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#include <shared/enum_def.h>
//#include "beeb_events.inl"
//#include <shared/enum_end.h>
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventHandler {
//public:
//    static const BeebEventHandler *handlers[256];
//
//    uint32_t flags=0;
//
//    explicit BeebEventHandler(uint32_t flags);
//    virtual ~BeebEventHandler();
//
//    virtual void Dump(const BeebEvent *e,Log *log) const=0;
//    void Destroy(BeebEvent *e) const;
//    virtual BeebEventData Clone(const BeebEventData &src) const=0;
//protected:
//    virtual void HandleDestroy(BeebEventData *data) const=0;
//private:
//};
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEventHandler::BeebEventHandler(uint32_t flags_):
//    flags(flags_)
//{
//    if(this->flags&BeebEventTypeFlag_ShownInUI) {
//        this->flags|=BeebEventTypeFlag_ChangesTimeline;
//    }
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEventHandler::~BeebEventHandler() {
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//void BeebEventHandler::Destroy(BeebEvent *e) const {
//    this->HandleDestroy(&e->data);
//
//    e->type=BeebEventType_None;
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventValueHandler:
//    public BeebEventHandler
//{
//public:
//    explicit BeebEventValueHandler(uint32_t flags):
//        BeebEventHandler(flags)
//    {
//    }
//
//    BeebEventData Clone(const BeebEventData &src) const override {
//        return src;
//    }
//protected:
//    void HandleDestroy(BeebEventData *data) const override {
//        (void)data;
//    }
//private:
//};
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventNoneHandler:
//    public BeebEventValueHandler
//{
//public:
//    static const BeebEventNoneHandler INSTANCE;
//
//    BeebEventNoneHandler():
//        BeebEventValueHandler(0)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        (void)e,(void)log;
//    }
//protected:
//private:
//};
//
//const BeebEventNoneHandler BeebEventNoneHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventKeyStateHandler:
//    public BeebEventValueHandler
//{
//public:
//    static const BeebEventKeyStateHandler INSTANCE;
//
//    BeebEventKeyStateHandler():
//        BeebEventValueHandler(0)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        log->f("%s: %s (%u; 0x%02X)",
//               e->data.key_state.state?"press":"release",
//               GetBeebKeyName(e->data.key_state.key),e->data.key_state.key,e->data.key_state.key);
//        (void)e,(void)log;
//    }
//protected:
//private:
//};
//
//const BeebEventKeyStateHandler BeebEventKeyStateHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventLoadDiscImageHandler:
//    public BeebEventHandler
//{
//public:
//    static const BeebEventLoadDiscImageHandler INSTANCE;
//
//    BeebEventLoadDiscImageHandler():
//        BeebEventHandler(0)
//    {
//    }
//
//    BeebEventData Clone(const BeebEventData &src) const override {
//        BeebEventData clone;
//
//        clone.load_disc_image=new BeebEventLoadDiscImageData;
//
//        clone.load_disc_image->drive=src.load_disc_image->drive;
//        clone.load_disc_image->disc_image=DiscImage::Clone(src.load_disc_image->disc_image);
//        clone.load_disc_image->hash=src.load_disc_image->hash;
//
//        return clone;
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        BeebEventLoadDiscImageData *d=e->data.load_disc_image;
//
//        log->f("Drive %d: ",d->drive);
//
//        if(!d->disc_image) {
//            log->f("*no image*");
//        } else {
//            std::string hash=d->disc_image->GetHash();
//            ASSERT(hash==d->hash);
//            log->f("%s (hash: %s)",d->disc_image->GetName().c_str(),hash.c_str());
//        }
//    }
//protected:
//    void HandleDestroy(BeebEventData *data) const override {
//        delete data->load_disc_image;
//        data->load_disc_image=nullptr;
//    }
//private:
//};
//
//const BeebEventLoadDiscImageHandler BeebEventLoadDiscImageHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventConfigHandler:
//    public BeebEventHandler
//{
//public:
//    explicit BeebEventConfigHandler(uint32_t flags):
//        BeebEventHandler(flags)
//    {
//    }
//
//    BeebEventData Clone(const BeebEventData &src) const override {
//        BeebEventData clone;
//
//        clone.config=new BeebEventConfigData(*src.config);
//
//        return clone;
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        BeebEventConfigData *d=e->data.config;
//
//        log->f("Config: %s",d->config.config.name.c_str());
//    }
//protected:
//    void HandleDestroy(BeebEventData *data) const override {
//        delete data->config;
//        data->config=nullptr;
//    }
//private:
//};
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventChangeConfigHandler:
//    public BeebEventConfigHandler
//{
//public:
//    static const BeebEventChangeConfigHandler INSTANCE;
//
//    BeebEventChangeConfigHandler():
//        BeebEventConfigHandler(0)
//    {
//    }
//protected:
//private:
//};
//
//const BeebEventChangeConfigHandler BeebEventChangeConfigHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventRootHandler:
//    public BeebEventConfigHandler
//{
//public:
//    static const BeebEventRootHandler INSTANCE;
//
//    BeebEventRootHandler():
//        BeebEventConfigHandler(BeebEventTypeFlag_ShownInUI|BeebEventTypeFlag_Start)
//    {
//    }
//protected:
//private:
//};
//
//const BeebEventRootHandler BeebEventRootHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventHardResetHandler:
//    public BeebEventHandler
//{
//public:
//    static const BeebEventHardResetHandler INSTANCE;
//
//    BeebEventHardResetHandler():
//        BeebEventHandler(0)
//    {
//    }
//
//    BeebEventData Clone(const BeebEventData &src) const override {
//        BeebEventData clone={};
//
//        if(src.hard_reset) {
//            clone.hard_reset=new BeebEventHardResetData(*src.hard_reset);
//        }
//
//        return clone;
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        log->f("Config: %s; boot=%s",e->data.hard_reset->loaded_config.config.name.c_str(),BOOL_STR(e->data.hard_reset->boot));
//    }
//protected:
//    void HandleDestroy(BeebEventData *data) const override {
//        delete data->hard_reset;
//        data->hard_reset=nullptr;
//    }
//private:
//};
//
//const BeebEventHardResetHandler BeebEventHardResetHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_TURBO_DISC
//class BeebEventSetTurboDiscHandler:
//    public BeebEventValueHandler
//{
//public:
//    static const BeebEventSetTurboDiscHandler INSTANCE;
//
//    BeebEventSetTurboDiscHandler():
//        BeebEventValueHandler(0)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        log->f("Turbo: %s\n",BOOL_STR(e->data.set_turbo_disc.turbo));
//    }
//protected:
//private:
//};
//
//const BeebEventSetTurboDiscHandler BeebEventSetTurboDiscHandler::INSTANCE;
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventStateHandler:
//    public BeebEventHandler
//{
//public:
//    BeebEventStateHandler(uint32_t flags):
//        BeebEventHandler(flags)
//    {
//    }
//
//    BeebEventData Clone(const BeebEventData &src) const override {
//        BeebEventData clone={};
//
//        if(src.state) {
//            clone.state=new BeebEventStateData(*src.state);
//        }
//
//        return clone;
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        const BeebEventStateData *d=e->data.state;
//
//        log->f("(BeebState *)%p (%s)",(void *)d->state.get(),d->state->GetName().c_str());
//    }
//protected:
//    void HandleDestroy(BeebEventData *data) const override {
//        delete data->state;
//        data->state=nullptr;
//    }
//private:
//};
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventLoadStateHandler:
//    public BeebEventStateHandler
//{
//public:
//    static const BeebEventLoadStateHandler INSTANCE;
//
//    BeebEventLoadStateHandler():
//        BeebEventStateHandler(BeebEventTypeFlag_ChangesTimeline|BeebEventTypeFlag_Start|BeebEventTypeFlag_Synthetic)
//    {
//    }
//protected:
//private:
//};
//
//const BeebEventLoadStateHandler BeebEventLoadStateHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventSaveStateHandler:
//    public BeebEventStateHandler
//{
//public:
//    static const BeebEventSaveStateHandler INSTANCE;
//
//    BeebEventSaveStateHandler():
//        BeebEventStateHandler(BeebEventTypeFlag_ChangesTimeline|BeebEventTypeFlag_ShownInUI|BeebEventTypeFlag_CanDelete)
//    {
//    }
//protected:
//private:
//};
//
//const BeebEventSaveStateHandler BeebEventSaveStateHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventWindowProxyHandler:
//    public BeebEventValueHandler
//{
//public:
//    static const BeebEventWindowProxyHandler INSTANCE;
//
//    BeebEventWindowProxyHandler():
//        BeebEventValueHandler(BeebEventTypeFlag_ChangesTimeline|BeebEventTypeFlag_Synthetic)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        log->f("SDL_Window ID: %" PRIu32,e->data.window_proxy.sdl_window_id);
//    }
//protected:
//private:
//};
//
//const BeebEventWindowProxyHandler BeebEventWindowProxyHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventStartPasteHandler:
//    public BeebEventHandler
//{
//public:
//    static const BeebEventStartPasteHandler INSTANCE;
//
//    BeebEventStartPasteHandler():
//        BeebEventHandler(0)
//    {
//    }
//
//    BeebEventData Clone(const BeebEventData &src) const override {
//        BeebEventData clone={};
//
//        if(src.paste) {
//            clone.paste=new BeebEventPasteData(*src.paste);
//        }
//
//        return clone;
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        log->f("Paste %zu bytes: ",e->data.paste->text->size());
//
//        static const size_t MAX=20;
//
//        bool in=false;
//
//        for(size_t i=0;i<(std::min)(e->data.paste->text->size(),MAX);++i) {
//            char c=e->data.paste->text->at(i);
//
//            if(c>=32&&c<=126) {
//                if(!in) {
//                    if(i>0) {
//                        log->f("+");
//                    }
//
//                    log->f("\"");
//                    in=true;
//                }
//
//                log->f("%c",c);
//            } else {
//                if(in) {
//                    log->f("\"");
//                    in=false;
//                }
//
//                if(i>0) {
//                    log->f("+");
//                }
//
//                log->f("CHR$%d",c);
//            }
//        }
//
//        if(in) {
//            log->f("\"");
//        }
//
//        if(e->data.paste->text->size()>MAX) {
//            log->f("+...");
//        }
//    }
//protected:
//    void HandleDestroy(BeebEventData *data) const override {
//        delete data->paste;
//        data->paste=nullptr;
//    }
//private:
//};
//
//const BeebEventStartPasteHandler BeebEventStartPasteHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//class BeebEventStopPasteHandler:
//    public BeebEventValueHandler
//{
//public:
//    static const BeebEventStopPasteHandler INSTANCE;
//
//    BeebEventStopPasteHandler():
//        BeebEventValueHandler(0)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        (void)e;
//
//        log->f("Stop Paste");
//    }
//protected:
//private:
//};
//
//const BeebEventStopPasteHandler BeebEventStopPasteHandler::INSTANCE;
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//class BeebEventSetByteHandler:
//    public BeebEventValueHandler
//{
//public:
//    static const BeebEventSetByteHandler INSTANCE;
//
//    BeebEventSetByteHandler():
//        BeebEventValueHandler(0)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        (void)e;
//
//        log->f("address=$%04x, value=$%02x",e->data.set_byte.address,e->data.set_byte.value);
//    }
//protected:
//private:
//};
//
//const BeebEventSetByteHandler BeebEventSetByteHandler::INSTANCE;
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//class BeebEventSetBytesHandler:
//    public BeebEventHandler
//{
//public:
//    static const BeebEventSetBytesHandler INSTANCE;
//
//    BeebEventSetBytesHandler():
//        BeebEventHandler(0)
//    {
//    }
//
//    BeebEventData Clone(const BeebEventData &src) const override {
//        BeebEventData clone={};
//
//        if(src.set_bytes) {
//            clone.set_bytes=new BeebEventSetBytesData(*src.set_bytes);
//        }
//
//        return clone;
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        (void)e;
//
//        log->f("address=$%08x, %zu byte(s)",e->data.set_bytes->address,e->data.set_bytes->values.size());
//    }
//protected:
//    void HandleDestroy(BeebEventData *data) const override {
//        delete data->set_bytes;
//        data->set_bytes=nullptr;
//    }
//private:
//};
//
//const BeebEventSetBytesHandler BeebEventSetBytesHandler::INSTANCE;
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//class BeebEventSetExtByteHandler:
//        public BeebEventValueHandler
//{
//public:
//    static const BeebEventSetExtByteHandler INSTANCE;
//
//    BeebEventSetExtByteHandler():
//            BeebEventValueHandler(0)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        (void)e;
//
//        log->f("address=$%08x, value=$%02x",e->data.set_byte.address,e->data.set_byte.value);
//    }
//protected:
//private:
//};
//
//const BeebEventSetExtByteHandler BeebEventSetExtByteHandler::INSTANCE;
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//class BeebEventDebugAsyncCallHandler:
//    public BeebEventValueHandler
//{
//public:
//    static const BeebEventDebugAsyncCallHandler INSTANCE;
//
//    BeebEventDebugAsyncCallHandler():
//        BeebEventValueHandler(0)
//    {
//    }
//
//    void Dump(const BeebEvent *e,Log *log) const override {
//        log->f("Address: $%04x, A=%u ($%02x), X=%u ($%02x), Y=%u ($%02x), C=%s",
//               e->data.debug_async_call.addr,
//               e->data.debug_async_call.a,e->data.debug_async_call.a,
//               e->data.debug_async_call.x,e->data.debug_async_call.x,
//               e->data.debug_async_call.y,e->data.debug_async_call.y,
//               BOOL_STR(e->data.debug_async_call.c));
//    }
//protected:
//private:
//};
//
//const BeebEventDebugAsyncCallHandler BeebEventDebugAsyncCallHandler::INSTANCE;
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//const BeebEventHandler *BeebEventHandler::handlers[256]={
//#define BEEB_EVENT_TYPE(X) &BeebEvent##X##Handler::INSTANCE,
//#include "beeb_events_types.inl"
//};
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeNone(uint64_t time) {
//    return BeebEvent{BeebEventType_None,time,{}};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeKeyState(uint64_t time,BeebKey key,uint8_t state) {
//    BeebEventData data={};
//
//    data.key_state.key=key;
//    data.key_state.state=state;
//
//    return BeebEvent{BeebEventType_KeyState,time,data};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeLoadDiscImage(uint64_t time,int drive,std::shared_ptr<const DiscImage> disc_image) {
//    BeebEventData data={};
//
//    data.load_disc_image=new BeebEventLoadDiscImageData;
//
//    ASSERT(drive>=0&&drive<NUM_DRIVES);
//    data.load_disc_image->drive=(uint8_t)drive;
//    data.load_disc_image->disc_image=std::move(disc_image);
//
//    if(data.load_disc_image->disc_image) {
//        data.load_disc_image->hash=data.load_disc_image->disc_image->GetHash();
//    }
//
//    return BeebEvent{BeebEventType_LoadDiscImage,time,data};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeHardReset(uint64_t time,BeebLoadedConfig loaded_config,bool boot) {
//    BeebEventData data={};
//
//    data.hard_reset=new BeebEventHardResetData;
//
//    data.hard_reset->loaded_config=std::move(loaded_config);
//    data.hard_reset->boot=boot;
//
//    return BeebEvent{BeebEventType_HardReset,time,data};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_TURBO_DISC
//BeebEvent BeebEvent::MakeSetTurboDisc(uint64_t time,bool turbo) {
//    BeebEventData data={};
//
//    data.set_turbo_disc.turbo=turbo;
//
//    return BeebEvent{BeebEventType_SetTurboDisc,time,data};
//}
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeLoadState(std::shared_ptr<BeebState> state) {
//    return MakeLoadOrSaveStateEvent(BeebEventType_LoadState,state->GetEmulated2MHzCycles(),state);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeSaveState(uint64_t time,std::shared_ptr<BeebState> state) {
//    return MakeLoadOrSaveStateEvent(BeebEventType_SaveState,time,state);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeWindowProxy(BeebWindow *window) {
//    BeebEventData data={};
//
//    data.window_proxy.sdl_window_id=window->GetSDLWindowID();
//
//    std::shared_ptr<BeebThread> thread=window->GetBeebThread();
//
//    uint64_t time=thread->GetEmulated2MHzCycles();
//
//    return BeebEvent{BeebEventType_WindowProxy,time,data};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeStartPaste(uint64_t time_2MHz_cycles,std::shared_ptr<std::string> text) {
//    BeebEventData data={};
//
//    data.paste=new BeebEventPasteData;
//
//    data.paste->text=std::move(text);
//
//    return BeebEvent{BeebEventType_StartPaste,time_2MHz_cycles,data};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeStopPaste(uint64_t time_2MHz_cycles) {
//    BeebEventData data={};
//
//    return BeebEvent{BeebEventType_StopPaste,time_2MHz_cycles,data};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//BeebEvent BeebEvent::MakeSetByte(uint64_t time_2MHz_cycles,uint16_t address,uint8_t value) {
//    BeebEventData data={};
//
//    data.set_byte.address=address;
//    data.set_byte.value=value;
//
//    return BeebEvent{BeebEventType_SetByte,time_2MHz_cycles,data};
//}
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//BeebEvent BeebEvent::MakeSetBytes(uint64_t time_2MHz_cycles,uint32_t address,std::vector<uint8_t> values) {
//    BeebEventData data={};
//
//    data.set_bytes=new BeebEventSetBytesData;
//
//    data.set_bytes->address=address;
//    data.set_bytes->values=std::move(values);
//
//    return BeebEvent{BeebEventType_SetBytes,time_2MHz_cycles,data};
//}
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//BeebEvent BeebEvent::MakeSetExtByte(uint64_t time_2MHz_cycles,uint32_t address,uint8_t value) {
//    BeebEventData data={};
//
//    data.set_ext_byte.address=address;
//    data.set_ext_byte.value=value;
//
//    return BeebEvent{BeebEventType_SetExtByte,time_2MHz_cycles,data};
//}
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//#if BBCMICRO_DEBUGGER
//BeebEvent BeebEvent::MakeAsyncCall(uint64_t time_2MHz_cycles,uint16_t addr,uint8_t a,uint8_t x,uint8_t y,bool c) {
//    BeebEventData data={};
//
//    data.debug_async_call.addr=addr;
//    data.debug_async_call.a=a;
//    data.debug_async_call.x=x;
//    data.debug_async_call.y=y;
//    data.debug_async_call.c=c;
//
//    return BeebEvent{BeebEventType_DebugAsyncCall,time_2MHz_cycles,data};
//}
//#endif
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent::~BeebEvent() {
//    ASSERT(BeebEventHandler::handlers[this->type]);
//    BeebEventHandler::handlers[this->type]->Destroy(this);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent::BeebEvent(BeebEvent &&oth) {
//    this->Move(&oth);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent &BeebEvent::operator=(BeebEvent &&oth) {
//    if(&oth!=this) {
//        ASSERT(BeebEventHandler::handlers[this->type]);
//        BeebEventHandler::handlers[this->type]->Destroy(this);
//
//        this->Move(&oth);
//    }
//
//    return *this;
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent::BeebEvent(const BeebEvent &src) {
//    this->Copy(src);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent &BeebEvent::operator=(const BeebEvent &src) {
//    if(&src!=this) {
//        ASSERT(BeebEventHandler::handlers[this->type]);
//        BeebEventHandler::handlers[this->type]->Destroy(this);
//
//        this->Copy(src);
//    }
//
//    return *this;
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//void BeebEvent::Dump(Log *log) const {
//    log->f("BeebEvent: ");
//    LogIndenter indent(log);
//    log->f("Type: %s (%d; 0x%02X)\n",GetBeebEventTypeEnumName(this->type),this->type,this->type);
//
//    log->f("Time: %" PRIu64 "usec (%s)\n",this->time_2MHz_cycles,Get2MHzCyclesString(this->time_2MHz_cycles).c_str());
//    log->f("Data: ");
//    BeebEventHandler::handlers[this->type]->Dump(this,log);
//    log->f("\n");
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//void BeebEvent::DumpSummary(Log *log) const {
//    BeebEventHandler::handlers[this->type]->Dump(this,log);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//uint32_t BeebEvent::GetTypeFlags() const {
//    return BeebEventHandler::handlers[this->type]->flags;
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent::BeebEvent(uint8_t type_,uint64_t time_2MHz_cycles_,BeebEventData data_):
//    type(type_),
//    time_2MHz_cycles(time_2MHz_cycles_),
//    data(data_)
//{
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//void BeebEvent::Move(BeebEvent *oth) {
//    this->type=oth->type;
//    this->time_2MHz_cycles=oth->time_2MHz_cycles;
//    this->data=oth->data;
//
//    oth->type=BeebEventType_None;
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//void BeebEvent::Copy(const BeebEvent &src) {
//    ASSERT(BeebEventHandler::handlers[this->type]);
//
//    this->type=src.type;
//    this->time_2MHz_cycles=src.time_2MHz_cycles;
//    this->data=BeebEventHandler::handlers[this->type]->Clone(src.data);
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//
//BeebEvent BeebEvent::MakeLoadOrSaveStateEvent(BeebEventType type,uint64_t time,std::shared_ptr<BeebState> state) {
//    ASSERT(time==state->GetEmulated2MHzCycles());
//
//    BeebEventData data={};
//
//    data.state=new BeebEventStateData;
//
//    data.state->state=std::move(state);
//
//    return BeebEvent{(uint8_t)type,time,data};
//}
//
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
