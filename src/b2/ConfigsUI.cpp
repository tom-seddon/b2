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
#include <IconsFontAwesome5.h>
#include <beeb/BBCMicro.h>

#include <shared/enum_decl.h>
#include "ConfigsUI_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "ConfigsUI_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_ROMS("roms");

static const char NEW_CONFIG_POPUP[] = "new_config_popup";
static const char COPY_CONFIG_POPUP[] = "copy_config_popup";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ConfigsUI : public SettingsUI {
  public:
    explicit ConfigsUI(BeebWindow *window);

    void DoImGui() override;

    bool OnClose() override;

  protected:
  private:
    BeebWindow *m_beeb_window = nullptr;
    bool m_edited = false;
    OpenFileDialog m_ofd;
    int m_config_index = -1;

    void DoROMInfoGui(const char *caption, const BeebConfig::ROM &rom, const bool *writeable);

    // rom_edit_flags is a combination of ROMEditFlag values
    ROMEditAction DoROMEditGui(const char *caption, BeebConfig::ROM *rom, bool *writeable, ROMType *type, uint32_t rom_edit_flags);
    void DoROMs(BeebConfig::ROM *rom,
                bool *edited,
                uint32_t rom_edit_flags,
                uint32_t rom_edit_flag,
                const char *label,
                const BeebROM *const *roms);

    void DoEditConfigGui();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ConfigsUI::ConfigsUI(BeebWindow *beeb_window)
    : m_beeb_window(beeb_window)
    , m_ofd(RECENT_PATHS_ROMS) {
    this->SetDefaultSize(ImVec2(650, 450));

    m_ofd.AddAllFilesFilter();

    const std::string &config_name = m_beeb_window->GetConfigName();

    for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
        const BeebConfig *config = BeebWindows::GetConfigByIndex(i);
        if (config->name == config_name) {
            m_config_index = (int)i;
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *const CAPTIONS[16] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"};

template <class BeebConfigPointerType>
const BeebConfig *ImGuiPickConfigPopup(const char *popup_name,
                                       size_t (*get_num_configs_fn)(),
                                       BeebConfigPointerType (*get_config_by_index_fn)(size_t)) {
    const BeebConfig *result = nullptr;

    if (ImGui::BeginPopup(popup_name)) {
        for (size_t i = 0; i < (*get_num_configs_fn)(); ++i) {
            const BeebConfig *config = (*get_config_by_index_fn)(i);
            if (ImGui::MenuItem(config->name.c_str())) {
                result = config;
            }
        }

        ImGui::EndPopup();
    }

    return result;
}

static BeebConfig *GetConfigByIndex(int index) {
    if (index < 0) {
        return nullptr;
    } else if ((size_t)index >= BeebWindows::GetNumConfigs()) {
        return nullptr;
    } else {
        return BeebWindows::GetConfigByIndex((size_t)index);
    }
}

static const char *GetBeebWindowConfigNameCallback(void *context, int index) {
    (void)context;

    const BeebConfig *config = GetConfigByIndex(index);
    ASSERT(config);

    return config->name.c_str();
}

void ConfigsUI::DoImGui() {
    ImGui::Columns(2, "configs");

    if (ImGui::Button("New...")) {
        ImGui::OpenPopup(NEW_CONFIG_POPUP);
    }

    ImGui::SameLine();

    if (ImGui::Button("Copy...")) {
        ImGui::OpenPopup(COPY_CONFIG_POPUP);
    }

    ImGui::SameLine();

    if (ImGuiConfirmButton("Delete")) {
        if (m_config_index >= 0) {
            BeebWindows::RemoveConfigByIndex((size_t)m_config_index);
            if ((size_t)m_config_index >= BeebWindows::GetNumConfigs()) {
                m_config_index = (int)(BeebWindows::GetNumConfigs() - 1);
            }
        }
    }

    {
        ImGuiItemWidthPusher width_pusher(-1);

        float y = ImGui::GetCursorPosY();
        float h = ImGui::GetWindowHeight();
        float line_height = ImGui::GetTextLineHeightWithSpacing();

        // -1 for the height in items, as there's some kind of border that isn't
        // getting accommodated otherwise. A bit ugly.
        ImGui::ListBox("##empty",
                       &m_config_index,
                       &GetBeebWindowConfigNameCallback,
                       nullptr,
                       (int)BeebWindows::GetNumConfigs(),
                       (int)((h - y) / line_height) - 1);
    }

    ImGui::NextColumn();

    ImGui::BeginChild("hello");

    this->DoEditConfigGui();

    ImGui::EndChild();

    ImGui::Columns(1);

    if (const BeebConfig *config = ImGuiPickConfigPopup(NEW_CONFIG_POPUP,
                                                        &GetNumDefaultBeebConfigs,
                                                        &GetDefaultBeebConfigByIndex)) {
        BeebWindows::AddConfig(*config);
    }

    if (const BeebConfig *config = ImGuiPickConfigPopup(COPY_CONFIG_POPUP,
                                                        &BeebWindows::GetNumConfigs,
                                                        &BeebWindows::GetConfigByIndex)) {
        BeebWindows::AddConfig(*config);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *GetADJIDIPSwitchesString(void *data, int index) {
    ASSERT(index >= 0 && index < 4);

    auto tmp = (std::string *)data;
    *tmp = strprintf("%d (&%04X) (DIP 1=%s, DIP 2=%s)", 1 + index, BBCMicro::ADJI_ADDRESSES[index], index & 1 ? "ON" : "OFF", index & 2 ? "ON" : "OFF");

    return tmp->c_str();
}

void ConfigsUI::DoEditConfigGui() {
    BeebConfig *config = GetConfigByIndex(m_config_index);
    if (!config) {
        return;
    }

    uint32_t rom_edit_sideways_rom_flags;
    uint32_t rom_edit_os_rom_flags;
    switch (config->type_id) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroTypeID_B:
        rom_edit_sideways_rom_flags = ROMEditFlag_BSidewaysROMs;
        rom_edit_os_rom_flags = ROMEditFlag_BOSROMs;
        break;

    case BBCMicroTypeID_BPlus:
        rom_edit_sideways_rom_flags = ROMEditFlag_BPlusSidewaysROMs;
        rom_edit_os_rom_flags = ROMEditFlag_BPlusOSROMs;
        break;

    case BBCMicroTypeID_Master:
        rom_edit_sideways_rom_flags = ROMEditFlag_Master128SidewaysROMs;
        rom_edit_os_rom_flags = ROMEditFlag_Master128OSROMs;
        break;

    case BBCMicroTypeID_MasterCompact:
        rom_edit_sideways_rom_flags = ROMEditFlag_MasterCompactSidewaysROMs;
        rom_edit_os_rom_flags = ROMEditFlag_MasterCompactOSROMs;
        break;
    }

    // set to true if *config was edited - as well as
    // dirtying the corresponding loaded config, this will set
    // m_edited.
    bool edited = false;

    const ImGuiStyle &style = ImGui::GetStyle();

    ImGuiIDPusher config_id_pusher(config);

    ImGui::Text("Model: %s", GetModelName(config->type_id));
    ImGui::Text("Disc interface: %s", config->disc_interface->display_name.c_str());

    std::string title = config->name;

    std::string name;
    {
        // with a width of -1, the label disappears...
        //ImGuiItemWidthPusher pusher(-1);

        if (ImGuiInputText(&config->name, "Name", config->name)) {
            edited = true;
        }
    }

    ImGui::Columns(3, "rom_edit", true);

    ImGui::Text("ROM");
    float rom_width = ImGui::GetItemRectSize().x + 5 * style.ItemSpacing.x;

    ImGui::NextColumn();

    ImGui::Text("RAM");
    float ram_width = ImGui::GetItemRectSize().x + 2 * style.ItemSpacing.x;

    ImGui::NextColumn();

    ImGui::Text("Contents");

    ImGui::NextColumn();

    ImGui::Separator();

    if (this->DoROMEditGui("Host OS",
                           &config->os,
                           nullptr,
                           nullptr,
                           rom_edit_os_rom_flags) != ROMEditAction_None) {
        edited = true;
    }

    ROMEditAction action = ROMEditAction_None;
    uint8_t action_bank = 0;

    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t bank = 15 - i;

        {
            ImGuiIDPusher bank_id_pusher(bank);

            ImGui::Separator();

            BeebConfig::SidewaysROM *rom = &config->roms[bank];

            uint32_t rom_edit_flags = (bank < 15 ? (uint32_t)ROMEditFlag_CanMoveUp : 0) |
                                      (bank > 0 ? (uint32_t)ROMEditFlag_CanMoveDown : 0) |
                                      rom_edit_sideways_rom_flags;

            ROMEditAction a = this->DoROMEditGui(CAPTIONS[bank],
                                                 rom,
                                                 &rom->writeable,
                                                 &rom->type,
                                                 rom_edit_flags);
            if (a != ROMEditAction_None) {
                action = a;
                action_bank = bank;
                edited = true;
            }
        }
    }

    switch (action) {
    case ROMEditAction_None:
    case ROMEditAction_Edit:
        break;

    case ROMEditAction_MoveUp:
        ASSERT(action_bank < 15);
        std::swap(config->roms[action_bank], config->roms[action_bank + 1]);
        break;

    case ROMEditAction_MoveDown:
        ASSERT(action_bank > 0);
        std::swap(config->roms[action_bank], config->roms[action_bank - 1]);
        break;
    }

    ImGui::SetColumnOffset(1, rom_width);
    ImGui::SetColumnOffset(2, rom_width + ram_width);

    ImGui::Separator();

    ImGui::Columns(1);

    if (Has1MHzBus(config->type_id)) {
        if (!(config->disc_interface->flags & DiscInterfaceFlag_Uses1MHzBus)) {
            if (ImGui::Checkbox("External memory", &config->ext_mem)) {
                edited = true;
            }
        }
    }

    if (HasUserPort(config->type_id)) {
        if (ImGui::Checkbox("BeebLink", &config->beeblink)) {
            edited = true;
        }
    }

    if (ImGui::Checkbox("Video NuLA", &config->video_nula)) {
        edited = true;
    }

    if (HasCartridges(config->type_id)) {
        if (ImGui::Checkbox("Retro Hardware ADJI cartridge", &config->adji)) {
            edited = true;
        }

        if (config->adji) {
            std::string tmp;
            int adji_dip_switches = config->adji_dip_switches & 3;
            if (ImGui::ListBox("Address", &adji_dip_switches, &GetADJIDIPSwitchesString, &tmp, 4)) {
                config->adji_dip_switches = adji_dip_switches & 3;
                edited = true;
            }
        }
    }

    if (ImGui::Checkbox("Mouse", &config->mouse)) {
        edited = true;
    }

    if (HasTube(config->type_id)) {
        if (ImGuiRadioButton(&config->parasite_type, BBCMicroParasiteType_None, "No second processor")) {
            edited = true;
        }

        if (ImGuiRadioButton(&config->parasite_type, BBCMicroParasiteType_External3MHz6502, "6502 Second Processor")) {
            edited = true;
        }

        if (ImGuiRadioButton(&config->parasite_type, BBCMicroParasiteType_MasterTurbo, "Master Turbo")) {
            edited = true;
        }

        if (config->parasite_type != BBCMicroParasiteType_None) {
            if (config->parasite_os.file_name.empty() && !config->parasite_os.standard_rom) {
                switch (config->parasite_type) {
                case BBCMicroParasiteType_None:
                    // inhibit spurious warning
                    break;

                case BBCMicroParasiteType_External3MHz6502:
                    config->parasite_os.standard_rom = FindBeebROM(StandardROM_TUBE110);
                    break;
                case BBCMicroParasiteType_MasterTurbo:
                    config->parasite_os.standard_rom = FindBeebROM(StandardROM_MasterTurboParasite);
                    break;
                }
            }

            if (this->DoROMEditGui("Parasite OS",
                                   &config->parasite_os,
                                   nullptr,
                                   nullptr,
                                   ROMEditFlag_ParasiteROMs)) {
                edited = true;
            }

            if (config->type_id == BBCMicroTypeID_Master) {
                ImGui::TextWrapped("Note: When using MOS 3.20/MOS 3.50, try *CONFIGURE TUBE if 2nd processor doesn't seem to be working");
            } else {
                ImGui::TextWrapped("Note: Ensure a ROM with Tube host code is installed, e.g., Acorn 1770 DFS");
            }
        }
    }

    if (edited) {
        BeebWindows::ConfigDidChange((size_t)m_config_index);
        m_edited = true;
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
                             const bool *writeable) {
    ImGuiIDPusher id_pusher(caption);

    ImGui::AlignTextToFramePadding();

    ImGui::TextUnformatted(caption);

    ImGui::NextColumn();

    if (writeable) {
        bool value = *writeable;
        ImGui::Checkbox("##ram", &value);
    }

    ImGui::NextColumn();

    if (rom.standard_rom) {
        ImGui::Text("*%s*", rom.standard_rom->name.c_str());
    } else {
        ImGui::TextUnformatted(rom.file_name.c_str());
    }

    ImGui::NextColumn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char ROM_POPUP[] = "rom_popup";

static bool ImGuiROM(BeebConfig::ROM *rom, const BeebROM *beeb_rom) {
    if (ImGui::MenuItem(beeb_rom->name.c_str())) {
        rom->file_name.clear();
        rom->standard_rom = beeb_rom;
        return true;
    } else {
        return false;
    }
}

//static bool ImGuiMasterROMs(BeebConfig::ROM *rom, const BeebROM *master_roms) {
//    for (size_t i = 0; i < 8; ++i) {
//        if (ImGuiROM(rom, &master_roms[7 - i])) {
//            return true;
//        }
//    }
//
//    return false;
//}

static const BeebROM *const B_OS_ROMS[] = {
    &BEEB_ROM_OS12,
    nullptr,
};

static const BeebROM *const B_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_BASIC2,
    &BEEB_ROM_ACORN_DFS,
    &BEEB_ROM_WATFORD_DDFS_DDB2,
    &BEEB_ROM_WATFORD_DDFS_DDB3,
    &BEEB_ROM_OPUS_DDOS,
    &BEEB_ROM_OPUS_CHALLENGER,
    nullptr,
};

static const BeebROM *const BPLUS_OS_ROMS[] = {
    &BEEB_ROM_BPLUS_MOS,
    nullptr,
};

static const BeebROM *const BPLUS_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_BASIC2,
    &BEEB_ROM_ACORN_DFS,
    nullptr,
};

static const BeebROM *const PARASITE_ROMS[] = {
    &BEEB_ROM_TUBE110,
    &BEEB_ROM_MASTER_TURBO_PARASITE,
    nullptr,
};

static const BeebROM *const MOS320_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_F,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_C,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_B,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_A,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_9,
    nullptr,
};

static const BeebROM *const MOS320_MOS_ROMS[] = {
    &BEEB_ROM_MOS320_MOS_ROM,
    nullptr,
};

static const BeebROM *const MOS350_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_F,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_C,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_B,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_A,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_9,
    nullptr,
};

static const BeebROM *const MOS350_MOS_ROMS[] = {
    &BEEB_ROM_MOS350_MOS_ROM,
    nullptr,
};

static const BeebROM *const MOS500_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_MOS500_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOS500_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOS500_SIDEWAYS_ROM_F,
    nullptr,
};

static const BeebROM *const MOS500_MOS_ROMS[] = {
    &BEEB_ROM_MOS500_MOS_ROM,
    nullptr,
};

static const BeebROM *const MOS510_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_MOS510_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOS510_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOS510_SIDEWAYS_ROM_F,
    nullptr,
};

static const BeebROM *const MOS510_MOS_ROMS[] = {
    &BEEB_ROM_MOS510_MOS_ROM,
    nullptr,
};

static const BeebROM *const MOSI510C_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_MOSI510C_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOSI510C_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOSI510C_SIDEWAYS_ROM_F,
    nullptr,
};

static const BeebROM *const MOSI510C_MOS_ROMS[] = {
    &BEEB_ROM_MOSI510C_MOS_ROM,
    nullptr,
};

static bool ImGuiROMs(BeebConfig::ROM *rom, const BeebROM *const *b_roms) {
    for (size_t i = 0; b_roms[i]; ++i) {
        if (ImGuiROM(rom, b_roms[i])) {
            return true;
        }
    }

    return false;
}

void ConfigsUI::DoROMs(BeebConfig::ROM *rom,
                       bool *edited,
                       uint32_t rom_edit_flags,
                       uint32_t rom_edit_flag,
                       const char *label,
                       const BeebROM *const *roms) {
    if (rom_edit_flags & rom_edit_flag) {
        if (ImGui::BeginMenu(label)) {
            if (ImGuiROMs(rom, roms)) {
                *edited = true;
            }
            ImGui::EndMenu();
        }
    }
}

ROMEditAction ConfigsUI::DoROMEditGui(const char *caption,
                                      BeebConfig::ROM *rom,
                                      bool *writeable,
                                      ROMType *type,
                                      uint32_t rom_edit_flags) {
    ROMEditAction action = ROMEditAction_None;
    bool edited = false;

    ImGuiIDPusher id_pusher(caption);

    // doesn't seem to make any difference.
    //ImGui::AlignFirstTextHeightToWidgets();

    ImGui::TextUnformatted(caption);

    if (rom_edit_flags & (ROMEditFlag_CanMoveUp | ROMEditFlag_CanMoveDown)) {
        ImGui::SameLine();

        {
            ImGuiStyleColourPusher pusher;
            bool can_move_up = !!(rom_edit_flags & ROMEditFlag_CanMoveUp);
            pusher.PushDisabledButtonColours(!can_move_up);
            if (ImGui::Button(ICON_FA_LONG_ARROW_ALT_UP)) {
                if (can_move_up) {
                    action = ROMEditAction_MoveUp;
                }
            }
        }

        ImGui::SameLine();

        {
            ImGuiStyleColourPusher pusher;
            bool can_move_down = !!(rom_edit_flags & ROMEditFlag_CanMoveDown);
            pusher.PushDisabledButtonColours(!can_move_down);
            if (ImGui::Button(ICON_FA_LONG_ARROW_ALT_DOWN)) {
                if (can_move_down) {
                    action = ROMEditAction_MoveDown;
                }
            }
        }
    }

    ImGui::NextColumn();

    if (writeable) {
        ImGui::BeginDisabled(!type || *type != ROMType_16KB);
        if (ImGui::Checkbox("##ram", writeable)) {
            edited = true;
        }
        ImGui::EndDisabled();
    }

    ImGui::NextColumn();

    if (ImGui::Button("...")) {
        ImGui::OpenPopup(ROM_POPUP);
    }

    ImGui::SameLine();

    {
        ImGuiItemWidthPusher pusher(-1);

        if (rom->standard_rom) {
            ImGui::TextUnformatted(rom->standard_rom->name.c_str());
        } else {
            if (ImGuiInputText(&rom->file_name, "##name", rom->file_name)) {
                edited = true;
            }
        }
    }

    if (ImGui::BeginPopup(ROM_POPUP)) {
        if (ImGui::MenuItem("File...")) {
            if (m_ofd.Open(&rom->file_name)) {
                rom->standard_rom = nullptr;
                edited = true;
                m_ofd.AddLastPathToRecentPaths();
            }
        }

        if (ImGuiRecentMenu(&rom->file_name, "Recent file", m_ofd)) {
            rom->standard_rom = nullptr;
            edited = true;
        }

        ImGui::Separator();

        if (type) {
            if (ImGui::BeginMenu("Type", !rom->standard_rom)) {
                for (int i = 0; i < ROMType_Count; ++i) {
                    const ROMTypeMetadata *metadata = GetROMTypeMetadata((ROMType)i);
                    bool selected = *type == i;
                    if (ImGui::MenuItem(metadata->description, nullptr, &selected)) {
                        *type = (ROMType)i;

                        if (*type != ROMType_16KB) {
                            if (writeable) {
                                *writeable = false;
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("(empty)")) {
            rom->standard_rom = nullptr;
            rom->file_name.clear();
            if (type) {
                *type = ROMType_16KB;
            }
            edited = true;
        }

        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_BOSROMs, "B OS ROM", B_OS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_BSidewaysROMs, "B Sideways ROM", B_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_BPlusOSROMs, "B+ OS ROM", BPLUS_OS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_BPlusSidewaysROMs, "B+ Sideways ROM", BPLUS_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_Master128OSROMs, "MOS 3.20 OS ROM", MOS320_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_Master128SidewaysROMs, "MOS 3.20 Sideways ROM", MOS320_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_Master128OSROMs, "MOS 3.50 OS ROM", MOS350_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_Master128SidewaysROMs, "MOS 3.50 Sideways ROM", MOS350_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_ParasiteROMs, "Parasite ROM", PARASITE_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactSidewaysROMs, "MOS 5.00 Sideways ROM", MOS500_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactSidewaysROMs, "MOS 5.10 Sideways ROM", MOS510_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactSidewaysROMs, "PC 128 S Sideways ROM", MOSI510C_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactOSROMs, "MOS 5.00 OS ROM", MOS500_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactOSROMs, "MOS 5.10 OS ROM", MOS510_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactOSROMs, "PC 128 S OS ROM", MOSI510C_MOS_ROMS);

        ImGui::EndPopup();
    }

    ImGui::NextColumn();

    if (edited) {
        if (action == ROMEditAction_None) {
            action = ROMEditAction_Edit;
        }
    }

    return action;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateConfigsUI(BeebWindow *beeb_window) {
    return std::make_unique<ConfigsUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
