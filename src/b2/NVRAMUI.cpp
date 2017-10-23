#include <shared/system.h>
#include "NVRAMUI.h"
#include "dear_imgui.h"
#include "BeebThread.h"
#include "BeebWindow.h"

#include <shared/enum_decl.h>
#include "NVRAMUI_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "NVRAMUI_private.inl"
#include <shared/enum_end.h>

#include "SettingsUI.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class NVRAMUI:
    public SettingsUI
{
public:
    explicit NVRAMUI(BeebWindow *beeb_window);

    void DoImGui(CommandContextStack *cc_stack) override;

    bool OnClose() override;
protected:
private:
    BeebWindow *m_beeb_window;
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

NVRAMUI::NVRAMUI(BeebWindow *beeb_window):
    m_beeb_window(beeb_window)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void NVRAMUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    std::shared_ptr<BeebThread> thread=m_beeb_window->GetBeebThread();
    std::vector<uint8_t> nvram=thread->GetNVRAM();

    if(nvram.empty()) {
        ImGui::Text("This computer has no non-volatile RAM.");
        return;
    } else if(nvram.size()<50) {
        ImGui::Text("%zu bytes of non-volatile RAM.",nvram.size());
        return;
    }

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool NVRAMUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateNVRAMUI(BeebWindow *beeb_window) {
    return std::make_unique<NVRAMUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
