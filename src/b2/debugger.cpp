#include <shared/system.h>
#include "debugger.h"

#if BBCMICRO_DEBUGGER

#include "dear_imgui.h"
#include "BeebThread.h"

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
        m_beeb_thread->SendUpdate6502StateMessage();

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

std::unique_ptr<SettingsUI> Create6502DebugWindow(std::shared_ptr<BeebThread> beeb_thread) {
    return std::make_unique<M6502DebugWindow>(std::move(beeb_thread));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif