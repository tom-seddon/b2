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

class MemoryDebugWindow:
    public DebugUI
{
public:
    explicit MemoryDebugWindow(std::shared_ptr<BeebThread> beeb_thread):
        m_beeb_thread(std::move(beeb_thread))
    {
        m_memory_editor.ReadFn=&MemoryEditorRead;
        m_memory_editor.WriteFn=&MemoryEditorWrite;
    }

    void DoImGui(CommandContextStack *cc_stack) {
        (void)cc_stack;

        m_memory_editor.DrawContents((uint8_t *)this,65536,0);
    }
protected:
private:
    const std::shared_ptr<BeebThread> m_beeb_thread;
    M6502Word m_addr{};
    MemoryEditor m_memory_editor;
    uint8_t m_page_buffer[256]={};
    int m_page_buffer_page=-1;

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static uint8_t MemoryEditorRead(uint8_t *data,size_t off) {
        auto self=(MemoryDebugWindow *)data;

        M6502Word addr={(uint16_t)off};
        ASSERT(addr.w==off);

        if(self->m_page_buffer_page!=addr.b.h) {
            M6502Word page_addr=addr;
            addr.b.l=0;

            self->m_beeb_thread->DebugCopyMemory(self->m_page_buffer,page_addr,256);
            self->m_page_buffer_page=addr.b.h;
        }

        return self->m_page_buffer[addr.b.l];
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

#endif
