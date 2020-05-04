#include <shared/system.h>
#include "ConfigsUI.h"
#include "dear_imgui.h"
#include "native_ui.h"
#include "b2.h"
#include "BeebWindows.h"
#include <shared/debug.h>
#include "SettingsUI.h"
#include "commands.h"
#include <beeb/DiscInterface.h>
#include "BeebWindow.h"
#include "BeebConfig.h"
#include <beeb/type.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_ROMS("roms");

static const char NEW_CONFIG_POPUP[]="new_config_popup";
static const char COPY_CONFIG_POPUP[]="copy_config_popup";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ConfigsUI:
    public SettingsUI
{
public:
    explicit ConfigsUI(BeebWindow *window);

    void DoImGui() override;

    bool OnClose() override;
protected:
private:
    BeebWindow *m_beeb_window=nullptr;
    bool m_edited=false;
    OpenFileDialog m_ofd;
    int m_config_index=-1;

    void DoROMInfoGui(const char *caption,const BeebConfig::ROM &rom,const bool *writeable);
    bool DoROMEditGui(const char *caption,BeebConfig::ROM *rom,bool *writeable);

    void DoEditConfigGui();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ConfigsUI::ConfigsUI(BeebWindow *beeb_window):
m_beeb_window(beeb_window),
m_ofd(RECENT_PATHS_ROMS)
{
    m_ofd.AddAllFilesFilter();

    const std::string &config_name=m_beeb_window->GetConfigName();

    for(size_t i=0;i<BeebWindows::GetNumConfigs();++i) {
        const BeebConfig *config=BeebWindows::GetConfigByIndex(i);
        if(config->name==config_name) {
            m_config_index=(int)i;
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *const CAPTIONS[16]={"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};

template<class BeebConfigPointerType>
const BeebConfig *ImGuiPickConfigPopup(const char *popup_name,
                                       size_t (*get_num_configs_fn)(),
                                       BeebConfigPointerType (*get_config_by_index_fn)(size_t))
{
    const BeebConfig *result=nullptr;

    if(ImGui::BeginPopup(popup_name)) {
        for(size_t i=0;i<(*get_num_configs_fn)();++i) {
            const BeebConfig *config=(*get_config_by_index_fn)(i);
            if(ImGui::MenuItem(config->name.c_str())) {
                result=config;
            }
        }

        ImGui::EndPopup();
    }

    return result;
}

static BeebConfig *GetConfigByIndex(int index) {
    if(index<0) {
        return nullptr;
    } else if((size_t)index>=BeebWindows::GetNumConfigs()) {
        return nullptr;
    } else {
        return BeebWindows::GetConfigByIndex((size_t)index);
    }
}

static bool GetBeebWindowConfigNameCallback(void *context,int index,const char **name_ptr) {
    (void)context;

    const BeebConfig *config=GetConfigByIndex(index);
    ASSERT(config);

    *name_ptr=config->name.c_str();
    return true;
}

void ConfigsUI::DoImGui() {
    ImGui::Columns(2,"configs");

    if(ImGui::Button("New...")) {
        ImGui::OpenPopup(NEW_CONFIG_POPUP);
    }

    ImGui::SameLine();

    if(ImGui::Button("Copy...")) {
        ImGui::OpenPopup(COPY_CONFIG_POPUP);
    }

    ImGui::SameLine();

    if(ImGuiConfirmButton("Delete")) {
        BeebWindows::RemoveConfigByIndex((size_t)m_config_index);
        if(m_config_index>=BeebWindows::GetNumConfigs()) {
            m_config_index=BeebWindows::GetNumConfigs()-1;
        }
    }

    {
        ImGuiItemWidthPusher width_pusher(-1);

        float y=ImGui::GetCursorPosY();
        float h=ImGui::GetWindowHeight();
        float line_height=ImGui::GetTextLineHeightWithSpacing();

        // -1 for the height in items, as there's some kind of border that isn't
        // getting accommodated otherwise. A bit ugly.
        ImGui::ListBox("##empty",
                       &m_config_index,
                       &GetBeebWindowConfigNameCallback,
                       nullptr,
                       (int)BeebWindows::GetNumConfigs(),
                       (int)(h-y)/line_height-1);
    }

    ImGui::NextColumn();

    ImGui::BeginChild("hello");

    this->DoEditConfigGui();

    ImGui::EndChild();

    ImGui::Columns(1);

    if(const BeebConfig *config=ImGuiPickConfigPopup(NEW_CONFIG_POPUP,
                                                     &GetNumDefaultBeebConfigs,
                                                     &GetDefaultBeebConfigByIndex))
    {
        BeebWindows::AddConfig(*config);
    }

    if(const BeebConfig *config=ImGuiPickConfigPopup(COPY_CONFIG_POPUP,
                                                     &BeebWindows::GetNumConfigs,
                                                     &BeebWindows::GetConfigByIndex))
    {
        BeebWindows::AddConfig(*config);
    }

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ConfigsUI::DoEditConfigGui() {
    if(BeebConfig *config=GetConfigByIndex(m_config_index)) {

        // set to true if *config was edited - as well as
        // dirtying the corresponding loaded config, this will set
        // m_edited.
        bool edited=false;

        const ImGuiStyle &style=ImGui::GetStyle();

        ImGuiIDPusher config_id_pusher(config);

        ImGui::Text("Model: %s",config->type->model_name);
        ImGui::Text("Disc interface: %s",config->disc_interface->name.c_str());

        std::string title=config->name;

        std::string name;
        {
            // with a width of -1, the label disappears...
            //ImGuiItemWidthPusher pusher(-1);

            if(ImGuiInputText(&config->name,"Name",config->name)) {
                edited=true;
            }
        }

        ImGui::Columns(3,"rom_edit",true);

        ImGui::Text("ROM");
        float rom_width=ImGui::GetItemRectSize().x+2*style.ItemSpacing.x;

        ImGui::NextColumn();

        ImGui::Text("RAM");
        float ram_width=ImGui::GetItemRectSize().x+2*style.ItemSpacing.x;

        ImGui::NextColumn();

        ImGui::Text("Contents");

        ImGui::NextColumn();

        ImGui::Separator();

        if(this->DoROMEditGui("OS",&config->os,nullptr)) {
            edited=true;
        }

        uint16_t occupied=0;

        for(uint8_t i=0;i<16;++i) {
            uint8_t bank=15-i;

            {
                ImGuiIDPusher bank_id_pusher(bank);

                ImGui::Separator();

                BeebConfig::SidewaysROM *editable_rom=&config->roms[bank];

                if(this->DoROMEditGui(CAPTIONS[bank],
                                      editable_rom,
                                      &editable_rom->writeable))
                {
                    edited=true;
                }

                occupied|=1<<bank;
            }
        }

        ImGui::SetColumnOffset(1,rom_width);
        ImGui::SetColumnOffset(2,rom_width+ram_width);

        ImGui::Separator();

        ImGui::Columns(1);

        if(!config->disc_interface->uses_1MHz_bus) {
            if(ImGui::Checkbox("External memory",&config->ext_mem)) {
                edited=true;
            }
        }

        if(ImGui::Checkbox("BeebLink",&config->beeblink)) {
            edited=true;
        }

        if(ImGui::Checkbox("Video NuLA",&config->video_nula)) {
            edited=true;
        }

        if(edited) {
            BeebWindows::ConfigDidChange((size_t)m_config_index);
            m_edited=true;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ConfigsUI::OnClose() {
    return m_edited;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ConfigsUI::DoROMInfoGui(const char *caption,
                             const BeebConfig::ROM &rom,
                             const bool *writeable)
{
    ImGuiIDPusher id_pusher(caption);

    ImGui::AlignTextToFramePadding();

    ImGui::TextUnformatted(caption);

    ImGui::NextColumn();

    if(writeable) {
        bool value=*writeable;
        ImGui::Checkbox("##ram",&value);
    }

    ImGui::NextColumn();

    if(rom.standard_rom) {
        ImGui::Text("*%s*",rom.standard_rom->name.c_str());
    } else {
        ImGui::TextUnformatted(rom.file_name.c_str());
    }

    ImGui::NextColumn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char ROM_POPUP[]="rom_popup";

static bool ImGuiROM(BeebConfig::ROM *rom,const BeebROM *beeb_rom) {
    if(ImGui::MenuItem(beeb_rom->name.c_str())) {
        rom->file_name.clear();
        rom->standard_rom=beeb_rom;
        return true;
    } else {
        return false;
    }
}

static bool ImGuiMasterROMs(BeebConfig::ROM *rom,const BeebROM *master_roms) {
    for(size_t i=0;i<8;++i) {
        if(ImGuiROM(rom,&master_roms[7-i])) {
            return true;
        }
    }

    return false;
}

static const BeebROM *const B_ROMS[]={
    &BEEB_ROM_OS12,
    &BEEB_ROM_BASIC2,
    &BEEB_ROM_ACORN_DFS,
    &BEEB_ROM_WATFORD_DDFS_DDB2,
    &BEEB_ROM_WATFORD_DDFS_DDB3,
    &BEEB_ROM_OPUS_DDOS,
    &BEEB_ROM_OPUS_CHALLENGER,
    nullptr,
};

static const BeebROM *const BPLUS_ROMS[]={
    &BEEB_ROM_BPLUS_MOS,
    &BEEB_ROM_BASIC2,
    &BEEB_ROM_ACORN_DFS,
    nullptr,
};

static bool ImGuiBROMs(BeebConfig::ROM *rom,const BeebROM *const *b_roms) {
    for(size_t i=0;b_roms[i];++i) {
        if(ImGuiROM(rom,b_roms[i])) {
            return true;
        }
    }

    return false;
}

bool ConfigsUI::DoROMEditGui(const char *caption,BeebConfig::ROM *rom,bool *writeable) {
    bool edited=false;

    ImGuiIDPusher id_pusher(caption);

    // doesn't seem to make any difference.
    //ImGui::AlignFirstTextHeightToWidgets();

    ImGui::TextUnformatted(caption);

    ImGui::NextColumn();

    if(writeable) {
        if(ImGui::Checkbox("##ram",writeable)) {
            edited=true;
        }
    }

    ImGui::NextColumn();

    if(ImGui::Button("...")) {
        ImGui::OpenPopup(ROM_POPUP);
    }

    ImGui::SameLine();


    {
        ImGuiItemWidthPusher pusher(-1);

        if(rom->standard_rom) {
            ImGui::TextUnformatted(rom->standard_rom->name.c_str());
        } else {
            if(ImGuiInputText(&rom->file_name,"##name",rom->file_name)) {
                edited=true;
            }
        }
    }

    if(ImGui::BeginPopup(ROM_POPUP)) {
        if(ImGui::MenuItem("File...")) {
            if(m_ofd.Open(&rom->file_name)) {
                rom->standard_rom=nullptr;
                edited=true;
                m_ofd.AddLastPathToRecentPaths();
            }
        }

        if(ImGuiRecentMenu(&rom->file_name,"Recent file",m_ofd)) {
            rom->standard_rom=nullptr;
            edited=true;
        }

        ImGui::Separator();

        if(ImGui::MenuItem("(empty)")) {
            rom->standard_rom=nullptr;
            rom->file_name.clear();
            edited=true;
        }

        if(ImGui::BeginMenu("B ROMs")) {
            if(ImGuiBROMs(rom,B_ROMS)) {
                edited=true;
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("B+ ROMs")) {
            if(ImGuiBROMs(rom,BPLUS_ROMS)) {
                edited=true;
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("MOS 3.20 ROMs")) {
            if(ImGuiMasterROMs(rom,BEEB_ROMS_MOS320)) {
                edited=true;
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("MOS 3.50 ROMs")) {
            if(ImGuiMasterROMs(rom,BEEB_ROMS_MOS350)) {
                edited=true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::NextColumn();

    return edited;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateConfigsUI(BeebWindow *beeb_window) {
    return std::make_unique<ConfigsUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
