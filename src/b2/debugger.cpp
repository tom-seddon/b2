#include <shared/system.h>
#include "debugger.h"

#if BBCMICRO_DEBUGGER

#include "dear_imgui.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <shared/log.h>
#include <imgui_memory_editor.h>
#include <unordered_map>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(DBG,"debugger","DBG   ",&log_printer_stdout_and_debugger,true)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::unordered_map<M6502Fn,const char *> g_name_by_6502_fn;

static const char *GetFnName(M6502Fn fn) {
    if(!fn) {
        return "NULL";
    } else {
        auto it=g_name_by_6502_fn.find(fn);
        if(it==g_name_by_6502_fn.end()) {
            return "?";
        } else {
            return it->second;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DebugUI:
    public SettingsUI
{
public:
    explicit DebugUI(std::shared_ptr<BeebThread> beeb_thread):
        m_beeb_thread(std::move(beeb_thread))
    {
    }

    bool OnClose() override {
        return false;
    }
protected:
    const std::shared_ptr<BeebThread> m_beeb_thread;
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class M6502DebugWindow:
    public DebugUI
{
public:
    explicit M6502DebugWindow(std::shared_ptr<BeebThread> beeb_thread):
        DebugUI(std::move(beeb_thread))
    {
        if(g_name_by_6502_fn.empty()) {
            M6502_ForEachFn([](const char *name,M6502Fn fn,void *) {
                g_name_by_6502_fn[fn]=name;
            },nullptr);
        }
    }

    void DoImGui(CommandContextStack *cc_stack) {
        (void)cc_stack;
        //m_beeb_thread->SendUpdate6502StateMessage();

        bool halted;
        uint16_t pc,abus;
        uint8_t a,x,y,sp,opcode,read,dbus;
        M6502P p;
        M6502Fn tfn,ifn;
        const M6502Config *config;

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            const M6502 *s=m->GetM6502();

            config=s->config;
            halted=m->DebugIsHalted();
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

        ImGui::Separator();

        ImGui::Text("Opcode = $%02X %03d - %s %s",opcode,opcode,mnemonic,mode_name);
        ImGui::Text("tfn = %s; ifn = %s",GetFnName(tfn),GetFnName(ifn));
        ImGui::Text("State = %s",halted?"halted":"running");
        ImGui::Text("Address = $%04x; Data = $%02x %03d %s",abus,dbus,dbus,BINARY_BYTE_STRINGS[dbus]);
        ImGui::Text("Access = %s",M6502ReadType_GetName(read));

        //ImGui::Separator();
        //m_debug_occ.DoButton("stop");
        //ImGui::SameLine();
        //m_debug_occ.DoButton("run");
        //ImGui::SameLine();
        //m_debug_occ.DoButton("step_in");
        //ImGui::SameLine();
        //m_debug_occ.DoButton("step_over");
    }
protected:
private:
    void Reg(const char *name,uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s",name,value,value,BINARY_BYTE_STRINGS[value]);
    }
};

std::unique_ptr<SettingsUI> Create6502DebugWindow(BeebWindow *beeb_window) {
    return std::make_unique<M6502DebugWindow>(beeb_window->GetBeebThread());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A cache for data copied out from DebugCopyMemory. For each memory
// access by the debug window, 256 bytes is copied out, in an attempt
// to avoid calling LockBeeb too often.
//
// Rather than trying to be at all clever, this just has a very large buffer.
class BeebMemory {
public:
    explicit BeebMemory(std::shared_ptr<BeebThread> beeb_thread);

    void Reset();
    uint8_t GetByte(uint16_t addr);
    uint16_t GetDebugPage(uint16_t addr);
    BBCMicro::ByteDebugFlags GetDebugFlags(uint16_t addr);
protected:
private:
    struct Page {
        uint8_t ram[256];
        BBCMicro::ByteDebugFlags debug[256];
        uint16_t flat_page;
    };

    const std::shared_ptr<BeebThread> m_beeb_thread;
    uint8_t m_got_pages[32];
    Page m_pages[256]={};
    uint8_t m_ram[256][256]={};
    BBCMicro::ByteDebugFlags m_debug[256][256]={};

    void PrepareBuffer(M6502Word addr);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebMemory::BeebMemory(std::shared_ptr<BeebThread> beeb_thread):
    m_beeb_thread(std::move(beeb_thread))
{
    this->Reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebMemory::Reset() {
    memset(m_got_pages,0,sizeof m_got_pages);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BeebMemory::GetByte(uint16_t addr_) {
    M6502Word addr={addr_};

    this->PrepareBuffer(addr);

    return m_pages[addr.b.h].ram[addr.b.l];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint16_t BeebMemory::GetDebugPage(uint16_t addr_) {
    M6502Word addr={addr_};

    this->PrepareBuffer(addr);

    return m_pages[addr.b.h].flat_page;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::ByteDebugFlags BeebMemory::GetDebugFlags(uint16_t addr_) {
    M6502Word addr={addr_};

    this->PrepareBuffer(addr);

    return m_pages[addr.b.h].debug[addr.b.l];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebMemory::PrepareBuffer(M6502Word addr) {
    uint8_t *flags=&m_got_pages[addr.b.h>>3];
    uint8_t mask=1<<(addr.b.h&7);
    if(!(*flags&mask)) {
        addr.b.l=0;

        std::unique_lock<Mutex> lock;
        const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);

        Page *page=&m_pages[addr.b.h];

        page->flat_page=m->DebugGetFlatPage(addr.b.h);
        m->DebugCopyMemory(page->ram,page->debug,addr,256);

        *flags|=mask;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MemoryDebugWindow:
    public DebugUI
{
public:
    explicit MemoryDebugWindow(std::shared_ptr<BeebThread> beeb_thread):
        DebugUI(std::move(beeb_thread)),
        m_mem(m_beeb_thread)
    {
        m_memory_editor.ReadFn=&MemoryEditorRead;
        m_memory_editor.WriteFn=&MemoryEditorWrite;
    }

    void DoImGui(CommandContextStack *cc_stack) {
        (void)cc_stack;

        m_mem.Reset();

        m_memory_editor.DrawContents((uint8_t *)this,65536,0);
    }
protected:
private:
    M6502Word m_addr{};
    MemoryEditor m_memory_editor;
    BeebMemory m_mem;

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static uint8_t MemoryEditorRead(uint8_t *data,size_t off) {
        auto self=(MemoryDebugWindow *)data;

        ASSERT((uint16_t)off==off);
        return self->m_mem.GetByte((uint16_t)off);
    }

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static void MemoryEditorWrite(uint8_t *data,size_t off,uint8_t d) {
        auto self=(MemoryDebugWindow *)data;

        self->m_beeb_thread->SendDebugSetByteMessage((uint16_t)off,d);
    }
};

std::unique_ptr<SettingsUI> CreateMemoryDebugWindow(BeebWindow *beeb_window) {
    return std::make_unique<MemoryDebugWindow>(beeb_window->GetBeebThread());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DisassemblyDebugWindow:
    public DebugUI
{
public:
    explicit DisassemblyDebugWindow(std::shared_ptr<BeebThread> beeb_thread):
        DebugUI(std::move(beeb_thread)),
        m_occ(this,&ms_command_table),
        m_mem(m_beeb_thread)
    {
    }

    uint32_t GetExtraImGuiWindowFlags() const override {
        // The bottom line of the disassembly should just be clipped
        // if it runs off the bottom... only drawing whole lines just
        // looks weird. But when that happens, dear imgui
        // automatically adds a scroll bar. And that's even weirder.
        return ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
    }

    void DoImGui(CommandContextStack *cc_stack) {
        cc_stack->Push(m_occ);

        const M6502Config *config;
        uint16_t pc;
        uint8_t x,y;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            const M6502 *s=m->GetM6502();

            config=s->config;
            pc=s->opcode_pc.w;
            x=s->x;
            y=s->y;
        }

        float maxY=ImGui::GetCurrentWindow()->Size.y;//-ImGui::GetTextLineHeight()-GImGui->Style.WindowPadding.y*2.f;

        m_occ.DoToggleCheckboxUI("toggle_track_pc");

        m_occ.DoButton("go_back");

        if(ImGui::InputText("Address",
                            m_address_text,sizeof m_address_text,
                            ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll))
        {
            char *ep;
            unsigned long address=strtoul(m_address_text,&ep,16);
            if(*ep==0||isspace(*ep)&&address<65536) {
                this->GoTo((uint16_t)address);
            }
        }

        if(m_track_pc) {
            m_addr=pc;
        }

        m_mem.Reset();
        //m_line_addrs.clear();

        m_num_lines=0;
        uint16_t addr=m_addr;
        while(ImGui::GetCursorPosY()<=maxY) {
            ++m_num_lines;
            M6502Word line_addr={addr};
            //m_line_addrs.push_back(line_addr.w);

            ImGuiIDPusher id_pusher(addr);

            uint8_t opcode=m_mem.GetByte(addr);
            BBCMicro::ByteDebugFlags debug_flags=m_mem.GetDebugFlags(addr);
            uint16_t debug_flat_page=m_mem.GetDebugPage(addr);
            char flat_page_code=BBCMicro::DebugGetFlatPageCode(debug_flat_page);
            ++addr;

            const M6502DisassemblyInfo *di=&config->disassembly_info[opcode];

            M6502Word operand={};
            if(di->num_bytes>=2) {
                operand.b.l=m_mem.GetByte(addr++);
            }
            if(di->num_bytes>=3) {
                operand.b.h=m_mem.GetByte(addr++);
            }

            ImGuiStyleColourPusher pusher;

            if(line_addr.w==pc) {
                pusher.Push(ImGuiCol_Text,ImVec4(1.f,1.f,0.f,1.f));
            }

            bool break_execute=!!debug_flags.bits.break_execute;
            if(ImGui::Checkbox("",&break_execute)) {
                debug_flags.bits.break_execute=break_execute;

                std::unique_lock<Mutex> lock;
                BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

                m->DebugSetByteFlags(line_addr,debug_flags);
            }

            ImGui::SameLine();

            ImGui::Text("%c.%04x  %c%c %c%c %c%c  %c%c%c  %s ",
                        flat_page_code,
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

        {
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
protected:
private:
    static const char IND_PREFIX[];

    ObjectCommandContext<DisassemblyDebugWindow> m_occ;
    uint16_t m_addr=0;
    BeebMemory m_mem;
    bool m_track_pc=true;
    char m_address_text[100]={};
    //std::vector<uint16_t> m_line_addrs;
    std::vector<uint16_t> m_history;
    int m_num_lines=0;
    //char m_disassembly_text[100];
    float m_wheel=0;

    void DoIndirect(uint16_t address,uint16_t mask,uint16_t post_index) {
        M6502Word addr;
        addr.b.l=m_mem.GetByte(address);

        ++address;
        address&=mask;

        addr.b.h=m_mem.GetByte(address);

        addr.w+=post_index;

        M6502Word target_addr;
        target_addr.b.l=m_mem.GetByte(addr.w);
        ++addr.w;
        target_addr.b.h=m_mem.GetByte(addr.w);

        this->AddWord(IND_PREFIX,target_addr.w,"");
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
            // No point using SmallButton - it doesn't set the horizontal padding to 0.

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

            opcode=m_mem.GetByte(m_addr-1);
            if(config->disassembly_info[opcode].num_bytes==1) {
                --m_addr;
                return;
            }

            opcode=m_mem.GetByte(m_addr-2);
            if(config->disassembly_info[opcode].num_bytes==2) {
                m_addr-=2;
                return;
            }

            m_addr-=3;
        }
    }

    void Down(const M6502Config *config,int n) {
        for(int i=0;i<n;++i) {
            uint8_t opcode=m_mem.GetByte(m_addr);
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
    return std::make_unique<DisassemblyDebugWindow>(beeb_window->GetBeebThread());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
