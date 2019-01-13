#include <shared/system.h>
#include "PixelMetadataUI.h"

#if VIDEO_TRACK_METADATA

#include "SettingsUI.h"
#include "BeebWindow.h"
#include "dear_imgui.h"
#include <beeb/video.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class PixelMetadataUI:
    public SettingsUI
{
public:
    explicit PixelMetadataUI(BeebWindow *beeb_window);

    void DoImGui(CommandContextStack *cc_stack) override;

    bool OnClose() override;
protected:
private:
    BeebWindow *m_beeb_window=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

PixelMetadataUI::PixelMetadataUI(BeebWindow *beeb_window):
    m_beeb_window(beeb_window)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PixelMetadataUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    if(const VideoDataUnit *unit=m_beeb_window->GetVideoDataUnitForMousePixel()) {
        if(unit->metadata.flags&VideoDataUnitMetadataFlag_HasAddress) {
            ImGui::Text("Address: $%04X",unit->metadata.address);
        } else {
            ImGui::TextUnformatted("Address:");
        }

        if(unit->metadata.flags&VideoDataUnitMetadataFlag_HasValue) {
            uint8_t x=unit->metadata.value;

            char str[4];
            if(x>=32&&x<127) {
                str[0]='\'';
                str[1]=(char)x;
                str[2]='\'';
            } else {
                str[2]=str[1]=str[0]='-';
            }
            str[3]=0;

            ImGui::Text("Value: %s %-3u ($%02x) (%%%s)",str,x,x,BINARY_BYTE_STRINGS[x]);
        } else {
            ImGui::TextUnformatted("Value:");
        }

        ImGui::Text("%s cycle",unit->metadata.flags&VideoDataUnitMetadataFlag_OddCycle?"Odd":"Even");

        ImGui::Text("6845:%s%s%s",
                    unit->metadata.flags&VideoDataUnitMetadataFlag_6845DISPEN?" DISPEN":"",
                    unit->metadata.flags&VideoDataUnitMetadataFlag_6845CUDISP?" CUDISP":"",
                    unit->metadata.flags&VideoDataUnitMetadataFlag_6845Raster0?" Raster0":"");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool PixelMetadataUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreatePixelMetadataUI(BeebWindow *beeb_window) {
    return std::make_unique<PixelMetadataUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
