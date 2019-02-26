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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_ROMS("roms");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ConfigsUI:
    public SettingsUI
{
public:
    ConfigsUI();

    void DoImGui(CommandContextStack *cc_stack) override;

    bool OnClose() override;
protected:
private:
    bool m_edited=false;

    void DoROMInfoGui(const char *caption,const std::string &file_name,const bool *writeable);
    bool DoROMEditGui(const char *caption,std::string *file_name,bool *writeable);

    bool DoEditConfigGui(const BeebConfig *config,BeebConfig *editable_config);

    bool DoFileSelect(std::string *file_name);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ConfigsUI::ConfigsUI() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ConfigsUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    BeebWindows::ForEachConfig([&](const BeebConfig *config,BeebConfig *editable_config) {
        return this->DoEditConfigGui(config,editable_config);
    });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ConfigsUI::OnClose() {
    return m_edited;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *const CAPTIONS[16]={"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};

bool ConfigsUI::DoEditConfigGui(const BeebConfig *config,BeebConfig *editable_config) {
    // set to true if *editable_config was edited - as well as
    // dirtying the corresponding loaded config, this will set
    // m_edited.
    bool edited=false;
    bool is_default=config->name==BeebWindows::GetDefaultConfigName();

    const ImGuiStyle &style=ImGui::GetStyle();

    ImGuiIDPusher config_id_pusher(config);

    std::string title=config->name;
    if(!editable_config) {
        title+=" (not editable)";
    }

    if(is_default) {
        title+=" (default)";
    }

    if(ImGui::CollapsingHeader(title.c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
        if(editable_config) {
            std::string name;
            if(ImGuiInputText(&editable_config->name,"Name",editable_config->name)) {
                edited=true;
            }
        }

        if(ImGui::Button("Copy")) {
            BeebWindows::AddConfig(*config);
            m_edited=true;
        }

        if(editable_config) {
            ImGui::SameLine();

            if(ImGui::Button("Delete")) {
                BeebWindows::RemoveConfigByName(editable_config->name);
                m_edited=true;
                return false;
            }
        }

        if(!is_default) {
            ImGui::SameLine();

            if(ImGui::Button("Set as default")) {
                BeebWindows::SetDefaultConfig(config->name);
                m_edited=true;
            }
        }

        ImGui::Columns(3,"rom_edit",true);

        ImGui::Text("ROM");
        float rom_width=ImGui::GetItemRectSize().x+2*style.ItemSpacing.x;

        ImGui::NextColumn();

        ImGui::Text("RAM");
        float ram_width=ImGui::GetItemRectSize().x+2*style.ItemSpacing.x;

        ImGui::NextColumn();

        ImGui::Text("File");

        ImGui::NextColumn();

        ImGui::Separator();

        if(editable_config) {
            if(this->DoROMEditGui("OS",&editable_config->os_file_name,nullptr)) {
                edited=true;
            }
        } else {
            this->DoROMInfoGui("OS",config->os_file_name,nullptr);
        }

        uint16_t occupied=0;

        for(uint8_t i=0;i<16;++i) {
            uint8_t bank=15-i;
            const BeebConfig::ROM *rom=&config->roms[bank];

            if(!rom->file_name.empty()||rom->writeable) {
                ImGuiIDPusher bank_id_pusher(bank);

                ImGui::Separator();

                if(editable_config) {
                    BeebConfig::ROM *editable_rom=&editable_config->roms[bank];

                    if(this->DoROMEditGui(CAPTIONS[bank],&editable_rom->file_name,&editable_rom->writeable)) {
                        edited=true;
                    }
                } else {
                    this->DoROMInfoGui(CAPTIONS[bank],rom->file_name,&rom->writeable);
                }

                occupied|=1<<bank;
            }
        }

        ImGui::SetColumnOffset(1,rom_width);
        ImGui::SetColumnOffset(2,rom_width+ram_width);

        ImGui::Separator();

        ImGui::Columns(1);

        if(editable_config) {
            if(!editable_config->disc_interface->uses_1MHz_bus) {
                if(ImGui::Checkbox("External memory",&editable_config->ext_mem)) {
                    edited=true;
                }
            }

            if(ImGui::Checkbox("BeebLink",&editable_config->beeblink)) {
                edited=true;
            }

            if(occupied!=0xffff) {
                {
                    ImGuiIDPusher ram_id_pusher("ram");

                    ImGui::TextUnformatted("Add RAM:");

                    for(uint8_t i=0;i<16;++i) {
                        uint8_t bank=15-i;
                        if(!(occupied&1<<bank)) {
                            ImGui::SameLine();

                            if(ImGui::Button(CAPTIONS[bank])) {
                                editable_config->roms[bank].writeable=true;
                                edited=true;
                            }
                        }
                    }
                }

                {
                    ImGuiIDPusher rom_id_pusher("rom");

                    ImGui::TextUnformatted("Add ROM:");

                    for(uint8_t i=0;i<16;++i) {
                        uint8_t bank=15-i;
                        if(!(occupied&1<<bank)) {
                            ImGui::SameLine();

                            if(ImGui::Button(CAPTIONS[bank])) {
                                if(this->DoFileSelect(&editable_config->roms[bank].file_name)) {
                                    edited=true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(edited) {
        BeebWindows::ConfigDidChange(editable_config->name);
        m_edited=true;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ConfigsUI::DoROMInfoGui(const char *caption,const std::string &file_name,const bool *writeable) {
    ImGuiIDPusher id_pusher(caption);

    ImGui::AlignTextToFramePadding();

    ImGui::TextUnformatted(caption);

    ImGui::NextColumn();

    if(writeable) {
        bool value=*writeable;
        ImGui::Checkbox("##ram",&value);
    }

    ImGui::NextColumn();

    ImGui::TextUnformatted(file_name.c_str());

    ImGui::NextColumn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ConfigsUI::DoROMEditGui(const char *caption,std::string *file_name,bool *writeable) {
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

    if(ImGuiInputText(file_name,"##name",*file_name)) {
        edited=true;
    }

    ImGui::SameLine();
    if(ImGui::Button("...")) {
        if(this->DoFileSelect(file_name)) {
            edited=true;
        }
    }

    ImGui::NextColumn();

    return edited;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool ConfigsUI::DoFileSelect(std::string *file_name) {
    OpenFileDialog fd(RECENT_PATHS_ROMS);

    fd.AddAllFilesFilter();

    if(!fd.Open(file_name)) {
        return false;
    }

    fd.AddLastPathToRecentPaths();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateConfigsUI(BeebWindow *beeb_window) {
    (void)beeb_window;

    return std::make_unique<ConfigsUI>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
