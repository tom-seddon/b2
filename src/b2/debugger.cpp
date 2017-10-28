#include <shared/system.h>
#include "debugger.h"

#if BBCMICRO_DEBUGGER

#include "dear_imgui.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <shared/log.h>
#include <imgui_memory_editor.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(DBG,"debugger","DBG   ",&log_printer_stdout_and_debugger,true)

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
    }

    void DoImGui(CommandContextStack *cc_stack) {
        (void)cc_stack;
        //m_beeb_thread->SendUpdate6502StateMessage();

        {
            std::unique_lock<std::mutex> lock;
            if(const M6502 *s=m_beeb_thread->Get6502State(&lock)) {
                this->Reg("A",s->a);
                this->Reg("X",s->x);
                this->Reg("Y",s->y);
                ImGui::Text("PC = $%04x",s->pc.w);
                ImGui::Text("S = $01%02X",s->s.b.l);

                M6502P p=M6502_GetP(s);
                char pstr[9];
                ImGui::Text("P = $%02x %s",p.value,M6502P_GetString(pstr,p));
            } else {
                ImGui::TextUnformatted("6502 state not available");
            }
        }
    }
protected:
private:
    const std::shared_ptr<BeebThread> m_beeb_thread;

    void Reg(const char *name,uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s",name,value,value,BINARY_BYTE_STRINGS[value]);
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
protected:
private:
    const std::shared_ptr<BeebThread> m_beeb_thread;
    uint8_t m_buffer[256]={};
    int m_buffer_page;
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

    if(m_buffer_page!=addr.b.h) {
        M6502Word page_addr=addr;
        page_addr.b.l=0;

        m_beeb_thread->DebugCopyMemory(m_buffer,page_addr,256);
        m_buffer_page=addr.b.h;
    }

    return m_buffer[addr.b.l];
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

static const char HEX[]="0123456789ABCDEF";

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
            std::unique_lock<std::mutex> lock;
            const M6502 *s=m_beeb_thread->Get6502State(&lock);

            config=s->config;
            pc=s->opcode_pc.w;
        }

        float height=ImGui::GetCurrentWindow()->Size.y;

        m_occ.DoToggleCheckboxUI("toggle_track_pc");

        if(m_track_pc) {
            m_addr=pc;
        }

        uint16_t addr=m_addr;
        while(ImGui::GetCursorPosY()<height) {
            uint16_t line_addr=addr;
            char disassembly_text[50],bytes_text[50];
            uint8_t bytes[3];
            size_t num_bytes;

            char *c=disassembly_text;
            num_bytes=0;

            bytes[0]=m_mem.GetByte(addr++);
            const M6502DisassemblyInfo *di=&config->disassembly_info[bytes[0]];

            memcpy(c,di->mnemonic,di->mnemonic_length);
            c+=di->mnemonic_length;
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

                    c=AddWord(c,"$",dest.b.h,dest.b.l,"");
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
                    *c++=HEX[bytes[i]>>4&15];
                    *c++=HEX[bytes[i]&15];
                } else {
                    *c++=' ';
                    *c++=' ';
                }
            }

            *c++=0;
            ASSERT(c<=bytes_text+sizeof bytes_text);

            ImGuiStyleColourPusher pusher;

            if(line_addr==pc) {
                pusher.Push(ImGuiCol_Text,ImVec4(1.f,1.f,0.f,1.f));
            }

            ImGui::Text("%04x %s %s",line_addr,bytes_text,disassembly_text);
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

        *c++=HEX[value>>4&15];
        *c++=HEX[value&15];

        while((*c=*suffix++)!=0) {
            ++c;
        }

        return c;
    }

    static char *AddWord(char *c,const char *prefix,uint8_t lsb,uint8_t msb,const char *suffix) {
        while((*c=*prefix++)!=0) {
            ++c;
        }

        *c++=HEX[msb>>4&15];
        *c++=HEX[msb&15];
        *c++=HEX[lsb>>4&15];
        *c++=HEX[lsb&15];

        while((*c=*suffix++)!=0) {
            ++c;
        }

        return c;
    }

    static ObjectCommandTable<DisassemblyDebugWindow> ms_command_table;
};

ObjectCommandTable<DisassemblyDebugWindow> DisassemblyDebugWindow::ms_command_table("Disassembly Window",{
    {"toggle_track_pc","Track PC",&DisassemblyDebugWindow::ToggleTrackPC,&DisassemblyDebugWindow::IsTrackingPC,nullptr},
});

std::unique_ptr<SettingsUI> CreateDisassemblyDebugWindow(BeebWindow *beeb_window) {
    return std::make_unique<DisassemblyDebugWindow>(beeb_window->GetBeebThread());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
