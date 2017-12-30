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
    bool OnClose() override {
        return false;
    }
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class M6502DebugWindow:
    public DebugUI
{
public:
    explicit M6502DebugWindow(std::shared_ptr<BeebThread> beeb_thread):
        m_beeb_thread(std::move(beeb_thread))
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

        ImGui::Separator();

        if(ImGuiButton("Stop",!halted)) {
            std::unique_lock<Mutex> lock;
            BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);
            m->DebugHalt();
        }

        ImGui::SameLine();
        if(ImGuiButton("Run",halted)) {
            std::unique_lock<Mutex> lock;
            BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);
            m->DebugRun();
            m_beeb_thread->SendDebugWakeUpMessage();
        }

        ImGui::SameLine();
        if(ImGuiButton("Step In",halted)) {
            this->StepIn();
        }

        ImGui::SameLine();
        if(ImGuiButton("Step Over",halted)) {
            const M6502DisassemblyInfo *di=&config->disassembly_info[opcode];

            if(di->always_step_in) {
                this->StepIn();
            } else {
                std::unique_lock<Mutex> lock;
                BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

                const M6502 *s=m->GetM6502();

                M6502Word next_pc={(uint16_t)(s->opcode_pc.w+di->num_bytes)};
                m->DebugAddTempBreakpoint(next_pc);
                printf("step over %s/%s - next_pc=$%04x\n",mnemonic,mode_name,next_pc.w);
                m->DebugRun();
                m_beeb_thread->SendDebugWakeUpMessage();
            }
        }

        //ImGui::SameLine();

        //if(ImGuiButton("Step Out",halted)) {
        //}
    }
protected:
private:
    const std::shared_ptr<BeebThread> m_beeb_thread;

    void Reg(const char *name,uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s",name,value,value,BINARY_BYTE_STRINGS[value]);
    }

    void StepIn() {
        printf("step in...\n");
        std::unique_lock<Mutex> lock;
        BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);

        m->DebugStepIn();
        m->DebugRun();
        m_beeb_thread->SendDebugWakeUpMessage();
    }
};

std::unique_ptr<SettingsUI> Create6502DebugWindow(BeebWindow *beeb_window) {
    return std::make_unique<M6502DebugWindow>(beeb_window->GetBeebThread());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A 256-byte cache for data copied out from DebugCopyMemory, since
// the debug windows' memory accesses are often predictable.
class BeebMemory
{
public:
    explicit BeebMemory(std::shared_ptr<BeebThread> beeb_thread);

    void Reset();
    uint8_t GetByte(uint16_t addr);
    uint16_t GetDebugPage(uint16_t addr);
    BBCMicro::ByteDebugFlags GetDebugFlags(uint16_t addr);
protected:
private:
    const std::shared_ptr<BeebThread> m_beeb_thread;
    uint8_t m_buffer[256]={};
    uint16_t m_debug_flat_page=0;
    BBCMicro::ByteDebugFlags m_debug_buffer[256]={};

    int m_buffer_page;

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
    m_buffer_page=-1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BeebMemory::GetByte(uint16_t addr_) {
    M6502Word addr={addr_};

    this->PrepareBuffer(addr);

    return m_buffer[addr.b.l];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint16_t BeebMemory::GetDebugPage(uint16_t addr_) {
    M6502Word addr={addr_};

    this->PrepareBuffer(addr);

    return m_debug_flat_page;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::ByteDebugFlags BeebMemory::GetDebugFlags(uint16_t addr_) {
    M6502Word addr={addr_};

    this->PrepareBuffer(addr);

    return m_debug_buffer[addr.b.l];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebMemory::PrepareBuffer(M6502Word addr) {
    if(m_buffer_page!=addr.b.h) {
        addr.b.l=0;

        std::unique_lock<Mutex> lock;
        const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);

        m_debug_flat_page=m->DebugGetFlatPage(addr.b.h);
        m->DebugCopyMemory(m_buffer,m_debug_buffer,addr,256);

        m_buffer_page=addr.b.h;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MemoryDebugWindow:
    public DebugUI
{
public:
    explicit MemoryDebugWindow(std::shared_ptr<BeebThread> beeb_thread):
        m_beeb_thread(std::move(beeb_thread)),
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
    const std::shared_ptr<BeebThread> m_beeb_thread;
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
        m_beeb_thread(std::move(beeb_thread)),
        m_occ(this,&ms_command_table),
        m_mem(m_beeb_thread)
    {
    }

    void DoImGui(CommandContextStack *cc_stack) {
        cc_stack->Push(m_occ);

        const M6502Config *config;
        uint16_t pc;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m=m_beeb_thread->LockBeeb(&lock);
            const M6502 *s=m->GetM6502();

            config=s->config;
            pc=s->opcode_pc.w;
        }

        float height=ImGui::GetCurrentWindow()->Size.y;

        m_occ.DoToggleCheckboxUI("toggle_track_pc");

        if(m_track_pc) {
            m_addr=pc;
        }

        m_mem.Reset();

        uint16_t addr=m_addr;
        while(ImGui::GetCursorPosY()<height) {
            M6502Word line_addr={addr};
            char disassembly_text[50],bytes_text[50];
            uint8_t bytes[3];

            char *c=disassembly_text;
            size_t num_bytes=0;

            ImGuiIDPusher id_pusher(addr);

            bytes[0]=m_mem.GetByte(addr);
            BBCMicro::ByteDebugFlags debug_flags=m_mem.GetDebugFlags(addr);
            uint16_t debug_flat_page=m_mem.GetDebugPage(addr);
            char flat_page_code=BBCMicro::DebugGetFlatPageCode(debug_flat_page);
            ++addr;

            const M6502DisassemblyInfo *di=&config->disassembly_info[bytes[0]];

            memcpy(c,di->mnemonic,sizeof di->mnemonic-1);
            c+=sizeof di->mnemonic-1;
            *c++=' ';

            switch(di->mode) {
            default:
                ASSERT(0);
                // fall through
            case M6502AddrMode_IMP:
                num_bytes=1;
                break;

            case M6502AddrMode_REL:
                {
                    bytes[1]=m_mem.GetByte(addr++);

                    M6502Word dest;
                    dest.w=addr+(uint16_t)(int16_t)(int8_t)bytes[1];

                    c=AddWord(c,"$",dest.b.l,dest.b.h,"");
                    num_bytes=2;
                }
                break;

            case M6502AddrMode_IMM:
                bytes[1]=m_mem.GetByte(addr++);
                c=AddByte(c,"#$",bytes[1],"");
                num_bytes=2;
                break;

            case M6502AddrMode_ZPG:
                bytes[1]=m_mem.GetByte(addr++);
                c=AddByte(c,"$",bytes[1],"");
                num_bytes=2;
                break;

            case M6502AddrMode_ZPX:
                bytes[1]=m_mem.GetByte(addr++);
                c=AddByte(c,"$",bytes[1],",X");
                num_bytes=2;
                break;

            case M6502AddrMode_ZPY:
                bytes[1]=m_mem.GetByte(addr++);
                c=AddByte(c,"$",bytes[1],",Y");
                num_bytes=2;
                break;

            case M6502AddrMode_ABS:
                bytes[1]=m_mem.GetByte(addr++);
                bytes[2]=m_mem.GetByte(addr++);
                c=AddWord(c,"$",bytes[1],bytes[2],"");
                num_bytes=3;
                break;

            case M6502AddrMode_ABX:
                bytes[1]=m_mem.GetByte(addr++);
                bytes[2]=m_mem.GetByte(addr++);
                c=AddWord(c,"$",bytes[1],bytes[2],",X");
                num_bytes=3;
                break;

            case M6502AddrMode_ABY:
                bytes[1]=m_mem.GetByte(addr++);
                bytes[2]=m_mem.GetByte(addr++);
                c=AddWord(c,"$",bytes[1],bytes[2],",Y");
                num_bytes=3;
                break;

            case M6502AddrMode_INX:
                bytes[1]=m_mem.GetByte(addr++);
                c=AddByte(c,"($",bytes[1],",X)");
                num_bytes=2;
                break;

            case M6502AddrMode_INY:
                bytes[1]=m_mem.GetByte(addr++);
                c=AddByte(c,"($",bytes[1],"),Y");
                num_bytes=2;
                break;

            case M6502AddrMode_IND:
                bytes[1]=m_mem.GetByte(addr++);
                bytes[2]=m_mem.GetByte(addr++);
                c=AddWord(c,"($",bytes[1],bytes[2],")");
                num_bytes=3;
                break;

            case M6502AddrMode_ACC:
                *c++='A';
                num_bytes=1;
                break;

            case M6502AddrMode_INZ:
                bytes[1]=m_mem.GetByte(addr++);
                c=AddByte(c,"($",bytes[1],")");
                num_bytes=2;
                break;

            case M6502AddrMode_INDX:
                bytes[1]=m_mem.GetByte(addr++);
                bytes[2]=m_mem.GetByte(addr++);
                c=AddWord(c,"($",bytes[1],bytes[2],",X)");
                num_bytes=2;
                break;
            }

            *c++=0;
            ASSERT(c<=disassembly_text+sizeof disassembly_text);

            c=bytes_text;

            for(size_t i=0;i<3;++i) {
                if(i>0) {
                    *c++=' ';
                }

                if(i<num_bytes) {
                    *c++=HEX_CHARS_LC[bytes[i]>>4&15];
                    *c++=HEX_CHARS_LC[bytes[i]&15];
                } else {
                    *c++=' ';
                    *c++=' ';
                }
            }

            *c++=0;
            ASSERT(c<=bytes_text+sizeof bytes_text);

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


            ImGui::Text("%c.%04x %s %s",flat_page_code,line_addr.w,bytes_text,disassembly_text);
        }
    }
protected:
private:
    ObjectCommandContext<DisassemblyDebugWindow> m_occ;
    const std::shared_ptr<BeebThread> m_beeb_thread;
    uint16_t m_addr=0;
    BeebMemory m_mem;
    bool m_track_pc=true;

    bool IsTrackingPC() const {
        return m_track_pc;
    }

    void ToggleTrackPC() {
        m_track_pc=!m_track_pc;
    }

    static char *AddByte(char *c,const char *prefix,uint8_t value,const char *suffix) {
        while((*c=*prefix++)!=0) {
            ++c;
        }

        *c++=HEX_CHARS_LC[value>>4&15];
        *c++=HEX_CHARS_LC[value&15];

        while((*c=*suffix++)!=0) {
            ++c;
        }

        return c;
    }

    static char *AddWord(char *c,const char *prefix,uint8_t lsb,uint8_t msb,const char *suffix) {
        while((*c=*prefix++)!=0) {
            ++c;
        }

        *c++=HEX_CHARS_LC[msb>>4&15];
        *c++=HEX_CHARS_LC[msb&15];
        *c++=HEX_CHARS_LC[lsb>>4&15];
        *c++=HEX_CHARS_LC[lsb&15];

        while((*c=*suffix++)!=0) {
            ++c;
        }

        return c;
    }

    static ObjectCommandTable<DisassemblyDebugWindow> ms_command_table;
};

ObjectCommandTable<DisassemblyDebugWindow> DisassemblyDebugWindow::ms_command_table("Disassembly Window",{
    {CommandDef("toggle_track_pc","Track PC").Shortcut(SDLK_t),&DisassemblyDebugWindow::ToggleTrackPC,&DisassemblyDebugWindow::IsTrackingPC,nullptr},
});

std::unique_ptr<SettingsUI> CreateDisassemblyDebugWindow(BeebWindow *beeb_window) {
    return std::make_unique<DisassemblyDebugWindow>(beeb_window->GetBeebThread());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
