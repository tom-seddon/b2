#include <shared/system.h>
#include "debugger.h"

#if BBCMICRO_DEBUGGER

#include "dear_imgui.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <shared/log.h>
#include <dear_imgui_hex_editor.h>

// Ugh, ugh, ugh.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <imgui_memory_editor.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <unordered_map>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// if true, use dear_imgui_hex_editor_lib (newer, designed with b2 in mind)
// rather than imgui_memory_editor_lib (part of dear_imgui_club).
//
// In the long run, this flag will go away...
#define HEX_EDITOR_LIB 1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(DBG,"debugger","DBG   ",&log_printer_stdout_and_debugger,true)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct FnName {
    std::string name;
    std::vector<const char *> names;
};

static std::unordered_map<M6502Fn,FnName> g_name_by_6502_fn;

static const char *GetFnName(M6502Fn fn) {
    if(!fn) {
        return "NULL";
    } else {
        auto it=g_name_by_6502_fn.find(fn);
        if(it==g_name_by_6502_fn.end()) {
            return "?";
        } else {
            return it->second.name.c_str();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DebugUI:
    public SettingsUI
{
public:
    ~DebugUI() {
        for(int i=0;i<16;++i) {
            delete m_debug_big_pages[i];
        }
    }

    bool OnClose() override {
        return false;
    }

    void SetBeebThread(std::shared_ptr<BeebThread> beeb_thread) {
        ASSERT(!m_beeb_thread);
        m_beeb_thread=std::move(beeb_thread);
    }

    void DoImGui() final {
        for(size_t i=0;i<16;++i) {
            if(DebugBigPage *dbp=m_debug_big_pages[i]) {
                dbp->bp=nullptr;
                ++dbp->idle_count;
            }
        }

        this->HandleDoImGui();
    }
protected:
    struct DebugBigPage {
        const BBCMicro::BigPage *bp=nullptr;

        // points to this->ram_buffer, or NULL.
        const uint8_t *r=nullptr;
        uint8_t *w=nullptr;

        // points to this->debug_buffer, or NULL.
        BBCMicro::DebugState::ByteDebugFlags *debug=nullptr;

        uint8_t ram_buffer[BBCMicro::BIG_PAGE_SIZE_BYTES]={};
        BBCMicro::DebugState::ByteDebugFlags debug_buffer[BBCMicro::BIG_PAGE_SIZE_BYTES]={};
        uint32_t idle_count=0;//eventually, for discarding...
    };

    std::shared_ptr<BeebThread> m_beeb_thread;
    uint32_t m_dpo=0;

    const DebugBigPage *GetDebugBigPageForAddress(M6502Word addr);

    bool ReadByte(uint8_t *value,
                  BBCMicro::DebugState::ByteDebugFlags *flags,
                  uint16_t addr);

    void DoDebugPageOverrideImGui();

    virtual void HandleDoImGui()=0;
private:
    DebugBigPage *m_debug_big_pages[16]={};

    void DoDebugPageOverrideFlagImGui(uint32_t mask,
                                      uint32_t current,
                                      const char *name,
                                      const char *popup_name,
                                      uint32_t override_flag,
                                      uint32_t flag);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const DebugUI::DebugBigPage *DebugUI::GetDebugBigPageForAddress(M6502Word addr) {
    DebugBigPage *dbp=m_debug_big_pages[addr.p.p];

    if(!dbp) {
        dbp=new DebugBigPage;
        m_debug_big_pages[addr.p.p]=dbp;
    }

    if(!dbp->bp) {
        dbp->idle_count=0;

        std::unique_lock<Mutex> lock;
        const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);

        dbp->bp=m->DebugGetBigPage(addr.b.h,m_dpo);
        ASSERT(dbp->bp);

        if(dbp->bp->r) {
            memcpy(dbp->ram_buffer,
                   dbp->bp->r,
                   BBCMicro::BIG_PAGE_SIZE_BYTES);
            dbp->r=dbp->ram_buffer;
        } else {
            dbp->r=nullptr;
        }

        if(dbp->bp->w&&dbp->bp->r) {
            dbp->w=dbp->ram_buffer;
        } else {
            dbp->w=nullptr;
        }

        if(dbp->bp->debug) {
            memcpy(dbp->debug_buffer,
                   dbp->bp->debug,
                   BBCMicro::BIG_PAGE_SIZE_BYTES*sizeof(BBCMicro::DebugState::ByteDebugFlags));
            dbp->debug=dbp->debug_buffer;
        } else {
            dbp->debug=nullptr;
        }

    }

    return dbp;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DebugUI::ReadByte(uint8_t *value,
                       BBCMicro::DebugState::ByteDebugFlags *flags,
                       uint16_t addr_)
{
    M6502Word addr={addr_};

    const DebugBigPage *dbp=this->GetDebugBigPageForAddress(addr);

    if(!dbp) {
        return false;
    }

    if(!dbp->r) {
        return false;
    }

    *value=dbp->r[addr.p.o];

    if(flags) {
        if(dbp->debug) {
            *flags=dbp->debug[addr.p.o];
        } else {
            *flags={};
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//uint16_t DebugUI::GetDebugPage(uint16_t addr_) {
//    M6502Word addr={addr_};
//
//    this->PrepareForRead(addr);
//
//    return m_pages[addr.b.h]->flat_page;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BBCMicro::DebugState::ByteDebugFlags DebugUI::GetDebugFlags(uint16_t addr_) {
//    M6502Word addr={addr_};
//
//    this->PrepareForRead(addr);
//
//    return m_pages[addr.b.h]->debug[addr.b.l];
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::DoDebugPageOverrideImGui() {
    static const char ROM_POPUP[]="rom_popup";
    static const char SHADOW_POPUP[]="shadow_popup";
    static const char ANDY_POPUP[]="andy_popup";
    static const char HAZEL_POPUP[]="hazel_popup";
    static const char OS_POPUP[]="os_popup";

    uint32_t dpo_mask;
    uint32_t dpo_current;
    {
        std::unique_lock<Mutex> lock;
        const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
        dpo_mask=m->DebugGetPageOverrideMask();
        dpo_current=m->DebugGetCurrentPageOverride();
    }

    // ROM.
    if(dpo_mask&BBCMicroDebugPagingOverride_OverrideROM) {
        if(ImGui::Button("ROM")) {
            ImGui::OpenPopup(ROM_POPUP);
        }

        ImGui::SameLine();

        if(m_dpo&BBCMicroDebugPagingOverride_OverrideROM) {
            ImGui::Text("%x!",m_dpo&BBCMicroDebugPagingOverride_ROM);
        } else {
            ImGui::Text("%x",dpo_current&BBCMicroDebugPagingOverride_ROM);
        }

        if(ImGui::BeginPopup(ROM_POPUP)) {
            if(ImGui::Button("Use current")) {
                m_dpo&=~(uint32_t)BBCMicroDebugPagingOverride_OverrideROM;
                m_dpo&=~(uint32_t)BBCMicroDebugPagingOverride_ROM;
                ImGui::CloseCurrentPopup();
            }

            ImGui::Text("Force");

            for(uint8_t i=0;i<16;++i) {
                ImGui::SameLine();

                char text[10];
                snprintf(text,sizeof text,"%X",i);

                if(ImGui::Button(text)) {
                    m_dpo|=BBCMicroDebugPagingOverride_OverrideROM;
                    m_dpo=(m_dpo&~(uint32_t)BBCMicroDebugPagingOverride_ROM)|i;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
    }

    this->DoDebugPageOverrideFlagImGui(dpo_mask,dpo_current,"Shadow",SHADOW_POPUP,BBCMicroDebugPagingOverride_OverrideShadow,BBCMicroDebugPagingOverride_Shadow);

    this->DoDebugPageOverrideFlagImGui(dpo_mask,dpo_current,"ANDY",ANDY_POPUP,BBCMicroDebugPagingOverride_OverrideANDY,BBCMicroDebugPagingOverride_ANDY);

    this->DoDebugPageOverrideFlagImGui(dpo_mask,dpo_current,"HAZEL",HAZEL_POPUP,BBCMicroDebugPagingOverride_OverrideHAZEL,BBCMicroDebugPagingOverride_HAZEL);

    this->DoDebugPageOverrideFlagImGui(dpo_mask,dpo_current,"OS",OS_POPUP,BBCMicroDebugPagingOverride_OverrideOS,BBCMicroDebugPagingOverride_OS);
}

void DebugUI::DoDebugPageOverrideFlagImGui(uint32_t mask,
                                           uint32_t current,
                                           const char *name,
                                           const char *popup_name,
                                           uint32_t override_mask,
                                           uint32_t flag_mask)
{
    if(!(mask&override_mask)) {
        return;
    }

    ImGui::SameLine();

    if(ImGui::Button(name)) {
        ImGui::OpenPopup(popup_name);
    }

    ImGui::SameLine();

    if(m_dpo&override_mask) {
        ImGui::Text("%s!",m_dpo&flag_mask?"on":"off");
    } else {
        ImGui::Text("%s",current&flag_mask?"on":"off");
    }

    if(ImGui::BeginPopup(popup_name)) {
        if(ImGui::Button("Use current")) {
            m_dpo&=~override_mask;
            m_dpo&=~flag_mask;
            ImGui::CloseCurrentPopup();
        }

        if(ImGui::Button("Force on")) {
            m_dpo|=override_mask;
            m_dpo|=flag_mask;
            ImGui::CloseCurrentPopup();
        }

        if(ImGui::Button("Force off")) {
            m_dpo|=override_mask;
            m_dpo&=~flag_mask;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class DerivedType>
static std::unique_ptr<SettingsUI> CreateDebugUI(BeebWindow *beeb_window) {
    std::unique_ptr<DebugUI> ptr=std::make_unique<DerivedType>();

    ptr->SetBeebThread(beeb_window->GetBeebThread());

    return ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class M6502DebugWindow:
    public DebugUI
{
public:
    M6502DebugWindow() {
        if(g_name_by_6502_fn.empty()) {
            M6502_ForEachFn([](const char *name,M6502Fn fn,void *) {
                g_name_by_6502_fn[fn].names.push_back(name);
            },nullptr);

            for(auto &&it:g_name_by_6502_fn) {
                for(const std::string &name:it.second.names) {
                    if(!it.second.name.empty()) {
                        it.second.name+="/";
                    }

                    it.second.name+=name;
                }
            }
        }
    }
protected:
    void HandleDoImGui() override {
        //m_beeb_thread->SendUpdate6502StateMessage();

        bool halted;
        uint16_t pc,abus;
        uint8_t a,x,y,sp,opcode,read,dbus;
        M6502P p;
        M6502Fn tfn,ifn;
        const M6502Config *config;
        const volatile uint64_t *cycles;
        char halt_reason[1000];

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            const M6502 *s=m->GetM6502();

            config=s->config;
            halted=m->DebugIsHalted();
            if(const char *tmp=m->DebugGetHaltReason()) {
                strlcpy(halt_reason,tmp,sizeof halt_reason);
            } else {
                halt_reason[0]=0;
            }
            a=s->a;
            x=s->x;
            y=s->y;
            pc=s->opcode_pc.w;
            sp=s->s.b.l;
            read=s->read;
            abus=s->abus.w;
            dbus=s->dbus;
            p=M6502_GetP(s);
            tfn=s->tfn;
            ifn=s->ifn;
            opcode=M6502_GetOpcode(s);
            cycles=m->GetNum2MHzCycles();
        }

        this->Reg("A",a);
        this->Reg("X",x);
        this->Reg("Y",y);
        ImGui::Text("PC = $%04x",pc);
        ImGui::Text("S = $01%02X",sp);
        const char *mnemonic=config->disassembly_info[opcode].mnemonic;
        const char *mode_name=M6502AddrMode_GetName(config->disassembly_info[opcode].mode);

        char pstr[9];
        ImGui::Text("P = $%02x %s",p.value,M6502P_GetString(pstr,p));

        char cycles_str[MAX_UINT64_THOUSANDS_LEN];
        GetThousandsString(cycles_str,*cycles);

        ImGui::Separator();

        ImGui::Text("Cycles = %s",cycles_str);
        ImGui::Text("Opcode = $%02X %03d - %s %s",opcode,opcode,mnemonic,mode_name);
        ImGui::Text("tfn = %s",GetFnName(tfn));
        ImGui::Text("ifn = %s",GetFnName(ifn));
        if(halted) {
            if(halt_reason[0]==0) {
                ImGui::TextUnformatted("State = halted");
            } else {
                ImGui::Text("State = halted: %s",halt_reason);
            }
        } else {
            ImGui::TextUnformatted("State = running");
        }

        ImGui::Text("Address = $%04x; Data = $%02x %03d %s",abus,dbus,dbus,BINARY_BYTE_STRINGS[dbus]);
        ImGui::Text("Access = %s",M6502ReadType_GetName(read));
    }
private:
    void Reg(const char *name,uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s",name,value,value,BINARY_BYTE_STRINGS[value]);
    }
};

std::unique_ptr<SettingsUI> Create6502DebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<M6502DebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if HEX_EDITOR_LIB

LOG_DEFINE(HEXEDIT,"HEXEDIT",&log_printer_stdout_and_debugger,true)

class MemoryDebugWindow:
public DebugUI
{
public:
    MemoryDebugWindow():
    m_handler(this),
    m_hex_editor(&m_handler)
    {
    }
protected:
    void HandleDoImGui() override {
        this->DoDebugPageOverrideImGui();

        m_hex_editor.DoImGui();
    }
private:
    class Handler:
    public HexEditorHandler
    {
    public:
        explicit Handler(MemoryDebugWindow *window):
        m_window(window)
        {
        }

        void ReadByte(HexEditorByte *byte,size_t offset) override {
            M6502Word addr={(uint16_t)offset};

            const DebugBigPage *dbp=m_window->GetDebugBigPageForAddress(addr);

            if(!dbp||!dbp->r) {
                byte->got_value=false;
            } else {
                byte->got_value=true;
                byte->value=dbp->r[addr.p.o];
                byte->can_write=!!dbp->w;
            }
        }

        void WriteByte(size_t offset,uint8_t value) override {
            std::vector<uint8_t> data;
            data.resize(1);
            data[0]=value;

            m_window->m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetBytesMessage>((uint32_t)offset,
                                                                                             m_window->m_dpo,
                                                                                             std::move(data)));
        }

        size_t GetSize() override {
            return 65536;
        }

        uintptr_t GetBaseAddress() override {
            return 0;
        }

        void DebugPrint(const char *fmt,...) override {
            va_list v;

            va_start(v,fmt);
            LOGV(HEXEDIT,fmt,v);
            va_end(v);
        }
    protected:
    private:
        MemoryDebugWindow *const m_window;
    };

    Handler m_handler;
    HexEditor m_hex_editor;
};

#else

class MemoryDebugWindow:
    public DebugUI
{
public:
    MemoryDebugWindow() {
        m_memory_editor.ReadFn=&MemoryEditorRead;
        m_memory_editor.WriteFn=&MemoryEditorWrite;
    }
protected:
    void HandleDoImGui() override {
        m_memory_editor.DrawContents((uint8_t *)this,65536,0);
    }
private:
    MemoryEditor m_memory_editor;

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static uint8_t MemoryEditorRead(uint8_t *data,size_t off) {
        auto self=(MemoryDebugWindow *)data;

        ASSERT((uint16_t)off==off);

        uint8_t value;
        self->ReadByte(&value,nullptr,nullptr,(uint16_t)off);
        return value;
    }

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static void MemoryEditorWrite(uint8_t *data,size_t off,uint8_t d) {
        auto self=(MemoryDebugWindow *)data;

        self->m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetByteMessage>((uint16_t)off,0,d));
    }
};

#endif

std::unique_ptr<SettingsUI> CreateMemoryDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<MemoryDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ExtMemoryDebugWindow:
    public DebugUI
{
public:
    ExtMemoryDebugWindow() {
        m_memory_editor.ReadFn=&MemoryEditorRead;
        m_memory_editor.WriteFn=&MemoryEditorWrite;
    }
protected:
    void HandleDoImGui() override {
        bool enabled;
        uint8_t l,h;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);

            if(const ExtMem *s=m->DebugGetExtMem()) {
                enabled=true;
                l=s->GetAddressL();
                h=s->GetAddressH();
            } else {
                enabled=false;
                h=l=0;//inhibit spurious unused variable warning.
            }
        }

        if(enabled) {
            this->Reg("L",l);
            this->Reg("H",h);

            m_memory_editor.DrawContents((uint8_t *)this,16777216,0);
        } else {
            ImGui::Text("External memory disabled");
        }
    }
private:
    MemoryEditor m_memory_editor;

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static uint8_t MemoryEditorRead(const MemoryEditor::u8 *data,size_t off) {
        auto self=(ExtMemoryDebugWindow *)data;

        ASSERT((uint32_t)off==off);

        std::unique_lock<Mutex> lock;
        const BBCMicro *m=self->m_beeb_thread->LockBeeb(&lock);
        const ExtMem *s=m->DebugGetExtMem();
        return ExtMem::ReadMemory(s,(uint32_t)off);
    }

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static void MemoryEditorWrite(MemoryEditor::u8 *data,size_t off,uint8_t d) {
        auto self=(ExtMemoryDebugWindow *)data;

        self->m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetExtByteMessage>((uint32_t)off,d));
    }

    void Reg(const char *name,uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s",name,value,value,BINARY_BYTE_STRINGS[value]);
    }
};

std::unique_ptr<SettingsUI> CreateExtMemoryDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<ExtMemoryDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DisassemblyDebugWindow:
    public DebugUI
{
public:
    uint32_t GetExtraImGuiWindowFlags() const override {
        // The bottom line of the disassembly should just be clipped
        // if it runs off the bottom... only drawing whole lines just
        // looks weird. But when that happens, dear imgui
        // automatically adds a scroll bar. And that's even weirder.
        return ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
    }

    const CommandTable *GetCommandTable() const override {
        return &ms_command_table;
    }
protected:
    void HandleDoImGui() override {
        CommandContext cc(this,this->GetCommandTable());

        const M6502Config *config;
        uint16_t pc;
        uint8_t x,y;
        bool has_debug_state;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            const M6502 *s=m->GetM6502();

            config=s->config;
            pc=s->opcode_pc.w;
            x=s->x;
            y=s->y;
            has_debug_state=m->HasDebugState();
        }

        float maxY=ImGui::GetCurrentWindow()->Size.y;//-ImGui::GetTextLineHeight()-GImGui->Style.WindowPadding.y*2.f;

        this->DoDebugPageOverrideImGui();

        cc.DoToggleCheckboxUI("toggle_track_pc");

        cc.DoButton("go_back");

        if(ImGui::InputText("Address",
                            m_address_text,sizeof m_address_text,
                            ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll))
        {
            char *ep;
            unsigned long address=strtoul(m_address_text,&ep,16);
            if(*ep==0||(isspace(*ep)&&address<65536)) {
                this->GoTo((uint16_t)address);
            }
        }

        if(m_track_pc) {
            m_addr=pc;
        }

        m_num_lines=0;
        uint16_t addr=m_addr;
        while(ImGui::GetCursorPosY()<=maxY) {
            ++m_num_lines;
            M6502Word line_addr={addr};
            //m_line_addrs.push_back(line_addr.w);

            ImGuiIDPusher id_pusher(addr);

            uint8_t opcode;
            BBCMicro::DebugState::ByteDebugFlags debug_flags;
            this->ReadByte(&opcode,&debug_flags,addr++);

            const M6502DisassemblyInfo *di=&config->disassembly_info[opcode];

            M6502Word operand={};
            if(di->num_bytes>=2) {
                this->ReadByte(&operand.b.l,nullptr,addr++);
            }
            if(di->num_bytes>=3) {
                this->ReadByte(&operand.b.h,nullptr,addr++);
            }

            ImGuiStyleColourPusher pusher;

            if(line_addr.w==pc) {
                pusher.Push(ImGuiCol_Text,ImVec4(1.f,1.f,0.f,1.f));
            }

            if(has_debug_state) {
                bool break_execute=!!debug_flags.bits.break_execute;
                if(ImGui::Checkbox("",&break_execute)) {
                    debug_flags.bits.break_execute=break_execute;

//                    std::unique_lock<Mutex> lock;
//                    BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);
//
//                    m->DebugSetByteFlags(line_addr,debug_flags);
                }

                ImGui::SameLine();
            }

//            char prefix[3];
//            if(has_debug_state) {
//                prefix[0]=dbp->bp->code;
//                prefix[1]='.';
//                prefix[2]=0;
//            } else {
//                prefix[0]=0;
//            }

            ImGui::Text("%04x  %c%c %c%c %c%c  %c%c%c  %s ",
                        line_addr.w,
                        HEX_CHARS_LC[opcode>>4&15],
                        HEX_CHARS_LC[opcode&15],
                        di->num_bytes>=2?HEX_CHARS_LC[operand.b.l>>4&15]:' ',
                        di->num_bytes>=2?HEX_CHARS_LC[operand.b.l&15]:' ',
                        di->num_bytes>=3?HEX_CHARS_LC[operand.b.h>>4&15]:' ',
                        di->num_bytes>=3?HEX_CHARS_LC[operand.b.h&15]:' ',
                        opcode>=32&&opcode<127?opcode:' ',
                        operand.b.l>=32&&operand.b.l<127?operand.b.l:' ',
                        operand.b.h>=32&&operand.b.h<127?operand.b.h:' ',
                        di->mnemonic);

            switch(di->mode) {
            default:
                ASSERT(0);
                // fall through
            case M6502AddrMode_IMP:
                break;

            case M6502AddrMode_REL:
                {
                    M6502Word dest;
                    dest.w=addr+(uint16_t)(int16_t)(int8_t)operand.b.l;

                    this->AddWord("$",dest.w,"");
                }
                break;

            case M6502AddrMode_IMM:
                this->AddByte("#$",operand.b.l,"");
                break;

            case M6502AddrMode_ZPG:
                this->AddByte("$",operand.b.l,"");
                break;

            case M6502AddrMode_ZPX:
                this->AddByte("$",operand.b.l,",X");
                this->AddByte(IND_PREFIX,operand.b.l+x,"");
                break;

            case M6502AddrMode_ZPY:
                this->AddByte("$",operand.b.l,",Y");
                this->AddByte(IND_PREFIX,operand.b.l+y,"");
                break;

            case M6502AddrMode_ABS:
                this->AddWord("$",operand.w,"");
                break;

            case M6502AddrMode_ABX:
                this->AddWord("$",operand.w,",X");
                this->AddWord(IND_PREFIX,operand.w+x,"");
                break;

            case M6502AddrMode_ABY:
                this->AddWord("$",operand.w,",Y");
                this->AddWord(IND_PREFIX,operand.w+y,"");
                break;

            case M6502AddrMode_INX:
                this->AddByte("($",operand.b.l,",X)");
                this->DoIndirect((operand.b.l+x)&0xff,0xff,0);
                break;

            case M6502AddrMode_INY:
                this->AddByte("($",operand.b.l,"),Y");
                this->DoIndirect(operand.b.l,0xff,y);
                break;

            case M6502AddrMode_IND:
                this->AddWord("($",operand.w,")");
                // doesn't handle the 6502 page crossing bug...
                this->DoIndirect(operand.w,0xffff,0);
                break;

            case M6502AddrMode_ACC:
                ImGui::SameLine();
                ImGui::TextUnformatted("A");
                break;

            case M6502AddrMode_INZ:
                this->AddByte("($",operand.b.l,")");
                this->DoIndirect(operand.b.l,0xff,0);
                break;

            case M6502AddrMode_INDX:
                this->AddWord("($",operand.w,",X)");
                this->DoIndirect(operand.w+x,0xffff,0);
                break;
            }
        }

        if(ImGui::IsWindowFocused()) {
            ImGuiIO& io=ImGui::GetIO();

            m_wheel+=io.MouseWheel;

            int wheel=(int)m_wheel;

            m_wheel-=(float)wheel;

            wheel*=5;//wild guess...

            if(wheel<0) {
                this->Down(config,-wheel);
            } else if(wheel>0) {
                this->Up(config,wheel);
            }
        }
    }
private:
    static const char IND_PREFIX[];

    uint16_t m_addr=0;
    bool m_track_pc=true;
    char m_address_text[100]={};
    //std::vector<uint16_t> m_line_addrs;
    std::vector<uint16_t> m_history;
    int m_num_lines=0;
    //char m_disassembly_text[100];
    float m_wheel=0;

    void DoIndirect(uint16_t address,uint16_t mask,uint16_t post_index) {
        M6502Word addr;

        this->ReadByte(&addr.b.l,nullptr,address);

        ++address;
        address&=mask;

        this->ReadByte(&addr.b.h,nullptr,address);

        addr.w+=post_index;

        this->AddWord(IND_PREFIX,addr.w,"");
    }

    void AddWord(const char *prefix,uint16_t w,const char *suffix) {
        char label[5]={
            HEX_CHARS_LC[w>>12&15],
            HEX_CHARS_LC[w>>8&15],
            HEX_CHARS_LC[w>>4&15],
            HEX_CHARS_LC[w&15],
        };

        this->DoClickableAddress(prefix,label,suffix,w);
    }

    void AddByte(const char *prefix,uint8_t value,const char *suffix) {
        char label[3]={
            HEX_CHARS_LC[value>>4&15],
            HEX_CHARS_LC[value&15],
        };

        this->DoClickableAddress(prefix,label,suffix,value);
    }

    void DoClickableAddress(const char *prefix,const char *label,const char *suffix,uint16_t address) {
        if(prefix[0]!=0) {
            ImGui::SameLine(0.f,0.f);
            ImGui::TextUnformatted(prefix);
        }

        ImGui::SameLine(0.f,0.f);

        {
            // No point using SmallButton - it doesn't set the
            // horizontal frame padding to 0.

            ImGuiStyleVarPusher pusher(ImGuiStyleVar_FramePadding,ImVec2(0.f,0.f));

            if(ImGui::ButtonEx(label,ImVec2(0.f,0.f),ImGuiButtonFlags_AlignTextBaseLine)) {
                this->GoTo(address);
            }
        }

        if(suffix[0]!=0) {
            ImGui::SameLine(0.f,0.f);
            ImGui::TextUnformatted(suffix);
        }
    }

    void GoTo(uint16_t address) {
        if(m_history.empty()||m_addr!=m_history.back()) {
            m_history.push_back(m_addr);
        }
        m_track_pc=false;
        m_addr=address;
    }

    bool IsTrackingPC() const {
        return m_track_pc;
    }

    void ToggleTrackPC() {
        m_track_pc=!m_track_pc;
    }

    bool IsBackEnabled() const {
        return !m_history.empty();
    }

    void Back() {
        if(!m_history.empty()) {
            m_track_pc=false;
            m_addr=m_history.back();
            m_history.pop_back();
        }
    }

    bool IsMoveEnabled() const {
        if(m_track_pc) {
            return false;
        }

        //if(m_line_addrs.empty()) {
        //    return false;
        //}

        return true;
    }

    void PageUp() {
        const M6502Config *config=this->Get6502Config();

        this->Up(config,m_num_lines-2);
    }

    void PageDown() {
        const M6502Config *config=this->Get6502Config();

        this->Down(config,m_num_lines-2);
    }

    void Up() {
        const M6502Config *config=this->Get6502Config();

        this->Up(config,1);
    }

    void Down() {
        const M6502Config *config=this->Get6502Config();

        this->Down(config,1);
    }

    void Up(const M6502Config *config,int n) {
        for(int i=0;i<n;++i) {
            uint8_t opcode;

            this->ReadByte(&opcode,nullptr,m_addr-1);
            if(config->disassembly_info[opcode].num_bytes==1) {
                --m_addr;
                continue;
            }

            this->ReadByte(&opcode,nullptr,m_addr-2);
            if(config->disassembly_info[opcode].num_bytes==2) {
                m_addr-=2;
                continue;
            }

            m_addr-=3;
        }
    }

    void Down(const M6502Config *config,int n) {
        for(int i=0;i<n;++i) {
            uint8_t opcode;
            this->ReadByte(&opcode,nullptr,m_addr);
            m_addr+=config->disassembly_info[opcode].num_bytes;
        }
    }

    const M6502Config *Get6502Config() {
        std::unique_lock<Mutex> lock;
        const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
        const M6502 *s=m->GetM6502();

        return s->config;
    }

    static ObjectCommandTable<DisassemblyDebugWindow> ms_command_table;
};

const char DisassemblyDebugWindow::IND_PREFIX[]=" --> $";

ObjectCommandTable<DisassemblyDebugWindow> DisassemblyDebugWindow::ms_command_table("Disassembly Window",{
    {CommandDef("toggle_track_pc","Track PC").Shortcut(SDLK_t),&DisassemblyDebugWindow::ToggleTrackPC,&DisassemblyDebugWindow::IsTrackingPC,nullptr},
    {CommandDef("back","Back").Shortcut(SDLK_BACKSPACE),&DisassemblyDebugWindow::Back,nullptr,&DisassemblyDebugWindow::IsBackEnabled},
    {CommandDef("up","Up").Shortcut(SDLK_UP),&DisassemblyDebugWindow::Up,&DisassemblyDebugWindow::IsMoveEnabled},
    {CommandDef("down","Down").Shortcut(SDLK_DOWN),&DisassemblyDebugWindow::Down,&DisassemblyDebugWindow::IsMoveEnabled},
    {CommandDef("page_up","Page Up").Shortcut(SDLK_PAGEUP),&DisassemblyDebugWindow::PageUp,&DisassemblyDebugWindow::IsMoveEnabled},
    {CommandDef("page_down","Page Down").Shortcut(SDLK_PAGEDOWN),&DisassemblyDebugWindow::PageDown,&DisassemblyDebugWindow::IsMoveEnabled},
});

std::unique_ptr<SettingsUI> CreateDisassemblyDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<DisassemblyDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CRTCDebugWindow:
    public DebugUI
{
public:
protected:
    void HandleDoImGui() override {
        CRTC::Registers registers;
        uint8_t address;
        uint16_t cursor_address;
        uint16_t display_address;
        BBCMicro::AddressableLatch latch;

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);

            const CRTC *c=m->DebugGetCRTC();
            //const VideoULA *u=m->DebugGetVideoULA();

            registers=c->m_registers;
            address=c->m_address;

            cursor_address=m->DebugGetBeebAddressFromCRTCAddress(registers.bits.cursorh,registers.bits.cursorl);
            display_address=m->DebugGetBeebAddressFromCRTCAddress(registers.bits.addrh,registers.bits.addrl);

            latch=m->DebugGetAddressableLatch();

            //ucontrol=u->control;
            //memcpy(upalette,u->m_palette,16);
        }

        if(ImGui::CollapsingHeader("Register Values")) {
            ImGui::Text("Address = $%02x %03u",address,address);
            for(size_t i=0;i<18;++i) {
                ImGui::Text("R%zu = $%02x %03u %s",i,registers.values[i],registers.values[i],BINARY_BYTE_STRINGS[registers.values[i]]);
            }
            ImGui::Separator();
        }

        ImGui::Text("H Displayed = %u, Total = %u",registers.bits.nhd,registers.bits.nht);
        ImGui::Text("V Displayed = %u, Total = %u",registers.bits.nvd,registers.bits.nvt);
        ImGui::Text("Scanlines = %u * %u + %u = %u",registers.bits.nvd,registers.bits.nr+1,registers.bits.nadj,registers.bits.nvd*(registers.bits.nr+1)+registers.bits.nadj);
        ImGui::Text("Address = $%04x",display_address);
        ImGui::Text("(Wrap Adjustment = $%04x)",BBCMicro::SCREEN_WRAP_ADJUSTMENTS[latch.bits.screen_base]<<3);
        ImGui::Separator();
        ImGui::Text("HSync Pos = %u, Width = %u",registers.bits.nhsp,registers.bits.nsw.bits.wh);
        ImGui::Text("VSync Pos = %u, Width = %u",registers.bits.nvsp,registers.bits.nsw.bits.wv);
        ImGui::Text("Interlace Sync = %s, Video = %s",BOOL_STR(registers.bits.r8.bits.s),BOOL_STR(registers.bits.r8.bits.v));
        ImGui::Text("Delay Mode = %s",DELAY_NAMES[registers.bits.r8.bits.d]);
        ImGui::Separator();
        ImGui::Text("Cursor Start = %u, End = %u, Mode = %s",registers.bits.ncstart.bits.start,registers.bits.ncend,GetCRTCCursorModeEnumName(registers.bits.ncstart.bits.mode));
        ImGui::Text("Cursor Delay Mode = %s",DELAY_NAMES[registers.bits.r8.bits.c]);
        ImGui::Text("Cursor Address = $%04x",cursor_address);
        ImGui::Separator();
    }
private:
    static const char *const INTERLACE_NAMES[];
    static const char *const DELAY_NAMES[];
};

const char *const CRTCDebugWindow::INTERLACE_NAMES[]={"Normal","Normal","Interlace sync","Interlace sync+video"};

const char *const CRTCDebugWindow::DELAY_NAMES[]={"0","1","2","Off"};

std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<CRTCDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoULADebugWindow:
    public DebugUI
{
public:
protected:
    void HandleDoImGui() override {
        VideoULA::Control control;
        uint8_t palette[16];

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);

            const VideoULA *u=m->DebugGetVideoULA();

            control=u->control;
            memcpy(palette,u->m_palette,16);
        }


        if(ImGui::CollapsingHeader("Register Values")) {
            ImGui::Text("Control = $%02x %03u %s",control.value,control.value,BINARY_BYTE_STRINGS[control.value]);
            for(size_t i=0;i<16;++i) {
                uint8_t p=palette[i];
                ImGui::Text("Palette[%zu] = $%01x %02u %s ",i,p,p,BINARY_BYTE_STRINGS[p]+4);

                uint8_t colour=p&7;
                if(p&8) {
                    if(control.bits.flash) {
                        colour^=7;
                    }
                }

                ImGui::SameLine();

                {
                    ImGuiStyleColourPusher pusher(ImGuiCol_Text,COLOUR_COLOURS[colour]);
                    ImGui::TextUnformatted(COLOUR_NAMES[colour]);
                }

                if(p&8) {
                    ImGui::SameLine();

                    ImGui::Text("(%s/%s)",COLOUR_NAMES[p&7],COLOUR_NAMES[(p&7)^7]);
                }
            }
            ImGui::Separator();
        }

        ImGui::Text("Flash colour = %u",control.bits.flash);
        ImGui::Text("Teletext output = %s",BOOL_STR(control.bits.teletext));
        ImGui::Text("Chars per line = %u",(1<<control.bits.line_width)*10);
        ImGui::Text("6845 clock = %u MHz",1+control.bits.fast_6845);
        ImGui::Text("Cursor Shape = %s",CURSOR_SHAPES[control.bits.cursor]);

        for(uint8_t i=0;i<16;i+=4) {
            ImGui::Text("Palette:");
            ImGuiStyleVarPusher vpusher(ImGuiStyleVar_FramePadding,ImVec2(0.f,0.f));

            for(uint8_t j=0;j<4;++j) {
                uint8_t index=i+j;
                uint8_t entry=palette[index];

                uint8_t colour=entry&7;
                if(entry&8) {
                    if(control.bits.flash) {
                        colour^=7;
                    }
                }

                ImGui::SameLine();
                ImGui::Text(" %x=",index);
                ImGuiStyleColourPusher cpusher(ImGuiCol_Text,COLOUR_COLOURS[colour]);
                ImGui::SameLine();
                ImGui::Text("%x",entry);
            }
        }
    }
private:
    static const char *const COLOUR_NAMES[];
    static const ImVec4 COLOUR_COLOURS[];
    static const char *const CURSOR_SHAPES[];
};

const char *const VideoULADebugWindow::COLOUR_NAMES[]={
    "Black","Red","Green","Yellow","Blue","Magenta","Cyan","White",
};

const ImVec4 VideoULADebugWindow::COLOUR_COLOURS[]={
    {.8f,.8f,.8f,1.f},//Black on black = useless
    {1.f,0.f,0.f,1.f},
    {0.f,1.f,0.f,1.f},
    {1.f,1.f,0.f,1.f},
    {.2f,.4f,1.f,1.f},//I find the blue a bit hard to see on black...
    {1.f,0.f,1.f,1.f},
    {0.f,1.f,1.f,1.f},
    {1.f,1.f,1.f,1.f},
};

const char *const VideoULADebugWindow::CURSOR_SHAPES[]={
    "....",".**.",".*..",".***","*...","*.**","**..","****"
};

std::unique_ptr<SettingsUI> CreateVideoULADebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<VideoULADebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class R6522DebugWindow:
    public DebugUI
{
public:
    struct PortState {
        uint8_t or_,ddr,p,c1,c2;
    };

    struct State {
        uint16_t t1,t2;
        M6502Word t1l;
        uint8_t t2ll;
        uint8_t sr;
        R6522::ACR acr;
        R6522::PCR pcr;
        R6522::IRQ ifr,ier;
        PortState a,b;
    };
protected:
    void DoRegisterValuesGui(const State &s,bool has_debug_state,BBCMicro::HardwareDebugState hw,R6522::IRQ BBCMicro::HardwareDebugState::*irq_mptr) {
        this->DoPortRegisterValuesGui('A',s.a);
        this->DoPortRegisterValuesGui('B',s.b);
        ImGui::Text("T1 : $%04x %05d %s%s",s.t1,s.t1,BINARY_BYTE_STRINGS[s.t1>>8&0xff],BINARY_BYTE_STRINGS[s.t1&0xff]);
        ImGui::Text("T1L: $%04x %05d %s%s",s.t1l.w,s.t1l.w,BINARY_BYTE_STRINGS[s.t1l.b.h],BINARY_BYTE_STRINGS[s.t1l.b.l]);
        ImGui::Text("T2 : $%04x %05d %s%s",s.t2,s.t2,BINARY_BYTE_STRINGS[s.t2>>8&0xff],BINARY_BYTE_STRINGS[s.t2&0xff]);
        ImGui::Text("SR : $%02x %03d %s",s.sr,s.sr,BINARY_BYTE_STRINGS[s.sr]);
        ImGui::Text("ACR: PA latching = %s",BOOL_STR(s.acr.bits.pa_latching));
        ImGui::Text("ACR: PB latching = %s",BOOL_STR(s.acr.bits.pb_latching));
        ImGui::Text("ACR: Shift mode = %s",ACR_SHIFT_MODES[s.acr.bits.sr]);
        ImGui::Text("ACR: T2 mode = %s",s.acr.bits.t2_count_pb6?"Timed interrupt":"Count PB6 pulses");
        ImGui::Text("ACR: T1 continuous = %s, output PB7 = %s",BOOL_STR(s.acr.bits.t1_continuous),BOOL_STR(s.acr.bits.t1_output_pb7));
        ImGui::Text("PCR: CA1 = %cve edge",s.pcr.bits.ca1_pos_irq?'+':'-');
        ImGui::Text("PCR: CA2 = %s",PCR_CONTROL_MODES[s.pcr.bits.ca2_mode]);
        ImGui::Text("PCR: CB1 = %cve edge",s.pcr.bits.cb1_pos_irq?'+':'-');
        ImGui::Text("PCR: CB2 = %s",PCR_CONTROL_MODES[s.pcr.bits.cb2_mode]);

        ImGui::Text("     [%-3s][%-3s][%-3s][%-3s][%-3s][%-3s][%-3s]",IRQ_NAMES[6],IRQ_NAMES[5],IRQ_NAMES[4],IRQ_NAMES[3],IRQ_NAMES[2],IRQ_NAMES[1],IRQ_NAMES[0]);
        ImGui::Text("IFR:   %u    %u    %u    %u    %u    %u    %u",s.ifr.value&1<<6,s.ifr.value&1<<5,s.ifr.value&1<<4,s.ifr.value&1<<3,s.ifr.value&1<<2,s.ifr.value&1<<1,s.ifr.value&1<<0);
        ImGui::Text("IER:   %u    %u    %u    %u    %u    %u    %u",s.ier.value&1<<6,s.ier.value&1<<5,s.ier.value&1<<4,s.ier.value&1<<3,s.ier.value&1<<2,s.ier.value&1<<1,s.ier.value&1<<0);

        if(has_debug_state) {
            ImGui::Separator();

            bool changed=false;

            R6522::IRQ *irq=&(hw.*irq_mptr);

            ImGui::Text("Break: ");

            for(uint8_t i=0;i<7;++i) {
                uint8_t bit=6-i;
                uint8_t mask=1<<bit;

                ImGui::SameLine();

                bool value=!!(irq->value&mask);
                if(ImGui::Checkbox(IRQ_NAMES[bit],&value)) {
                    irq->value&=~mask;
                    if(value) {
                        irq->value|=mask;
                    }
                    changed=true;
                }
            }

            if(changed) {
                std::unique_lock<Mutex> lock;
                BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);
                m->SetHardwareDebugState(hw);
            }
        }
    }

    void GetState(State *state,const R6522 *via) {
        state->t1=(uint16_t)via->m_t1;
        state->t2=(uint16_t)via->m_t2;
        state->t1l.b.l=via->m_t1ll;
        state->t1l.b.h=via->m_t1lh;
        state->t2ll=via->m_t2ll;
        state->sr=via->m_sr;
        state->acr=via->m_acr;
        state->pcr=via->m_pcr;
        state->ifr=via->ifr;
        state->ier=via->ier;

        state->a.or_=via->a.or_;
        state->a.ddr=via->a.ddr;
        state->a.p=via->a.p;
        state->a.c1=via->a.c1;
        state->a.c2=via->a.c2;

        state->b.or_=via->b.or_;
        state->b.ddr=via->b.ddr;
        state->b.p=via->b.p;
        state->b.c1=via->b.c1;
        state->b.c2=via->b.c2;
    }
private:
    void DoPortRegisterValuesGui(char port,const PortState &p) {
        ImGui::Text("Port %c: Pins = $%02x %03d %s",port,p.p,p.p,BINARY_BYTE_STRINGS[p.p]);
        ImGui::Text("Port %c: DDR%c = $%02x %03d %s",port,port,p.ddr,p.ddr,BINARY_BYTE_STRINGS[p.ddr]);
        ImGui::Text("Port %c: OR%c  = $%02x %03d %s",port,port,p.or_,p.or_,BINARY_BYTE_STRINGS[p.or_]);
        ImGui::Text("Port %c: C%c1 = %s C%c2 = %s",port,port,BOOL_STR(p.c1),port,BOOL_STR(p.c2));
    }

    static const char *const ACR_SHIFT_MODES[];
    static const char *const PCR_CONTROL_MODES[];
    static const char *const IRQ_NAMES[];
};

// indexed by bit
const char *const R6522DebugWindow::IRQ_NAMES[]={
    "CA2",
    "CA1",
    "SR",
    "CB2",
    "CB1",
    "T2",
    "T1",
};

const char *const R6522DebugWindow::ACR_SHIFT_MODES[]={
    "Off",
    "In, T2",
    "In, clock",
    "In, CB1",
    "Out, free, T2",
    "Out, T2",
    "Out, clock",
    "Out, CB1",
};

const char *const R6522DebugWindow::PCR_CONTROL_MODES[]={
    "Input -ve edge",
    "Indep. IRQ input -ve edge",
    "Input +ve edge",
    "Indep. IRQ input +ve edge",
    "Handshake output",
    "Pulse output",
    "Low output",
    "High output",
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SystemVIADebugWindow:
    public R6522DebugWindow
{
public:
protected:
    void HandleDoImGui() override {
        State state;
        BBCMicro::AddressableLatch latch;
        BBCMicroType type;
        bool has_debug_state;
        BBCMicro::HardwareDebugState hw;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            this->GetState(&state,m->DebugGetSystemVIA());
            latch=m->DebugGetAddressableLatch();
            type=m->GetType();
            has_debug_state=m->HasDebugState();
            hw=m->GetHardwareDebugState();
        }

        this->DoRegisterValuesGui(state,has_debug_state,hw,&BBCMicro::HardwareDebugState::system_via_irq_breakpoints);

        ImGui::Separator();

        BBCMicro::SystemVIAPB pb;
        pb.value=state.b.p;

        ImGui::Text("Joystick 0 Fire = %s",BOOL_STR(!pb.bits.not_joystick0_fire));
        ImGui::Text("Joystick 1 Fire = %s",BOOL_STR(!pb.bits.not_joystick1_fire));
        ImGui::Text("Latch Bit = %u, Value = %u",pb.bits.latch_index,pb.bits.latch_value);
        switch(type) {
        case BBCMicroType_B:
        case BBCMicroType_BPlus:
            ImGui::Text("Speech Ready = %u, IRQ = %u",pb.b_bits.speech_ready,pb.b_bits.speech_interrupt);
            break;

        case BBCMicroType_Master:
            ImGui::Text("RTC CS = %u, AS = %u",pb.m128_bits.rtc_chip_select,pb.m128_bits.rtc_address_strobe);
            break;
        }

        ImGui::Separator();

        ImGui::Text("Sound Write = %s",BOOL_STR(!latch.bits.not_sound_write));
        ImGui::Text("Screen Wrap Size = $%04x",BBCMicro::SCREEN_WRAP_ADJUSTMENTS[latch.bits.screen_base]<<3);
        ImGui::Text("Caps Lock LED = %s",BOOL_STR(latch.bits.caps_lock_led));
        ImGui::Text("Shift Lock LED = %s",BOOL_STR(latch.bits.shift_lock_led));

        switch(type) {
        case BBCMicroType_B:
        case BBCMicroType_BPlus:
            ImGui::Text("Speech Read = %u, Write = %u",latch.b_bits.speech_read,latch.b_bits.speech_write);
            break;

        case BBCMicroType_Master:
            ImGui::Text("RTC Read = %u, DS = %u",latch.m128_bits.rtc_read,latch.m128_bits.rtc_data_strobe);
            break;
        }
    }
private:
};

std::unique_ptr<SettingsUI> CreateSystemVIADebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<SystemVIADebugWindow>(beeb_window);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class UserVIADebugWindow:
    public R6522DebugWindow
{
public:
protected:
    void HandleDoImGui() override {
        State state;
        bool has_debug_state;
        BBCMicro::HardwareDebugState hw;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            this->GetState(&state,m->DebugGetUserVIA());
            has_debug_state=m->HasDebugState();
            hw=m->GetHardwareDebugState();
        }

        this->DoRegisterValuesGui(state,has_debug_state,hw,&BBCMicro::HardwareDebugState::user_via_irq_breakpoints);
    }
private:
};

std::unique_ptr<SettingsUI> CreateUserVIADebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<UserVIADebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class NVRAMDebugWindow:
    public DebugUI
{
public:
protected:
    void HandleDoImGui() override {
        std::vector<uint8_t> nvram;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);

            nvram=m->GetNVRAM();
        }

        if(nvram.empty()) {
            ImGui::Text("This computer has no non-volatile RAM.");
        } else if(nvram.size()<50) {
            ImGui::Text("%zu bytes of non-volatile RAM.",nvram.size());
        } else {
            ImGui::Text("Econet station number: $%02X\n",nvram[0]);
            ImGui::Text("File server station number: $%02X\n",nvram[1]);
            ImGui::Text("File server network number: $%02X\n",nvram[2]);
            ImGui::Text("Printer server station number: $%02X\n",nvram[3]);
            ImGui::Text("Printer server station number: $%02X\n",nvram[4]);
            ImGui::Text("Default ROMs: Filing system: %d\n",nvram[5]&15);
            ImGui::Text("              Language: %d\n",nvram[5]>>4);
            {
                char roms_str[17]="0123456789ABCDEF";

                uint16_t tmp=nvram[6]|nvram[7]<<8;
                for(size_t i=0;i<16;++i) {
                    if(!(tmp&1<<i)) {
                        roms_str[i]='_';
                    }
                }

                ImGui::Text("Inserted ROMs: %s",roms_str);
            }
            ImGui::Text("EDIT ROM byte: $%02X (%d)\n",nvram[8],nvram[8]);
            ImGui::Text("Telecommunication applications byte: $%02X (%d)\n",nvram[9],nvram[9]);
            ImGui::Text("Default MODE: %d\n",nvram[10]&7);
            ImGui::Text("Default Shadow RAM: %s\n",BOOL_STR(nvram[10]&8));
            ImGui::Text("Default Interlace: %s\n",BOOL_STR((nvram[10]&16)==0));
            ImGui::Text("Default *TV: %d\n",(nvram[10]>>5&3)-(nvram[10]>>5&4));
            ImGui::Text("Default FDRIVE: %d\n",nvram[11]&7);
            ImGui::Text("Default Shift lock: %s\n",BOOL_STR(nvram[11]&8));
            ImGui::Text("Default No lock: %s\n",BOOL_STR(nvram[11]&16));
            ImGui::Text("Default Caps lock: %s\n",BOOL_STR(nvram[11]&32));
            ImGui::Text("Default ADFS load dir: %s\n",BOOL_STR(nvram[11]&64));
            // nvram[11] contrary to what NAUG says...
            ImGui::Text("Default drive: %s\n",nvram[11]&128?"floppy drive":"hard drive");
            ImGui::Text("Keyboard auto-repeat delay: %d\n",nvram[12]);
            ImGui::Text("Keyboard auto-repeat rate: %d\n",nvram[13]);
            ImGui::Text("Printer ignore char: %d (0x%02X)\n",nvram[14],nvram[14]);
            ImGui::Text("Tube on: %s\n",BOOL_STR(nvram[15]&1));
            ImGui::Text("Use printer ignore char: %s\n",BOOL_STR((nvram[15]&2)==0));
            ImGui::Text("Serial baud rate index: %d\n",nvram[15]>>2&7);
            ImGui::Text("*FX5 setting: %d\n",nvram[15]>>5&7);
            // 16 bit 0 unused
            ImGui::Text("Default beep volume: %s\n",nvram[16]&2?"loud":"quiet");
            ImGui::Text("Use Tube: %s\n",nvram[16]&4?"external":"internal");
            ImGui::Text("Default scrolling: %s\n",nvram[16]&8?"protected":"enabled");
            ImGui::Text("Default boot mode: %s\n",nvram[16]&16?"auto boot":"no boot");
            ImGui::Text("Default serial data format: %d\n",nvram[16]>>5&7);
        }
    }
private:
};

std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<NVRAMDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SN76489DebugWindow:
public DebugUI
{
public:
protected:
    void HandleDoImGui() override {
        SN76489::ChannelValues values[4];
        uint16_t seed;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            const SN76489 *sn=m->DebugGetSN76489();
            sn->GetState(values,&seed);
        }

        Tone(values[0],0);
        Tone(values[1],1);
        Tone(values[2],2);

        {
            const char *type;
            if(values[3].freq&4) {
                type="White";
            } else {
                type="Periodic";
            }

            uint16_t sn_freq;
            const char *suffix="";
            switch(values[3].freq&3) {
                case 0:
                    sn_freq=0x10;
                    break;

                case 1:
                    sn_freq=0x20;
                    break;

                case 2:
                    sn_freq=0x40;
                    break;

                case 3:
                    suffix=" (Tone 2)";
                    sn_freq=values[2].freq;
                    break;
            }

            ImGui::Text("Noise : vol=%-2d freq=%-5u (0x%04x) (%uHz)",
                        values[3].vol,sn_freq,sn_freq,GetHz(sn_freq));
            ImGui::Text("        %s%s",type,suffix);
            ImGui::Text("        seed: $%04x %%%s%s",
                        seed,BINARY_BYTE_STRINGS[seed>>8],BINARY_BYTE_STRINGS[seed&0xff]);
        }
    }
private:
    static uint32_t GetHz(uint16_t sn_freq) {
        return 4000000/(sn_freq*32);
    }

    static void Tone(const SN76489::ChannelValues &values,int n) {
        ImGui::Text("Tone %d: vol=%-2d freq=%-5u (0x%04x) (%uHz)",
                    n,values.vol,values.freq,values.freq,GetHz(values.freq));
    }
};

std::unique_ptr<SettingsUI> CreateSN76489DebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<SN76489DebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
