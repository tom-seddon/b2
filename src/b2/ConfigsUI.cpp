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
#include <beeb/BBCMicro.h>
#include <shared/strings.h>
#include "discs.h"
#include <shared/path.h>
#include <shared/file_io.h>
#include <beeb/ElectronULA.h>

#include <shared/enum_decl.h>
#include "ConfigsUI_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "ConfigsUI_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char NEW_CONFIG_POPUP[] = "new_config_popup";
static const char ROM_POPUP[] = "rom_popup";
static const char SCSI_POPUP[] = "scsi_popup";
static const char MMFS_POPUP[] = "mmfs_popup";
static const char CONFIG_CONTEXT_POPUP[] = "config_context_popup";

static RecentPaths g_hard_disks_recent_paths("hard_disks");
static RecentPaths g_roms_recent_paths("roms");
static RecentPaths g_mmfs_images_recent_paths("mmfs_images");

const Guid OPEN_ROM_IMAGE_SELECTOR_GUID{0xC4, 0x57, 0x6C, 0xD4, 0xE6, 0x33, 0x4C, 0x63, 0xAD, 0x43, 0xC0, 0x88, 0xF4, 0xC8, 0xFB, 0x2C};
const Guid OPEN_HARD_DISK_IMAGE_SELECTOR_GUID{0xF1, 0x5F, 0xA1, 0xE2, 0x3C, 0xF2, 0x48, 0x40, 0x95, 0x34, 0x90, 0x81, 0x32, 0x09, 0x4E, 0x10};
const Guid OPEN_MMFS_IMAGE_SELECTOR_GUID{0xA3, 0xB2, 0x91, 0xC8, 0xF4, 0x29, 0x49, 0x7D, 0x8E, 0x11, 0x6C, 0x45, 0xAB, 0x38, 0x2F, 0xE9};
const Guid NEW_HARD_DISK_IMAGE_SELECTOR_GUID{0xe7, 0x34, 0xcc, 0xea, 0x15, 0x0f, 0x44, 0x84, 0xa5, 0x42, 0x9d, 0x12, 0x83, 0x1f, 0xf1, 0xc8};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ConfigsUI : public SettingsUI {
  public:
    explicit ConfigsUI(BeebWindow *window, size_t initial_config_index);

    void DoImGui() override;

    bool OnClose() override;

  protected:
  private:
    BeebWindow *m_beeb_window = nullptr;
    bool m_edited = false;
    OpenFileDialog m_rom_ofd;
    OpenFileDialog m_hard_disk_ofd;
    OpenFileDialog m_mmfs_image_ofd;
    SaveFileDialog m_new_hard_disk_sfd;
    size_t m_config_index = INVALID_CONFIG_INDEX;

    void DoROMInfoGui(const char *caption, const BeebConfig::ROM &rom, const bool *writeable);

    // rom_edit_flags is a combination of ROMEditFlag values
    ROMEditAction DoROMEditGui(const char *caption, BeebConfig::ROM *rom, bool *writeable, ROMType *type, OSROMType *os_type, uint32_t rom_edit_flags);
    bool DoParasiteROMEditGui(BeebConfig::ROM *rom, StandardROM standard_rom);

    void DoROMs(BeebConfig::ROM *rom,
                bool *edited,
                uint32_t rom_edit_flags,
                uint32_t rom_edit_flag,
                const char *label,
                const BeebROM *const *roms);

    [[nodiscard]] bool DoEditConfigGui();

    bool CreateNewHardDiskImage(const HardDisk &disk, const std::string &new_dat_path) const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ConfigsUI::ConfigsUI(BeebWindow *beeb_window, size_t initial_config_index)
    : m_beeb_window(beeb_window)
    , m_rom_ofd(OPEN_ROM_IMAGE_SELECTOR_GUID, beeb_window->GetAppHandler())
    , m_hard_disk_ofd(OPEN_HARD_DISK_IMAGE_SELECTOR_GUID, beeb_window->GetAppHandler())
    , m_mmfs_image_ofd(OPEN_MMFS_IMAGE_SELECTOR_GUID, beeb_window->GetAppHandler())
    , m_new_hard_disk_sfd(NEW_HARD_DISK_IMAGE_SELECTOR_GUID, beeb_window->GetAppHandler())
    , m_config_index(initial_config_index) {
    this->SetDefaultSize(ImVec2(650, 450));

    m_rom_ofd.AddAllFilesFilter();

    m_hard_disk_ofd.AddFilter("BBC hard disk file", {".dat"});
    m_hard_disk_ofd.AddAllFilesFilter();

    m_new_hard_disk_sfd.AddFilter("BBC hard disk file", {".dat"});
    m_new_hard_disk_sfd.AddAllFilesFilter();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void Duplicate(size_t *index) {
    const BeebConfig *config = BeebWindows::GetConfigByIndex(*index);
    ++*index; //select newly-inserted item
    BeebWindows::InsertConfig(*config, *index);
}

void ConfigsUI::DoImGui() {
    ImGui::Columns(2, "configs");

    if (ImGui::Button("New...")) {
        ImGui::OpenPopup(NEW_CONFIG_POPUP);
    }

    ImGui::SameLine();

    bool is_usable = false;
    if (m_config_index < BeebWindows::GetNumConfigs()) {
        const BeebConfig *config = BeebWindows::GetConfigByIndex(m_config_index);
        if (config->IsUsable()) {
            is_usable = true;
        }
    }

    {
        ImGuiDisabledPusher pusher(!is_usable);

        if (ImGui::Button("Duplicate")) {
            if (m_config_index < BeebWindows::GetNumConfigs()) {
                Duplicate(&m_config_index);
                m_edited = true;
            }
        }
    }

    ImGui::SameLine();

    if (ImGuiConfirmButton("Delete")) {
        if (m_config_index < BeebWindows::GetNumConfigs()) {
            BeebWindows::RemoveConfigByIndex(m_config_index);
            m_edited = true;
            if (m_config_index >= BeebWindows::GetNumConfigs()) {
                m_config_index = BeebWindows::GetNumConfigs() - 1;
            }
        }
    }

    ImGui::SameLine();

    ImGui::SameLine();

    if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
        m_config_index = BeebWindows::MoveConfigUp(m_config_index);
        m_edited = true;
    }

    ImGui::SameLine();

    if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
        m_config_index = BeebWindows::MoveConfigDown(m_config_index);
        m_edited = true;
    }

    {
        ImGuiItemWidthPusher width_pusher(-1);

        float y = ImGui::GetCursorPosY();
        float h = ImGui::GetWindowHeight();
        float line_height = ImGui::GetTextLineHeightWithSpacing();

        bool popup = false;

        if (ImGui::BeginListBox("##empty", ImVec2(-FLT_MIN, h - y - line_height * .5f))) {
            for (size_t i = 0; i < BeebWindows::GetNumConfigs(); ++i) {
                const BeebConfig *config = BeebWindows::GetConfigByIndex(i);
                if (ImGui::Selectable(config->name.c_str(), i == m_config_index)) {
                    m_config_index = i;
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    m_config_index = i;
                    popup = true;
                }
            }

            ImGui::EndListBox();
        }

        if (popup) {
            ImGui::OpenPopup(CONFIG_CONTEXT_POPUP);
        }

        if (ImGui::BeginPopup(CONFIG_CONTEXT_POPUP)) {
            if (ImGui::MenuItem("Duplicate", nullptr, false, is_usable)) {
                Duplicate(&m_config_index);
                m_edited = true;
            }

            if (ImGui::BeginMenu("Delete")) {
                if (ImGui::MenuItem("Confirm")) {
                    BeebWindows::RemoveConfigByIndex(m_config_index);
                    m_edited = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::NextColumn();

    ImGui::BeginChild("hello");

    bool edited = this->DoEditConfigGui();
    if (edited) {
        BeebWindows::ConfigDidChange((size_t)m_config_index);
        m_edited = true;
    }

    ImGui::EndChild();

    ImGui::Columns(1);

    if (ImGui::BeginPopup(NEW_CONFIG_POPUP)) {
        for (size_t i = 0; i < GetNumDefaultBeebConfigs(); ++i) {
            const BeebConfig *config = GetDefaultBeebConfigByIndex(i);
            if (ImGui::MenuItem(config->name.c_str())) {
                BeebWindows::AddConfig(*config);
                m_edited = true;
            }
        }
        ImGui::EndPopup();
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

bool ConfigsUI::DoEditConfigGui() {
    if (m_config_index >= BeebWindows::GetNumConfigs()) {
        return false;
    }

    BeebConfig *config = BeebWindows::GetMutableConfigByIndex(m_config_index);

    // set to true if *config was edited - as well as
    // dirtying the corresponding loaded config, this will set
    // m_edited.
    bool edited = false;

    const ImGuiStyle &style = ImGui::GetStyle();

    ImGuiIDPusher config_id_pusher(config);

    std::string title = config->name;

    {
        // with a width of -1, the label disappears...
        //ImGuiItemWidthPusher pusher(-1);

        if (ImGuiInputText(&config->name, "Name", config->name)) {
            edited = true;
        }
    }

    if (!config->IsUsable()) {
        ImGui::TextWrapped("This config is not compatible with this build of b2. It can only be renamed, moved, or deleted.");
        ImGui::TextWrapped("Its contents will be preserved.");
        return edited;
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

    case BBCMicroTypeID_Electron:
        rom_edit_sideways_rom_flags = ROMEditFlag_ElectronSidewaysROMs;
        rom_edit_os_rom_flags = ROMEditFlag_ElectronOSROMs;
        break;
    }

    ImGui::Separator();

    ImGui::Text("Model: %s", GetModelName(config->type_id));
    ImGui::Text("Disc interface: %s", config->disc_interface ? config->disc_interface->display_name.c_str() : "(none)");

    ImGui::Separator();

    ImGui::Columns(3, "rom_edit", true);

    ImGui::Text("ROM");
    // GetFrameHeight = size of the arrow button.
    float rom_width = ImGui::GetItemRectSize().x + 2 * ImGui::GetFrameHeight() + 2 * style.ItemSpacing.x;

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
                           &config->os_rom_type,
                           rom_edit_os_rom_flags) != ROMEditAction_None) {
        edited = true;
    }

    ROMEditAction action = ROMEditAction_None;
    uint8_t action_bank = 0;
    ROMEditFlag bank_fixed_flags[16] = {};

    if (IsElectron(config->type_id)) {
        bank_fixed_flags[ElectronULA::KEYBOARD_ROM_BANK_BASE + 0] = ROMEditFlag_NotAvailable;
        bank_fixed_flags[ElectronULA::KEYBOARD_ROM_BANK_BASE + 1] = ROMEditFlag_NotAvailable;
        bank_fixed_flags[ElectronULA::BASIC_ROM_BANK_BASE + 0] = ROMEditFlag_NotAvailable;
    } else {
        uint8_t sideways_roms_end = 16 - GetNumNonOSSidewaysROMs(config->os_rom_type);
        for (uint8_t i = sideways_roms_end; i < 16; ++i) {
            bank_fixed_flags[i] = ROMEditFlag_ContainedInOSROM;
        }

        if (Has4ROMSlots(config->type_id) && !config->rom_board) {
            for (uint8_t i = 0; i < 12; ++i) {
                bank_fixed_flags[i] = ROMEditFlag_NotAccessibleWithoutROMBoard;
            }
        }
    }

    uint8_t bank_up[16];
    {
        uint8_t last_normal_bank = 0xff;

        for (int8_t bank = 15; bank >= 0; --bank) {
            bank_up[bank] = last_normal_bank;
            if (bank_fixed_flags[bank] == 0) {
                last_normal_bank = (uint8_t)bank;
            }
        }
    }

    uint8_t bank_down[16];
    {
        uint8_t last_normal_bank = 0xff;

        for (uint8_t bank = 0; bank < 16; ++bank) {
            bank_down[bank] = last_normal_bank;
            if (bank_fixed_flags[bank] == 0) {
                last_normal_bank = bank;
            }
        }
    }

    for (uint8_t i = 0; i < 16; ++i) {
        uint8_t bank = 15 - i;

        {
            ImGuiIDPusher bank_id_pusher(bank);

            ImGui::Separator();

            BeebConfig::SidewaysROM *rom = &config->roms[bank];

            uint32_t rom_edit_flags = rom_edit_sideways_rom_flags | bank_fixed_flags[bank];

            if (!(rom_edit_flags & (ROMEditFlag_NotAccessibleWithoutROMBoard | ROMEditFlag_NotAvailable))) {
                if (bank_up[bank] < 16) {
                    rom_edit_flags |= ROMEditFlag_CanMoveUp;
                }

                if (bank_down[bank] < 16) {
                    rom_edit_flags |= ROMEditFlag_CanMoveDown;
                }
            }

            char caption[10];
            snprintf(caption, sizeof caption, "%X", bank);

            ROMEditAction a = this->DoROMEditGui(caption,
                                                 rom,
                                                 &rom->writeable,
                                                 &rom->type,
                                                 nullptr,
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
        ASSERT(bank_up[action_bank] < 16);
        std::swap(config->roms[action_bank], config->roms[bank_up[action_bank]]);
        break;

    case ROMEditAction_MoveDown:
        ASSERT(action_bank > 0);
        ASSERT(bank_down[action_bank] < 16);
        std::swap(config->roms[action_bank], config->roms[bank_down[action_bank]]);
        break;
    }

    ImGui::SetColumnOffset(1, rom_width);
    ImGui::SetColumnOffset(2, rom_width + ram_width);

    ImGui::Separator();

    ImGui::Columns(1);

    ImGuiHeader("Additional hardware");

    if (Has1MHzBus(config->type_id)) {
        if (!config->disc_interface || !(config->disc_interface->flags & DiscInterfaceFlag_Uses1MHzBus)) {
            if (ImGui::Checkbox("External memory", &config->ext_mem)) {
                edited = true;
            }
        }
    }

    if (ImGui::Checkbox("BeebLink", &config->beeblink)) {
        edited = true;
    }

    if (CanHaveVideoNuLA(config->type_id)) {
        if (ImGui::Checkbox("Video NuLA", &config->video_nula)) {
            edited = true;
        }
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

            if (config->disc_interface) {
                uint16_t adji_addr = BBCMicro::ADJI_ADDRESSES[config->adji_dip_switches];
                if (config->disc_interface->control_addr == adji_addr ||
                    (adji_addr >= config->disc_interface->fdc_addr &&
                     adji_addr < config->disc_interface->fdc_addr + config->disc_interface->fdc_num_addrs)) {
                    ImGui::TextWrapped("This ADJI setting conflicts with the disc interface. The disc will take priority.");
                }
            }
        }
    }

    if (HasUserPort(config->type_id)) {
        if (ImGui::Checkbox("Mouse", &config->mouse)) {
            edited = true;
        }
    }

    if (Has4ROMSlots(config->type_id)) {
        if (ImGui::Checkbox("ROM board", &config->rom_board)) {
            edited = true;
        }
    }

    if (CanHaveSerial(config->type_id)) {
        if (!HasSerial(config->type_id)) {
            if (ImGui::Checkbox("Serial", &config->serial)) {
                edited = true;
            }
        }
    }

    if (IsElectron(config->type_id)) {
        bool plus_3 = config->disc_interface == &DISC_INTERFACE_PLUS_3;
        if (ImGui::Checkbox("Plus 3", &plus_3)) {
            if (plus_3) {
                config->disc_interface = &DISC_INTERFACE_PLUS_3;
            } else {
                config->disc_interface = nullptr;
            }
        }
    }

#if BBCMICRO_DEBUGGER
    if (ImGui::Checkbox("Extra debugging hardware", &config->extra_debugging_hardware)) {
        edited = true;
    }
#endif

    if (HasTube(config->type_id)) {
        ImGui::Separator();

        ImGuiHeader("Tube");

        if (config->parasite_type != BBCMicroParasiteType_None) {
            if (config->type_id == BBCMicroTypeID_Master) {
                ImGui::TextWrapped("Note: When using MOS 3.20/MOS 3.50, try *CONFIGURE TUBE if 2nd processor doesn't seem to be working");
            } else {
                ImGui::TextWrapped("Note: Ensure a ROM with Tube host code is installed, e.g., Acorn 1770 DFS");
            }
        }

        if (ImGuiRadioButton(&config->parasite_type, BBCMicroParasiteType_None, "No second processor")) {
            edited = true;
        }

        {
            ImGuiIDPusher pusher(BBCMicroParasiteType_External3MHz6502);

            if (ImGuiRadioButton(&config->parasite_type, BBCMicroParasiteType_External3MHz6502, "6502 Second Processor")) {
                edited = true;
            }

            if (this->DoParasiteROMEditGui(&config->parasite_os_external_3MHz_65c02, StandardROM_TUBE110)) {
                edited = true;
            }
        }

        {
            ImGuiIDPusher pusher(BBCMicroParasiteType_MasterTurbo);

            if (ImGuiRadioButton(&config->parasite_type, BBCMicroParasiteType_MasterTurbo, "Master Turbo")) {
                edited = true;
            }

            if (this->DoParasiteROMEditGui(&config->parasite_os_master_turbo, StandardROM_MasterTurboParasite)) {
                edited = true;
            }
        }
    }

    if (CanHaveSCSI(config->type_id)) {
        ImGui::Separator();

        ImGuiHeader("SCSI##header");

        ImGui::Checkbox("SCSI", &config->scsi);

        if (config->scsi) {
            for (size_t hard_disk_index = 0; hard_disk_index < config->hard_disk_dat_paths.size(); ++hard_disk_index) {
                ASSERT(hard_disk_index <= UINT32_MAX);
                ImGuiIDPusher id_pusher((uint32_t)hard_disk_index);

                char name[100];
                snprintf(name, sizeof name, "SCSI HD %zu", hard_disk_index);
                if (ImGuiInputText(&config->hard_disk_dat_paths[hard_disk_index],
                                   name,
                                   config->hard_disk_dat_paths[hard_disk_index])) {
                    edited = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("...")) {
                    ImGui::OpenPopup(SCSI_POPUP);
                }

                if (ImGui::BeginPopup(SCSI_POPUP)) {
                    if (ImGui::MenuItem("File...")) {
                        if (m_hard_disk_ofd.Open(m_beeb_window->GetSDLWindow(), &config->hard_disk_dat_paths[hard_disk_index])) {
                            m_hard_disk_ofd.AddLastPathToRecentPaths(&g_hard_disks_recent_paths);
                            edited = true;
                        }
                    }

                    if (ImGuiRecentMenu(&config->hard_disk_dat_paths[hard_disk_index], "Recent file", &g_hard_disks_recent_paths)) {
                        edited = true;
                    }

                    ImGui::Separator();

                    if (ImGui::BeginMenu("New")) {
                        for (size_t blank_hard_disk_index = 0; blank_hard_disk_index < NUM_BLANK_HARD_DISKS; ++blank_hard_disk_index) {
                            const HardDisk *disk = &BLANK_HARD_DISKS[blank_hard_disk_index];
                            if (ImGui::MenuItem(disk->name.c_str())) {
                                std::string dat_path;
                                if (m_new_hard_disk_sfd.Open(m_beeb_window->GetSDLWindow(), &dat_path)) {
                                    if (this->CreateNewHardDiskImage(*disk, dat_path)) {
                                        config->hard_disk_dat_paths[hard_disk_index] = dat_path;
                                        g_hard_disks_recent_paths.AddPath(dat_path);
                                        edited = true;
                                    }
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }

                    ImGui::EndPopup();
                }
            }
        }
    }

    ImGui::Separator();

    ImGuiHeader("MMFS##header");

    if (ImGui::Checkbox("MMFS", &config->mmfs_enabled)) {
        edited = true;
    }

    if (config->mmfs_enabled) {
        if (ImGuiInputText(&config->mmfs_config.image_path,
                           "Image Path",
                           config->mmfs_config.image_path)) {
            edited = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("...##mmfs")) {
            ImGui::OpenPopup(MMFS_POPUP);
        }

        if (ImGui::BeginPopup(MMFS_POPUP)) {
            if (ImGui::MenuItem("File...")) {
                if (m_mmfs_image_ofd.Open(m_beeb_window->GetSDLWindow(), &config->mmfs_config.image_path)) {
                    m_mmfs_image_ofd.AddLastPathToRecentPaths(&g_mmfs_images_recent_paths);
                    edited = true;
                }
            }

            if (ImGuiRecentMenu(&config->mmfs_config.image_path, "Recent file", &g_mmfs_images_recent_paths)) {
                edited = true;
            }

            ImGui::EndPopup();
        }

        if (ImGui::Checkbox("Enable debug logging", &config->mmfs_config.debug)) {
            edited = true;
        }

        ImGuiStyleColourPusher pusher;
        pusher.PushDefault(ImGuiCol_Text);
        ImGui::TextWrapped("MMB files (MMFS v1) or FAT32 disk images (MMFS v2)");
    }

    return edited;
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

static const BeebROM *const MOS511i_MOS_ROMS[] = {
    &BEEB_ROM_MOS511i_MOS_ROM,
    nullptr,
};

static const BeebROM *const MOS511i_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_MOS511i_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOS511i_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOS511i_SIDEWAYS_ROM_F,
    &BEEB_ROM_MOS511i_ARABIC,
    &BEEB_ROM_MOS511i_INTERNATIONAL,
    nullptr,
};

static const BeebROM *const ELECTRON_SIDEWAYS_ROMS[] = {
    &BEEB_ROM_BASIC2,
    &BEEB_ROM_PLUS_1,
    &BEEB_ROM_PLUS_3_ADFS,
    nullptr,
};

static const BeebROM *const ELECTRON_MOS_ROMS[] = {
    &BEEB_ROM_ELECTRON_MOS,
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
                                      OSROMType *os_type,
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
            if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
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
            if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
                if (can_move_down) {
                    action = ROMEditAction_MoveDown;
                }
            }
        }
    }

    ImGui::SameLine();

    ImGui::NextColumn();

    if (writeable && !(rom_edit_flags & (ROMEditFlag_ContainedInOSROM | ROMEditFlag_NotAccessibleWithoutROMBoard | ROMEditFlag_NotAvailable))) {
        ImGuiDisabledPusher pusher(!type || *type != ROMType_16KB);
        if (ImGui::Checkbox("##ram", writeable)) {
            edited = true;
        }
    }

    ImGui::NextColumn();

    if (!(rom_edit_flags & ROMEditFlag_ContainedInOSROM)) {
        if (ImGui::Button("...")) {
            ImGui::OpenPopup(ROM_POPUP);
        }

        ImGui::SameLine();
    }

    {
        ImGuiItemWidthPusher pusher(-1);

        if (rom_edit_flags & ROMEditFlag_ContainedInOSROM) {
            ImGui::Text("(contained in OS ROM)");
        } else if (rom_edit_flags & ROMEditFlag_NotAccessibleWithoutROMBoard) {
            ImGui::Text("(inaccessible without ROM board)");
        } else if (rom_edit_flags & ROMEditFlag_NotAvailable) {
            ImGui::Text("(this bank is not available for use)");
        } else if (rom->standard_rom) {
            ImGui::TextUnformatted(rom->standard_rom->name.c_str());
        } else {
            if (ImGuiInputText(&rom->file_name, "##name", rom->file_name)) {
                edited = true;
            }
        }
    }

    if (ImGui::BeginPopup(ROM_POPUP)) {
        if (ImGui::MenuItem("File...")) {
            if (m_rom_ofd.Open(m_beeb_window->GetSDLWindow(), &rom->file_name)) {
                m_rom_ofd.AddLastPathToRecentPaths(&g_roms_recent_paths);
                rom->standard_rom = nullptr;
                edited = true;
            }
        }

        if (ImGuiRecentMenu(&rom->file_name, "Recent file", &g_roms_recent_paths)) {
            rom->standard_rom = nullptr;
            edited = true;
        }

        ImGui::Separator();

        if (type) {
            if (ImGui::BeginMenu("Type", !rom->standard_rom)) {
                for (int i = 0; i < ROMType_Count; ++i) {
                    const ROMTypeMetadata *metadata = GetROMTypeMetadata((ROMType)i);
                    if (metadata->num_bytes == 0) {
                        continue;
                    }

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
        } else if (os_type) {
            if (ImGui::BeginMenu("Type", !rom->standard_rom)) {
                for (uint8_t i = 0; i < OSROMType_Count; ++i) {
                    const OSROMTypeMetadata *metadata = GetOSROMTypeMetadata((OSROMType)i);
                    if (ImGui::MenuItem(metadata->description, nullptr, *os_type == i)) {
                        *os_type = (OSROMType)i;
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
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactSidewaysROMs, "MOS 5.11i Sideways ROM", MOS511i_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactOSROMs, "MOS 5.00 OS ROM", MOS500_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactOSROMs, "MOS 5.10 OS ROM", MOS510_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactOSROMs, "PC 128 S OS ROM", MOSI510C_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_MasterCompactOSROMs, "MOS 5.11i OS ROM", MOS511i_MOS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_ElectronSidewaysROMs, "Electron Sideways ROM", ELECTRON_SIDEWAYS_ROMS);
        this->DoROMs(rom, &edited, rom_edit_flags, ROMEditFlag_ElectronOSROMs, "Electron OS", ELECTRON_MOS_ROMS);

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

bool ConfigsUI::DoParasiteROMEditGui(BeebConfig::ROM *rom, StandardROM standard_rom) {
    if (rom->file_name.empty() && !rom->standard_rom) {
        // I messed this up at some point, and now b2 is stuck with this forever
        // :(
        rom->standard_rom = FindBeebROM(standard_rom);
    }

    ROMEditAction a = this->DoROMEditGui("OS", rom, nullptr, nullptr, nullptr, ROMEditFlag_ParasiteROMs);
    if (a != ROMEditAction_None) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool
CopyFile(const std::string &src_path, const std::string &dest_path, bool adfs, Messages *msg) {
    std::vector<uint8_t> data;
    if (!LoadFile(&data, src_path, msg)) {
        return false;
    }

    if (adfs) {
        RandomizeADFSDiskIdentifier(&data);
    }

    if (!SaveFile(data, dest_path, msg)) {
        return false;
    }

    return true;
}

bool ConfigsUI::CreateNewHardDiskImage(const HardDisk &disk, const std::string &new_dat_path) const {
    Messages msg(m_beeb_window->GetMessageList());

    if (!CopyFile(disk.GetDATAssetPath(), new_dat_path, true, &msg)) {
        return false;
    }

    if (!CopyFile(disk.GetDSCAssetPath(), PathWithoutExtension(new_dat_path) + ".dsc", false, &msg)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateConfigsUI(BeebWindow *beeb_window, size_t initial_config_index) {
    return std::make_unique<ConfigsUI>(beeb_window, initial_config_index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
