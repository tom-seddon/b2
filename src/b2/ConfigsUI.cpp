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

    void DoImGui() override;

    bool OnClose() override;
protected:
private:
    bool m_edited=false;

    void DoROMInfoGui(const char *caption,const BeebConfig::ROM &rom,const bool *writeable);
    bool DoROMEditGui(const char *caption,BeebConfig::ROM *rom,bool *writeable);

    bool DoEditConfigGui(const BeebConfig *config,BeebConfig *editable_config);

    bool DoFileSelect(std::string *file_name);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ConfigsUI::ConfigsUI() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *const CAPTIONS[16]={"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};

void ConfigsUI::DoImGui() {
    const BeebConfig *copy_config=nullptr;
    BeebConfig *delete_config=nullptr;

    BeebWindows::ForEachConfig([&](const BeebConfig *config,BeebConfig *editable_config) {
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
                if(!copy_config) {
                    copy_config=config;
                }
            }

            if(editable_config) {
                ImGui::SameLine();

                if(ImGui::Button("Delete")) {
                    if(!delete_config) {
                        delete_config=editable_config;
                    }
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
                if(this->DoROMEditGui("OS",&editable_config->os,nullptr)) {
                    edited=true;
                }
            } else {
                this->DoROMInfoGui("OS",config->os,nullptr);
            }

            uint16_t occupied=0;

            for(uint8_t i=0;i<16;++i) {
                uint8_t bank=15-i;
                const BeebConfig::SidewaysROM *rom=&config->roms[bank];

                //if(!rom->file_name.empty()||rom->standard_rom||rom->writeable)
                {
                    ImGuiIDPusher bank_id_pusher(bank);

                    ImGui::Separator();

                    if(editable_config) {
                        BeebConfig::SidewaysROM *editable_rom=&editable_config->roms[bank];

                        if(this->DoROMEditGui(CAPTIONS[bank],
                                              editable_rom,
                                              &editable_rom->writeable))
                        {
                            edited=true;
                        }
                    } else {
                        this->DoROMInfoGui(CAPTIONS[bank],
                                           *rom,
                                           &rom->writeable);
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
    });

    // If somebody somehow manages to click both in one update, copy
    // takes priority.
    if(copy_config) {
        BeebWindows::AddConfig(*copy_config);
        m_edited=true;
    } else if(delete_config) {
        BeebWindows::RemoveConfigByName(delete_config->name);
        m_edited=true;
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

    if(rom->standard_rom) {
        ImGui::Text("*%s",rom->standard_rom->name.c_str());
    } else {
        if(ImGuiInputText(&rom->file_name,"##name",rom->file_name)) {
            edited=true;
        }
    }

    ImGui::SameLine();
    if(ImGui::Button("...")) {
        ImGui::OpenPopup(ROM_POPUP);
    }

    if(ImGui::BeginPopup(ROM_POPUP)) {
        if(ImGui::MenuItem("File...")) {
            if(this->DoFileSelect(&rom->file_name)) {
                rom->standard_rom=nullptr;
                edited=true;
            }
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
//)
//        if(this->DoFileSelect(file_name)) {
//            edited=true;
//        }
//    }
//
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
