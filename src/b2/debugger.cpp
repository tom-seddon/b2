#include <shared/system.h>
#include "debugger.h"
#include "commands.h"
#include <SDL.h>
#include "joysticks.h"
#include "SettingsUI.h"
#include <shared/file_io.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static CommandTable2 g_disassembly_table("Disassembly Window", BBCMICRO_DEBUGGER);
static Command2 g_toggle_track_pc_command = Command2(&g_disassembly_table, "toggle_track_pc", "Track PC").WithShortcut(SDLK_t);
static Command2 g_back_command = Command2(&g_disassembly_table, "back", "Back").WithShortcut(SDLK_BACKSPACE);
static Command2 g_up_command = Command2(&g_disassembly_table, "up", "Up").WithShortcut(SDLK_UP);
static Command2 g_down_command = Command2(&g_disassembly_table, "down", "Down").WithShortcut(SDLK_DOWN);
static Command2 g_page_up_command = Command2(&g_disassembly_table, "page_up", "Page Up").WithShortcut(SDLK_PAGEUP);
static Command2 g_page_down_command = Command2(&g_disassembly_table, "page_down", "Page Down").WithShortcut(SDLK_PAGEDOWN);
static Command2 g_step_over_command = Command2(&g_disassembly_table, "step_over", "Step Over").WithShortcut(SDLK_F10);
static Command2 g_step_in_command = Command2(&g_disassembly_table, "step_in", "Step In").WithShortcut(SDLK_F11);

static CommandTable2 g_6502_table("6502 Window", BBCMICRO_DEBUGGER);
static Command2 g_reset_relative_cycles_command = Command2(&g_6502_table, "reset_relative_cycles", "Reset").WithExtraText("Relative cycles");
static Command2 g_toggle_reset_relative_cycles_on_breakpoint_command = Command2(&g_6502_table, "toggle_reset_relative_cycles_on_breakpoint", "Reset on breakpoint").WithExtraText("Relative cycles").WithTick();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

#include "dear_imgui.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <shared/log.h>
#include <dear_imgui_hex_editor.h>
#include <inttypes.h>
#include "misc.h"
#include "load_save.h"
#include <algorithm>

// Ugh, ugh, ugh.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wformat"
#endif
#include <imgui_memory_editor.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <unordered_map>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(DBG, "debugger", "DBG   ", &log_printer_stdout_and_debugger, true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct FnName {
    std::string name;
    std::vector<const char *> names;
};

static std::unordered_map<M6502Fn, FnName> g_name_by_6502_fn;

static const char *GetFnName(M6502Fn fn) {
    if (!fn) {
        return "NULL";
    } else {
        if (g_name_by_6502_fn.empty()) {
            M6502_ForEachFn([](const char *name, M6502Fn fn, void *) {
                g_name_by_6502_fn[fn].names.push_back(name);
            },
                            nullptr);

            for (auto &&it : g_name_by_6502_fn) {
                for (const char *name : it.second.names) {
                    if (!it.second.name.empty()) {
                        it.second.name += "/";
                    }

                    it.second.name += name;
                }
            }
        }

        auto it = g_name_by_6502_fn.find(fn);
        if (it == g_name_by_6502_fn.end()) {
            return "?";
        } else {
            return it->second.name.c_str();
        }
    }
}

static bool ParseAddress(uint16_t *addr_ptr,
                         uint32_t *dso_ptr,
                         const std::shared_ptr<const BBCMicroType> &type,
                         const char *text) {
    uint32_t dso = 0;
    uint16_t addr;

    const char *ep;
    if (!GetUInt16FromString(&addr, text, 0, &ep)) {
        return false;
    }

    if (!isspace(*ep) && *ep != 0) {
        if (*ep != ADDRESS_SUFFIX_SEPARATOR) {
            return false;
        }

        if (!ParseAddressSuffix(&dso, type, ep + 1, nullptr)) {
            return false;
        }
    }

    *addr_ptr = addr;

    // The fixed bits are the ones that are fixed for the calling window, and
    // can't be overridden.
    static const uint32_t dso_fixed_bits_mask = BBCMicroDebugStateOverride_Parasite;
    if ((dso & ~dso_fixed_bits_mask) != 0) {
        *dso_ptr = (dso & ~dso_fixed_bits_mask) | (*dso_ptr & dso_fixed_bits_mask);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class RevealTargetUI;

class DebugUI : public SettingsUI {
  public:
    struct DebugBigPage {
        bool valid = false;
        BBCMicro::ReadOnlyBigPage bp;

        // ...any more???
    };

    using SettingsUI::SettingsUI;

    bool OnClose() override;

    void DoImGui() override final;

    void SetBeebWindow(BeebWindow *beeb_window);

    uint32_t GetDebugStateOverrides() const;
    void SetDebugStateOverrides(uint32_t dso);

  protected:
    BeebWindow *m_beeb_window = nullptr;
    std::shared_ptr<BeebThread> m_beeb_thread;
    uint32_t m_dso = 0;
    std::shared_ptr<const BBCMicroReadOnlyState> m_beeb_state;
    std::shared_ptr<const BBCMicro::DebugState> m_beeb_debug_state;

    virtual void DoImGui2() = 0;

    bool ReadByte(uint8_t *value,
                  uint8_t *address_flags,
                  uint8_t *byte_flags,
                  uint16_t addr,
                  bool mos);

    void DoDebugPageOverrideImGui();

    // If mouse clicked 2 and last item (whatever it was...) hovered, pops up
    // a popup for that byte/address with at least the DoByteDebugGui stuff.
    //void DoAddressPopupGui(M6502Word addr,bool mos);
    void DoBytePopupGui(const DebugBigPage *dbp, M6502Word addr);

    // Address info, checkboxes for breakpoint flags, etc.
    void DoByteDebugGui(const DebugBigPage *dbp, M6502Word addr);

    RevealTargetUI *DoRevealByteGui();
    RevealTargetUI *DoRevealAddressGui();
    RevealTargetUI *DoRevealGui(const char *text, bool address);

    void ApplyOverridesForDebugBigPage(const DebugBigPage *dbp);

    const DebugBigPage *GetDebugBigPageForAddress(M6502Word addr,
                                                  bool mos);

    uint32_t GetDebugStateOverrideMask() const;

    // If required state implied by m_dso is unavailable, display a suitable
    // message.
    bool IsStateUnavailableImGui() const;

  private:
    DebugBigPage m_dbps[2][16]; //[mos][mem big page]
    uint32_t m_popup_id = 0;    //salt for byte popup gui IDs

    bool DoDebugByteFlagsGui(const char *str,
                             uint8_t *flags);
    void DoDebugByteFlagGui(bool *changed,
                            uint8_t *flags,
                            const char *prefix,
                            const char *suffix,
                            uint8_t mask);
    void DoDebugPageOverrideFlagImGui(uint32_t mask,
                                      uint32_t current,
                                      const char *name,
                                      const char *popup_name,
                                      uint32_t override_flag,
                                      uint32_t flag);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class RevealTargetUI {
  public:
    virtual void RevealAddress(M6502Word addr) = 0;
    virtual void RevealByte(const DebugUI::DebugBigPage *dbp, M6502Word addr) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DebugUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::DoImGui() {
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 16; ++j) {
            m_dbps[i][j].valid = false;
        }
    }

    m_beeb_thread->DebugGetState(&m_beeb_state, &m_beeb_debug_state);

    m_popup_id = 0;

    this->DoImGui2();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::SetBeebWindow(BeebWindow *beeb_window) {
    m_beeb_window = beeb_window;
    ASSERT(!m_beeb_thread);
    m_beeb_thread = m_beeb_window->GetBeebThread();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t DebugUI::GetDebugStateOverrides() const {
    return m_dso;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::SetDebugStateOverrides(uint32_t dso) {
    m_dso = dso;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DebugUI::ReadByte(uint8_t *value,
                       uint8_t *addr_flags,
                       uint8_t *byte_flags,
                       uint16_t addr_,
                       bool mos) {
    M6502Word addr = {addr_};

    const DebugBigPage *dbp = this->GetDebugBigPageForAddress(addr, mos);

    if (!dbp->bp.r) {
        return false;
    }

    *value = dbp->bp.r[addr.p.o];

    if (addr_flags) {
        if (dbp->bp.address_debug_flags) {
            *addr_flags = dbp->bp.address_debug_flags[addr.p.o];
        } else {
            *addr_flags = 0;
        }
    }

    if (byte_flags) {
        if (dbp->bp.byte_debug_flags) {
            *byte_flags = dbp->bp.byte_debug_flags[addr.p.o];
        } else {
            *byte_flags = 0;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//uint16_t DebugUI::GetDebugPage(uint16_t addr_) {
//    M6502Word addr={addr_};
//
//    this->PrepareForRead(addr);
//
//    return m_pages[addr.b.h]->flat_page;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//BBCMicro::DebugState::ByteDebugFlags DebugUI::GetDebugFlags(uint16_t addr_) {
//    M6502Word addr={addr_};
//
//    this->PrepareForRead(addr);
//
//    return m_pages[addr.b.h]->debug[addr.b.l];
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::DoDebugPageOverrideImGui() {
    static const char ROM_POPUP[] = "rom_popup";
    static const char REGION_POPUP[] = "region_popup";
    static const char SHADOW_POPUP[] = "shadow_popup";
    static const char ANDY_POPUP[] = "andy_popup";
    static const char HAZEL_POPUP[] = "hazel_popup";
    static const char OS_POPUP[] = "os_popup";
    static const char PARASITE_ROM_POPUP[] = "parasite_rom_popup";

    uint32_t dso_mask = this->GetDebugStateOverrideMask();
    uint32_t dso_current = BBCMicro::DebugGetCurrentStateOverride(m_beeb_state.get());

    //if (dso_mask & BBCMicroDebugStateOverride_Parasite) {
    //    ImGui::CheckboxFlags("Parasite", &m_dso, BBCMicroDebugStateOverride_Parasite);
    //    ImGui::SameLine();
    //}

    if (m_dso & BBCMicroDebugStateOverride_Parasite) {
        this->DoDebugPageOverrideFlagImGui(dso_mask,
                                           dso_current,
                                           "Parasite ROM",
                                           PARASITE_ROM_POPUP,
                                           BBCMicroDebugStateOverride_OverrideParasiteROM,
                                           BBCMicroDebugStateOverride_ParasiteROM);

    } else {
        // ROM.
        if (dso_mask & BBCMicroDebugStateOverride_OverrideROM) {
            if (ImGui::Button("ROM")) {
                ImGui::OpenPopup(ROM_POPUP);
            }

            ImGui::SameLine();

            if (m_dso & BBCMicroDebugStateOverride_OverrideROM) {
                ImGui::Text("%c!", GetROMBankCode(m_dso & BBCMicroDebugStateOverride_ROM));
            } else {
                ImGui::Text("%c", GetROMBankCode(dso_current & BBCMicroDebugStateOverride_ROM));
            }

            if (ImGui::BeginPopup(ROM_POPUP)) {
                if (ImGui::Button("Use current")) {
                    m_dso &= ~BBCMicroDebugStateOverride_OverrideROM;
                    m_dso &= ~BBCMicroDebugStateOverride_ROM;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Text("Force");

                for (uint8_t i = 0; i < 16; ++i) {
                    ImGui::SameLine();

                    char text[2] = {};
                    text[0] = GetROMBankCode(i);

                    if (ImGui::Button(text)) {
                        m_dso |= BBCMicroDebugStateOverride_OverrideROM;
                        m_dso = (m_dso & ~BBCMicroDebugStateOverride_ROM) | i;
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }
        }

        // Mapper region
        if (dso_mask & BBCMicroDebugStateOverride_OverrideMapperRegion) {
            ImGui::SameLine();

            if (ImGui::Button("Region")) {
                ImGui::OpenPopup(REGION_POPUP);
            }

            ImGui::SameLine();

            if (m_dso & BBCMicroDebugStateOverride_OverrideMapperRegion) {
                ImGui::Text("%c!", GetMapperRegionCode((uint8_t)(m_dso >> BBCMicroDebugStateOverride_MapperRegionShift & BBCMicroDebugStateOverride_MapperRegionMask)));
            } else {
                ImGui::Text("%c", GetMapperRegionCode((uint8_t)(dso_current >> BBCMicroDebugStateOverride_MapperRegionShift & BBCMicroDebugStateOverride_MapperRegionMask)));
            }

            if (ImGui::BeginPopup(REGION_POPUP)) {
                if (ImGui::Button("Use current")) {
                    m_dso &= ~BBCMicroDebugStateOverride_OverrideMapperRegion;
                    m_dso &= ~(BBCMicroDebugStateOverride_MapperRegionMask << BBCMicroDebugStateOverride_MapperRegionShift);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Text("Force Region");
                for (uint32_t i = 0; i < 8; ++i) {
                    ImGui::SameLine();

                    char text[2] = {};
                    text[0] = GetMapperRegionCode((uint8_t)i);

                    if (ImGui::Button(text)) {
                        m_dso |= BBCMicroDebugStateOverride_OverrideMapperRegion;
                        m_dso &= ~(BBCMicroDebugStateOverride_MapperRegionMask << BBCMicroDebugStateOverride_MapperRegionShift);
                        m_dso |= i << BBCMicroDebugStateOverride_MapperRegionShift;
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }
        }

        this->DoDebugPageOverrideFlagImGui(dso_mask,
                                           dso_current,
                                           "Shadow",
                                           SHADOW_POPUP,
                                           BBCMicroDebugStateOverride_OverrideShadow,
                                           BBCMicroDebugStateOverride_Shadow);

        this->DoDebugPageOverrideFlagImGui(dso_mask,
                                           dso_current,
                                           "ANDY",
                                           ANDY_POPUP,
                                           BBCMicroDebugStateOverride_OverrideANDY,
                                           BBCMicroDebugStateOverride_ANDY);

        this->DoDebugPageOverrideFlagImGui(dso_mask,
                                           dso_current,
                                           "HAZEL",
                                           HAZEL_POPUP,
                                           BBCMicroDebugStateOverride_OverrideHAZEL,
                                           BBCMicroDebugStateOverride_HAZEL);

        this->DoDebugPageOverrideFlagImGui(dso_mask,
                                           dso_current,
                                           "OS",
                                           OS_POPUP,
                                           BBCMicroDebugStateOverride_OverrideOS,
                                           BBCMicroDebugStateOverride_OS);
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset")) {
        m_dso &= BBCMicroDebugStateOverride_Parasite;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void DebugUI::DoAddressPopupGui(M6502Word addr,bool mos) {
//    static const char CONTEXT_POPUP_NAME[]="debug_ui_addr_context";
//
//    ImGuiIDPusher pusher(addr.w);
//    ImGuiIDPusher pusher2((int)m_popup_id++);
//
//    // Check for opening popup here.
//    //
//    // Don't use ImGui::BeginPopupContextItem(), as that doesn't work properly
//    // for text items.
//    if(ImGui::IsMouseClicked(1)) {
//        if(ImGui::IsItemHovered()) {
//            ImGui::OpenPopup(CONTEXT_POPUP_NAME);
//        }
//    }
//
//    if(ImGui::BeginPopup(CONTEXT_POPUP_NAME)) {
//        const DebugBigPage *dbp=this->GetDebugBigPageForAddress(addr,mos);
//        this->DoByteDebugGui(dbp,addr);
//        ImGui::EndPopup();
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::DoBytePopupGui(const DebugBigPage *dbp, M6502Word addr) {
    static const char CONTEXT_POPUP_NAME[] = "debug_ui_byte_context";

    ImGuiIDPusher pusher(addr.w);
    ImGuiIDPusher pusher2((int)m_popup_id++);

    // Check for opening popup here.
    //
    // Don't use ImGui::BeginPopupContextItem(), as that doesn't work properly
    // for text items.
    if (ImGui::IsMouseClicked(1)) {
        if (ImGui::IsItemHovered()) {
            ImGui::OpenPopup(CONTEXT_POPUP_NAME);
        }
    }

    if (ImGui::BeginPopup(CONTEXT_POPUP_NAME)) {
        this->DoByteDebugGui(dbp, addr);
        ImGui::EndPopup();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::DoByteDebugGui(const DebugBigPage *dbp, M6502Word addr) {
    const char *cpu = dbp->bp.metadata->is_parasite ? "Parasite" : "Host";

    ImGuiStyleColourPusher pusher;
    pusher.PushDefault(ImGuiCol_Text);

    ImGui::Text("Address: $%04x (%s)", addr.w, cpu);

    if (!dbp) {
        ImGui::Separator();
        ImGui::Text("Byte: *unknown*");
    } else {
        if (dbp->bp.address_debug_flags) {
            char addr_str[50];
            snprintf(addr_str, sizeof addr_str, "$%04x (%s)", addr.w, cpu);

            uint8_t addr_flags = dbp->bp.address_debug_flags[addr.p.o];
            if (this->DoDebugByteFlagsGui(addr_str, &addr_flags)) {
                uint32_t dso = dbp->bp.metadata->is_parasite ? BBCMicroDebugStateOverride_Parasite : 0;
                m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetAddressDebugFlags>(addr, dso, addr_flags));
            }

            if (RevealTargetUI *reveal_target_ui = this->DoRevealAddressGui()) {
                reveal_target_ui->RevealAddress(addr);
            }
        }

        ImGui::Separator();

        char byte_str[10];
        snprintf(byte_str, sizeof byte_str, "$%04x%c%s",
                 addr.w, ADDRESS_SUFFIX_SEPARATOR, dbp->bp.metadata->minimal_codes);

        ImGui::Text("Byte: %s (%s)",
                    byte_str,
                    dbp->bp.metadata->description.c_str());

        if (dbp->bp.byte_debug_flags) {
            uint8_t byte_flags = dbp->bp.byte_debug_flags[addr.p.o];
            if (this->DoDebugByteFlagsGui(byte_str, &byte_flags)) {
                m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetByteDebugFlags>(dbp->bp.metadata->debug_flags_index,
                                                                                         (uint16_t)addr.p.o,
                                                                                         byte_flags));
            }
        }

        if (RevealTargetUI *reveal_target_ui = this->DoRevealByteGui()) {
            reveal_target_ui->RevealByte(dbp, addr);
        }

        ImGui::Separator();

        if (dbp->bp.r) {
            uint8_t value = dbp->bp.r[addr.p.o];
            ImGui::Text("Value: %3d %3uu ($%02x) (%%%s)", (int8_t)value, value, value, BINARY_BYTE_STRINGS[value]);
        } else {
            ImGui::TextUnformatted("Value: --");
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RevealTargetUI *DebugUI::DoRevealAddressGui() {
    RevealTargetUI *ui = this->DoRevealGui("Reveal address...", true);
    return ui;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RevealTargetUI *DebugUI::DoRevealByteGui() {
    RevealTargetUI *ui = this->DoRevealGui("Reveal byte...", false);
    return ui;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RevealTargetUI *DebugUI::DoRevealGui(const char *text, bool address) {
    static const char POPUP_NAME[] = "reveal_target_selector_popup";
    RevealTargetUI *result = nullptr;

    ImGuiIDPusher pusher(text);

    if (ImGui::Button(text)) {
        ImGui::OpenPopup(POPUP_NAME);
    }

    if (ImGui::BeginPopup(POPUP_NAME)) {
        struct UI {
            DebugUI *debug;
            RevealTargetUI *target;
        };
        UI uis[BeebWindowPopupType_MaxValue];
        size_t num_uis = 0;

        uint32_t this_dso = this->GetDebugStateOverrides();

        for (int i = 0; i < BeebWindowPopupType_MaxValue; ++i) {
            UI ui;
            ui.debug = dynamic_cast<DebugUI *>(m_beeb_window->GetPopupByType((BeebWindowPopupType)i));
            if (ui.debug) {
                ui.target = dynamic_cast<RevealTargetUI *>(ui.debug);
                if (ui.target) {
                    if (address) {
                        // all good - any window can show an address.
                    } else {
                        uint32_t other_dso = ui.debug->GetDebugStateOverrides();
                        if ((other_dso & BBCMicroDebugStateOverride_Parasite) != (this_dso & BBCMicroDebugStateOverride_Parasite)) {
                            // parasite flag doesn't match! the other window
                            // can't show this specific byte.
                            ui.target = nullptr;
                        }
                    }

                    if (ui.target) {
                        uis[num_uis++] = ui;
                    }
                }
            }
        }

        if (num_uis == 0) {
            ImGui::Text("No suitable windows active");
        } else {
            for (size_t i = 0; i < num_uis; ++i) {
                if (ImGui::Selectable(uis[i].debug->GetName().c_str())) {
                    result = uis[i].target;
                }
            }
        }

        ImGui::EndPopup();
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::ApplyOverridesForDebugBigPage(const DebugBigPage *dbp) {
    m_dso &= dbp->bp.metadata->dso_mask;
    m_dso |= dbp->bp.metadata->dso_value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DebugUI::DoDebugByteFlagsGui(const char *str,
                                  uint8_t *flags) {
    bool changed = false;

    this->DoDebugByteFlagGui(&changed, flags, "Break on read", str, BBCMicroByteDebugFlag_BreakRead);
    this->DoDebugByteFlagGui(&changed, flags, "Break on write", str, BBCMicroByteDebugFlag_BreakWrite);
    this->DoDebugByteFlagGui(&changed, flags, "Break on execute", str, BBCMicroByteDebugFlag_BreakExecute);

    return changed;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DebugUI::DoDebugByteFlagGui(bool *changed,
                                 uint8_t *flags,
                                 const char *prefix,
                                 const char *suffix,
                                 uint8_t mask) {
    bool flag = !!(*flags & mask);

    char label[1000]; //1000 = large enough
    snprintf(label, sizeof label, "%s %s", prefix, suffix);

    if (ImGui::Checkbox(label, &flag)) {
        if (flag) {
            *flags |= mask;
        } else {
            *flags &= ~mask;
        }

        *changed = true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const DebugUI::DebugBigPage *DebugUI::GetDebugBigPageForAddress(M6502Word addr,
                                                                bool mos) {
    ASSERT((int)mos == 0 || (int)mos == 1);

    DebugUI::DebugBigPage *dbp = &m_dbps[mos][addr.p.p];

    if (!dbp->valid) {
        BBCMicro::DebugGetBigPageForAddress(&dbp->bp, m_beeb_state.get(), m_beeb_debug_state.get(), addr, mos, m_dso);

        //if (dbp->bp.r) {
        //    memcpy(dbp->ram_buffer, dbp->bp.r, BIG_PAGE_SIZE_BYTES);
        //    dbp->bp.r = dbp->ram_buffer;
        //}

        //if (dbp->bp.byte_debug_flags) {
        //    ASSERT(dbp->bp.address_debug_flags);

        //    memcpy(dbp->byte_flags_buffer, dbp->bp.byte_debug_flags, BIG_PAGE_SIZE_BYTES);
        //    dbp->bp.byte_debug_flags = dbp->byte_flags_buffer;

        //    memcpy(dbp->addr_flags_buffer, dbp->bp.address_debug_flags, BIG_PAGE_SIZE_BYTES);
        //    dbp->bp.address_debug_flags = dbp->addr_flags_buffer;
        //} else {
        //    ASSERT(!dbp->bp.address_debug_flags);
        //}

        dbp->valid = true;
    }

    return dbp;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t DebugUI::GetDebugStateOverrideMask() const {
    uint32_t dso_mask = m_beeb_state->type->dso_mask;

    if (m_beeb_state->parasite_type != BBCMicroParasiteType_None) {
        dso_mask |= (BBCMicroDebugStateOverride_Parasite |
                     BBCMicroDebugStateOverride_ParasiteROM |
                     BBCMicroDebugStateOverride_OverrideParasiteROM);
    }

    return dso_mask;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DebugUI::IsStateUnavailableImGui() const {
    if (m_dso & BBCMicroDebugStateOverride_Parasite) {
        if (!(this->GetDebugStateOverrideMask() & BBCMicroDebugStateOverride_Parasite)) {
            ImGui::TextUnformatted("State not available");
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// These want to be the same width, so the UI doesn't jiggle around if the
// values are changing rapidly.
static const char DSO_FLAG_ON[] = "on ";
static const char DSO_FLAG_OFF[] = "off";

void DebugUI::DoDebugPageOverrideFlagImGui(uint32_t mask,
                                           uint32_t current,
                                           const char *name,
                                           const char *popup_name,
                                           uint32_t override_mask,
                                           uint32_t flag_mask) {
    if (!(mask & override_mask)) {
        return;
    }

    ImGui::SameLine();

    if (ImGui::Button(name)) {
        ImGui::OpenPopup(popup_name);
    }

    ImGui::SameLine();

    if (m_dso & override_mask) {
        ImGui::Text("%s!", m_dso & flag_mask ? DSO_FLAG_ON : DSO_FLAG_OFF);
    } else {
        ImGui::Text("%s", current & flag_mask ? DSO_FLAG_ON : DSO_FLAG_OFF);
    }

    if (ImGui::BeginPopup(popup_name)) {
        if (ImGui::Button("Use current")) {
            m_dso &= ~override_mask;
            m_dso &= ~flag_mask;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Force on")) {
            m_dso |= override_mask;
            m_dso |= flag_mask;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Force off")) {
            m_dso |= override_mask;
            m_dso &= ~flag_mask;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class DerivedType>
static std::unique_ptr<DerivedType> CreateDebugUI(BeebWindow *beeb_window, ImVec2 default_size = {}) {
    std::unique_ptr<DerivedType> ptr = std::make_unique<DerivedType>();

    ptr->SetBeebWindow(beeb_window);

    // Constructor (if any) may already have set the default size.
    if (default_size.x != 0 && default_size.y != 0) {
        ptr->SetDefaultSize(default_size);
    }

    return ptr;
}

template <class DerivedType>
static std::unique_ptr<DerivedType> CreateParasiteDebugUI(BeebWindow *beeb_window, ImVec2 default_size = {}) {
    std::unique_ptr<DerivedType> ptr = CreateDebugUI<DerivedType>(beeb_window, default_size);

    uint32_t dso = ptr->GetDebugStateOverrides();
    dso |= BBCMicroDebugStateOverride_Parasite;
    ptr->SetDebugStateOverrides(dso);

    return ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SystemDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        char cycles_str[MAX_UINT64_THOUSANDS_SIZE];

        GetThousandsString(cycles_str, m_beeb_state->cycle_count.n >> RSHIFT_CYCLE_COUNT_TO_2MHZ);
        ImGui::Text("2 MHz host cycles = %s", cycles_str);

        switch (m_beeb_state->parasite_type) {
        default:
            break;

        case BBCMicroParasiteType_External3MHz6502:
            GetThousandsString(cycles_str, Get3MHzCycleCount(m_beeb_state->cycle_count));
            ImGui::Text("3 MHz parasite cycles = %s", cycles_str);
            break;

        case BBCMicroParasiteType_MasterTurbo:
            GetThousandsString(cycles_str, m_beeb_state->cycle_count.n >> RSHIFT_CYCLE_COUNT_TO_4MHZ);
            ImGui::Text("4 MHz parasite cycles = %s", cycles_str);
            break;
        }

        ImGui::Text("Run time = %s", GetCycleCountString(m_beeb_state->cycle_count).c_str());

        if (m_beeb_debug_state && m_beeb_debug_state->is_halted) {
            if (m_beeb_debug_state->halt_reason[0] == 0) {
                ImGui::TextUnformatted("State = halted");
            } else {
                ImGui::Text("State = halted: %s", m_beeb_debug_state->halt_reason);
            }
        } else {
            ImGui::TextUnformatted("State = running");
        }

        ImGuiHeader("Update Flags");
        {
            uint32_t flags = m_beeb_thread->GetUpdateFlags();
            ROMType type = (ROMType)(flags >> BBCMicroUpdateFlag_ROMTypeShift & BBCMicroUpdateFlag_ROMTypeMask);
            flags &= ~(BBCMicroUpdateFlag_ROMTypeMask << BBCMicroUpdateFlag_ROMTypeShift);
            for (uint32_t mask = 1; mask != 0; mask <<= 1) {
                if (flags & mask) {
                    const char *flag = GetBBCMicroUpdateFlagEnumName(mask);
                    ImGui::BulletText("%s", flag);
                }
            }
            ImGui::BulletText("ROM Type: %s", GetROMTypeEnumName(type));
        }

        ImGuiHeader("Debugger State");
        ImGui::Text("Breakpoint change counter = %" PRIu64, m_beeb_debug_state->breakpoints_changed_counter);
        ImGui::Text("Num bytes+addresses with breakpoints = %" PRIu64, m_beeb_debug_state->num_breakpoint_bytes);
    }

  private:
};

std::unique_ptr<SettingsUI> CreateSystemDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<SystemDebugWindow>(beeb_window, ImVec2(350, 180));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class M6502DebugWindow : public DebugUI {
  public:
    M6502DebugWindow()
        : DebugUI(&g_6502_table) {
        this->SetDefaultSize(ImVec2(427, 258));
    }

  protected:
    void DoImGui2() override {
        if (this->IsStateUnavailableImGui()) {
            return;
        }

        //ImGui::Text("m_dso=0x%" PRIx32, m_dso);
        if (this->cst->WasActioned(g_reset_relative_cycles_command)) {
            m_beeb_thread->Send(std::make_shared<BeebThread::CallbackMessage>([dso = m_dso](BBCMicro *m) -> void {
                m->DebugResetRelativeCycleBase(dso);
            }));
        }

        uint64_t absolute_cycles = m_beeb_state->DebugGetCPUCycless(m_dso, m_beeb_state->cycle_count);
        uint64_t relative_cycles;
        bool reset_on_breakpoint;
        {
            BBCMicro::DebugState::RelativeCycleCountBase BBCMicro::DebugState::*base_mptr = BBCMicro::DebugGetRelativeCycleCountBaseMPtr(*m_beeb_state, m_dso);
            ASSERT(base_mptr);
            const BBCMicro::DebugState::RelativeCycleCountBase *base = &((*m_beeb_debug_state).*base_mptr);
            reset_on_breakpoint = base->reset_on_breakpoint;

            CycleCount relative_base = base->recent;
            if (relative_base.n == m_beeb_state->cycle_count.n - 1) {
                // Recent is not interesting - the CPU literally just hit it.
                relative_base = base->prev;
            }

            relative_cycles = absolute_cycles - m_beeb_state->DebugGetCPUCycless(m_dso, relative_base);
        }

        this->cst->SetTicked(g_toggle_reset_relative_cycles_on_breakpoint_command, reset_on_breakpoint);
        if (this->cst->WasActioned(g_toggle_reset_relative_cycles_on_breakpoint_command)) {
            m_beeb_thread->Send(std::make_shared<BeebThread::CallbackMessage>([dso = m_dso](BBCMicro *m) -> void {
                m->DebugToggleResetRelativeCycleBaseOnBreakpoint(dso);
            }));
        }

        const M6502 *cpu = m_beeb_state->DebugGetM6502(m_dso);

        this->Reg("A", cpu->a);
        this->Reg("X", cpu->x);
        this->Reg("Y", cpu->y);
        ImGui::Text("PC = $%04x", cpu->opcode_pc.w);
        ImGui::Text("S = $01%02X", cpu->s.b.l);
        uint8_t opcode = M6502_GetOpcode(cpu);
        const char *mnemonic = cpu->config->disassembly_info[opcode].mnemonic;
        const char *mode_name = M6502AddrMode_GetName(cpu->config->disassembly_info[opcode].mode);

        M6502P p = M6502_GetP(cpu);
        char pstr[9];
        ImGui::Text("P = $%02x %s", p.value, M6502P_GetString(pstr, p));

        ImGui::Text("Opcode = $%02X %03d - %s %s", opcode, opcode, mnemonic, mode_name);
        ImGui::Text("tfn = %s", GetFnName(cpu->tfn));
        ImGui::Text("ifn = %s", GetFnName(cpu->ifn));
        ImGui::Text("Address = $%04x; Data = $%02x %03d %s %s", cpu->abus.w, cpu->dbus, cpu->dbus, BINARY_BYTE_STRINGS[cpu->dbus], ASCII_BYTE_STRINGS[cpu->dbus]);
        ImGui::Text("Access = %s", M6502ReadType_GetName(cpu->read));

        char cycles_str[MAX_UINT64_THOUSANDS_SIZE];

        GetThousandsString(cycles_str, absolute_cycles);
        ImGui::Text("Absolute cycles = %s", cycles_str);

        GetThousandsString(cycles_str, relative_cycles);
        ImGui::Text("Relative cycles = %s", cycles_str);

        ImGui::SameLine();
        this->cst->DoButton(g_reset_relative_cycles_command);
        ImGui::SameLine();
        this->cst->DoToggleCheckbox(g_toggle_reset_relative_cycles_on_breakpoint_command);
    }

  private:
    void Reg(const char *name, uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s", name, value, value, BINARY_BYTE_STRINGS[value]);
    }
};

std::unique_ptr<SettingsUI> CreateHost6502DebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<M6502DebugWindow>(beeb_window);
}

std::unique_ptr<SettingsUI> CreateParasite6502DebugWindow(BeebWindow *beeb_window) {
    return CreateParasiteDebugUI<M6502DebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(HEXEDIT, "HEXEDIT", &log_printer_stdout_and_debugger, true);

class MemoryDebugWindow : public DebugUI,
                          public RevealTargetUI {
  public:
    MemoryDebugWindow()
        : m_handler(this)
        , m_hex_editor(&m_handler) {
        this->SetDefaultSize(ImVec2(600, 450));
    }

    void AllowMOSToggle() {
        m_show_mos_toggle = true;
    }

    virtual void RevealAddress(M6502Word addr) override {
        m_hex_editor.SetOffset(addr.w);
    }

    virtual void RevealByte(const DebugUI::DebugBigPage *dbp, M6502Word addr) override {
        this->ApplyOverridesForDebugBigPage(dbp);
        this->RevealAddress(addr);
    }

  protected:
    void DoImGui2() override {
        this->DoDebugPageOverrideImGui();

        if (m_show_mos_toggle) {
            if (HasIndependentMOSView(m_beeb_window->GetBeebThread()->GetBBCMicroTypeID())) {
                ImGui::SameLine();
                ImGui::Checkbox("MOS's view", &this->m_handler.mos);
            }
        }

        m_hex_editor.DoImGui();
    }

  private:
    class Handler : public HexEditorHandler {
      public:
        bool mos = false;

        explicit Handler(MemoryDebugWindow *window)
            : m_window(window) {
        }

        void ReadByte(HexEditorByte *byte, size_t offset) override {
            M6502Word addr = {(uint16_t)offset};

            const DebugBigPage *dbp = m_window->GetDebugBigPageForAddress(addr, false);

            if (!dbp || !dbp->bp.r) {
                byte->got_value = false;
            } else {
                byte->got_value = true;
                byte->value = dbp->bp.r[addr.p.o];
                byte->can_write = dbp->bp.writeable;
            }
        }

        void WriteByte(size_t offset, uint8_t value) override {
            std::vector<uint8_t> data;
            data.resize(1);
            data[0] = value;

            m_window->m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetBytesMessage>((uint16_t)offset,
                                                                                             m_window->m_dso,
                                                                                             this->mos,
                                                                                             std::move(data)));
        }

        size_t GetSize() override {
            return 65536;
        }

        void DebugPrint(const char *fmt, ...) override {
            va_list v;

            va_start(v, fmt);
            LOGV(HEXEDIT, fmt, v);
            va_end(v);
        }

        void DoContextPopupExtraGui(bool hex, size_t offset) override {
            M6502Word addr = {(uint16_t)offset};
            const DebugBigPage *dbp = m_window->GetDebugBigPageForAddress(addr, false);
            m_window->DoByteDebugGui(dbp, addr);

            ImGui::Separator();
            this->HexEditorHandler::DoContextPopupExtraGui(hex, offset);
        }

        void DoOptionsPopupExtraGui() override {
            ImGui::Separator();

            ImGui::InputText("Save Begin", m_save_begin_buffer, sizeof m_save_begin_buffer);

            const char *label = m_specify_end ? "Save End" : "Save Size";
            ImGui::InputText(label, m_save_end_buffer, sizeof m_save_end_buffer);

            if (ImGui::Checkbox("Specify End", &m_specify_end)) {
                uint16_t begin;
                uint32_t end_or_size;
                if (this->GetSaveParameters(&begin, &end_or_size)) {
                    if (m_specify_end) {
                        // end_or_size was size
                        snprintf(m_save_end_buffer, sizeof m_save_end_buffer, "0x%x", begin + end_or_size);
                    } else {
                        // end_or_size was end
                        snprintf(m_save_end_buffer, sizeof m_save_end_buffer, "0x%x", end_or_size - begin);
                    }
                }
            }

            uint16_t begin;
            uint32_t end_or_size;
            bool save_enabled = this->GetSaveParameters(&begin, &end_or_size);
            if (m_specify_end && end_or_size < begin) {
                save_enabled = false;
            }

            {
                ImGuiStyleColourPusher pusher;
                pusher.PushDisabledButtonColours(!save_enabled);
                if (ImGui::Button("Save memory...") && save_enabled) {
                    SaveFileDialog fd("save_memory");

                    fd.AddAllFilesFilter();

                    std::string path;
                    if (fd.Open(&path)) {
                        uint32_t end;
                        if (m_specify_end) {
                            end = end_or_size;
                        } else {
                            end = begin + end_or_size;
                        }

                        // (this clamp means that addr has to be the full 32
                        // bits. But this value is the terminating value, so the
                        // cast to uint16_t inside the loop is quite safe.)
                        end = std::min(end, 0x10000u);

                        std::vector<uint8_t> buffer;
                        for (uint32_t addr = begin; addr != end; ++addr) {
                            uint8_t value;
                            if (!m_window->ReadByte(&value, nullptr, nullptr, (uint16_t)addr, this->mos)) {
                                value = 0;
                            }
                            buffer.push_back(value);
                        }

                        Messages msgs(m_window->m_beeb_window->GetMessageList());
                        SaveFile(buffer, path, &msgs);
                    }
                }
            }
        }

        void GetAddressText(char *text,
                            size_t text_size,
                            size_t offset,
                            bool upper_case) override {
            const DebugBigPage *dbp = m_window->GetDebugBigPageForAddress({(uint16_t)offset}, this->mos);

            snprintf(text,
                     text_size,
                     upper_case ? "$%04X%c%s" : "$%04x%c%s",
                     (unsigned)offset,
                     ADDRESS_SUFFIX_SEPARATOR,
                     dbp->bp.metadata->aligned_codes);
        }

        bool ParseAddressText(size_t *offset, const char *text) override {
            uint16_t addr;
            if (!ParseAddress(&addr,
                              &m_window->m_dso,
                              m_window->m_beeb_state->type,
                              text)) {
                return false;
            }

            *offset = addr;
            return true;
        }

        int GetNumAddressChars() override {
            // 012345678
            // xx`$xxxxx
            return 8;
        }

      protected:
      private:
        char m_save_begin_buffer[50] = {};
        char m_save_end_buffer[50] = {};
        bool m_specify_end = false;
        MemoryDebugWindow *const m_window;

        bool GetSaveParameters(uint16_t *begin, uint32_t *end_or_size) const {
            if (!GetUInt16FromString(begin, m_save_begin_buffer)) {
                return false;
            }

            if (!GetUInt32FromString(end_or_size, m_save_end_buffer)) {
                return false;
            }

            return true;
        }
    };

    bool m_show_mos_toggle = false;
    Handler m_handler;
    HexEditor m_hex_editor;
};

std::unique_ptr<SettingsUI> CreateHostMemoryDebugWindow(BeebWindow *beeb_window) {
    std::unique_ptr<MemoryDebugWindow> window = CreateDebugUI<MemoryDebugWindow>(beeb_window);
    window->AllowMOSToggle();
    return window;
}

std::unique_ptr<SettingsUI> CreateParasiteMemoryDebugWindow(BeebWindow *beeb_window) {
    return CreateParasiteDebugUI<MemoryDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ExtMemoryDebugWindow : public DebugUI {
  public:
    ExtMemoryDebugWindow() {
        this->SetDefaultSize(ImVec2(600, 450));

        m_memory_editor.ReadFn = &MemoryEditorRead;
        m_memory_editor.WriteFn = &MemoryEditorWrite;
    }

  protected:
    void DoImGui2() override {
        bool enabled;
        uint8_t l, h;

        if (const ExtMem *s = m_beeb_state->DebugGetExtMem()) {
            enabled = true;
            l = s->GetAddressL();
            h = s->GetAddressH();
        } else {
            enabled = false;
            h = l = 0; //inhibit spurious unused variable warning.
        }

        if (enabled) {
            this->Reg("L", l);
            this->Reg("H", h);

            m_memory_editor.DrawContents((uint8_t *)this, 16777216, 0);
        } else {
            ImGui::Text("External memory disabled");
        }
    }

  private:
    MemoryEditor m_memory_editor;

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static uint8_t MemoryEditorRead(const ImU8 *data, size_t off) {
        auto self = (ExtMemoryDebugWindow *)data;

        ASSERT((uint32_t)off == off);

        const ExtMem *s = self->m_beeb_state->DebugGetExtMem();
        return ExtMem::ReadMemory(s, (uint32_t)off);
    }

    // There's no context parameter :( - so this hijacks the data
    // parameter for that purpose.
    static void MemoryEditorWrite(ImU8 *data, size_t off, uint8_t d) {
        auto self = (ExtMemoryDebugWindow *)data;

        self->m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetExtByteMessage>((uint32_t)off, d));
    }

    void Reg(const char *name, uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s", name, value, value, BINARY_BYTE_STRINGS[value]);
    }
};

std::unique_ptr<SettingsUI> CreateExtMemoryDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<ExtMemoryDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DisassemblyDebugWindow : public DebugUI,
                               public RevealTargetUI {
  public:
    DisassemblyDebugWindow()
        : DebugUI(&g_disassembly_table) {
        this->SetDefaultSize(ImVec2(450, 500));
    }

    uint32_t GetExtraImGuiWindowFlags() const override {
        // The bottom line of the disassembly should just be clipped
        // if it runs off the bottom... only drawing whole lines just
        // looks weird. But when that happens, dear imgui
        // automatically adds a scroll bar. And that's even weirder.
        return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }

    void RevealAddress(M6502Word addr) override {
        m_track_pc = false;
        m_addr = addr.w;
    }

    void RevealByte(const DebugUI::DebugBigPage *dbp, M6502Word addr) override {
        this->ApplyOverridesForDebugBigPage(dbp);
        this->RevealAddress(addr);
    }

    void SetTrackPC(bool track_pc) {
        m_track_pc = track_pc;
    }

  protected:
    void DoImGui2() override {
        if (this->IsStateUnavailableImGui()) {
            return;
        }

        const M6502 *cpu = m_beeb_state->DebugGetM6502(m_dso);

        //const M6502Config *config;
        //uint16_t pc;
        //uint8_t a, x, y;
        //M6502Word sp;
        //M6502P p;
        uint8_t pc_is_mos[16];
        BBCMicro::DebugGetMemBigPageIsMOSTable(pc_is_mos, m_beeb_state.get(), m_dso);

        //const BBCMicroType *type;
        //bool halted;
        //{
        //    std::unique_lock<Mutex> lock;
        //    const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

        //    const M6502 *s = m->DebugGetM6502(m_dso);

        //    type = m->GetType();
        //    halted = m->DebugIsHalted();

        //    config = s->config;
        //    pc = s->opcode_pc.w;
        //    a = s->a;
        //    x = s->x;
        //    y = s->y;
        //    p = M6502_GetP(s);
        //    sp = s->s;
        //    m->DebugGetMemBigPageIsMOSTable(pc_is_mos, m_dso);
        //}

        this->cst->SetTicked(g_toggle_track_pc_command, m_track_pc);
        if (this->cst->WasActioned(g_toggle_track_pc_command)) {
            m_track_pc = !m_track_pc;

            if (m_track_pc) {
                // force snap to current PC if it's halted.
                m_old_pc = -1;
            }
        }

        this->cst->SetEnabled(g_back_command, !m_history.empty());
        if (this->cst->WasActioned(g_back_command)) {
            ASSERT(!m_history.empty());
            m_track_pc = false;
            m_addr = m_history.back();
            m_history.pop_back();
        }

        this->cst->SetEnabled(g_up_command, !m_track_pc);
        if (this->cst->WasActioned(g_up_command)) {
            this->Up(cpu->config, 1);
        }

        this->cst->SetEnabled(g_down_command, !m_track_pc);
        if (this->cst->WasActioned(g_down_command)) {
            this->Down(cpu->config, 1);
        }

        this->cst->SetEnabled(g_page_up_command, !m_track_pc);
        if (this->cst->WasActioned(g_page_up_command)) {
            this->Up(cpu->config, m_num_lines - 2);
        }

        this->cst->SetEnabled(g_page_down_command, !m_track_pc);
        if (this->cst->WasActioned(g_page_down_command)) {
            this->Down(cpu->config, m_num_lines - 2);
        }

        this->cst->SetEnabled(g_step_over_command, m_beeb_window->DebugIsRunEnabled());
        if (this->cst->WasActioned(g_step_over_command)) {
            m_beeb_window->DebugStepOver(m_dso);
        }

        this->cst->SetEnabled(g_step_in_command, m_beeb_window->DebugIsRunEnabled());
        if (this->cst->WasActioned(g_step_in_command)) {
            m_beeb_window->DebugStepIn(m_dso);
        }

        float maxY = ImGui::GetCurrentWindow()->Size.y; //-ImGui::GetTextLineHeight()-GImGui->Style.WindowPadding.y*2.f;

        this->DoDebugPageOverrideImGui();

        this->ByteRegUI("A", cpu->a);
        ImGui::SameLine();
        this->ByteRegUI("X", cpu->x);
        ImGui::SameLine();
        this->ByteRegUI("Y", cpu->y);
        ImGui::SameLine();
        const M6502P p = M6502_GetP(cpu);
        {
            char pstr[9];
            M6502P_GetString(pstr, p);
            ImGui::TextUnformatted("P=");
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted(pstr);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                this->ByteRegPopupContentsUI(p.value);
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
        }
        this->ByteRegUI("S", cpu->s.b.l);
        ImGui::SameLine();
        this->WordRegUI("PC", cpu->opcode_pc);
        ImGui::SameLine();
        this->cst->DoToggleCheckbox(g_toggle_track_pc_command);

        this->cst->DoButton(g_back_command);

        if (ImGui::InputText("Address",
                             m_address_text, sizeof m_address_text,
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            uint16_t addr;
            if (ParseAddress(&addr, &m_dso, m_beeb_state->type, m_address_text)) {
                this->GoTo(addr);
            }
        }

        this->cst->DoButton(g_step_over_command);
        ImGui::SameLine();
        this->cst->DoButton(g_step_in_command);

        if (m_track_pc) {
            if (m_beeb_debug_state && m_beeb_debug_state->is_halted) {
                if (m_old_pc != cpu->opcode_pc.w) {
                    // well, *something* happened since last time...
                    m_addr = cpu->opcode_pc.w;
                    m_old_pc = cpu->opcode_pc.w;
                }
            } else {
                m_addr = cpu->opcode_pc.w;
            }
        }

        m_num_lines = 0;
        uint16_t addr = m_addr;
        while (ImGui::GetCursorPosY() <= maxY) {
            ++m_num_lines;
            M6502Word line_addr = {addr};
            bool mos = !!pc_is_mos[line_addr.p.p];
            //m_line_addrs.push_back(line_addr.w);

            ImGuiIDPusher id_pusher(addr);

            uint8_t opcode;
            uint8_t opcode_addr_flags;
            uint8_t opcode_byte_flags;
            this->ReadByte(&opcode,
                           &opcode_addr_flags,
                           &opcode_byte_flags,
                           addr++,
                           false);
            char ascii[4];

            const M6502DisassemblyInfo *di = &cpu->config->disassembly_info[opcode];

            M6502Word operand = {};
            M6502Word operand_addr_flags = {};
            M6502Word operand_byte_flags = {};
            if (di->num_bytes >= 2) {
                this->ReadByte(&operand.b.l,
                               &operand_addr_flags.b.l,
                               &operand_byte_flags.b.l,
                               addr++,
                               false);
            }
            if (di->num_bytes >= 3) {
                this->ReadByte(&operand.b.h,
                               &operand_addr_flags.b.h,
                               &operand_byte_flags.b.h,
                               addr++,
                               false);
            }

            ImGuiStyleColourPusher pusher;

            if (line_addr.w == cpu->opcode_pc.w) {
                pusher.Push(ImGuiCol_Text, ImVec4(1.f, 1.f, 0.f, 1.f));
            }

            const DebugBigPage *line_dbp = this->GetDebugBigPageForAddress(line_addr, false);
            ImGui::Text("$%04x%c%s", line_addr.w, ADDRESS_SUFFIX_SEPARATOR, line_dbp->bp.metadata->aligned_codes);
            this->DoBytePopupGui(line_dbp, line_addr);

            ImGui::SameLine();

            ImGui::TextUnformatted("  ");

            ImGui::SameLine();

            this->TextWithBreakpointBackground(opcode_addr_flags, opcode_byte_flags, "%02x", opcode);
            this->DoBytePopupGui(line_dbp, line_addr);
            if (opcode >= 32 && opcode < 127) {
                ascii[0] = (char)opcode;
            } else {
                ascii[0] = ' ';
            }

            ImGui::SameLine();

            if (di->num_bytes >= 2) {
                this->TextWithBreakpointBackground(operand_addr_flags.b.l, operand_byte_flags.b.l, "%02x", operand.b.l);

                M6502Word operand_l_addr = {(uint16_t)(line_addr.w + 1u)};
                const DebugBigPage *operand_l_dbp = this->GetDebugBigPageForAddress(operand_l_addr, false);
                this->DoBytePopupGui(operand_l_dbp, operand_l_addr);

                if (operand.b.l >= 32 && operand.b.l < 127) {
                    ascii[1] = (char)operand.b.l;
                } else {
                    ascii[1] = ' ';
                }
            } else {
                ImGui::TextUnformatted("  ");
                ascii[1] = ' ';
            }

            ImGui::SameLine();

            if (di->num_bytes >= 3) {
                this->TextWithBreakpointBackground(operand_addr_flags.b.h, operand_byte_flags.b.h, "%02x", operand.b.h);

                M6502Word operand_h_addr = {(uint16_t)(line_addr.w + 2u)};
                const DebugBigPage *operand_h_dbp = this->GetDebugBigPageForAddress(operand_h_addr, false);
                this->DoBytePopupGui(operand_h_dbp, operand_h_addr);

                if (operand.b.h >= 32 && operand.b.h < 127) {
                    ascii[2] = (char)operand.b.h;
                } else {
                    ascii[2] = ' ';
                }
            } else {
                ImGui::TextUnformatted("  ");
                ascii[2] = ' ';
            }

            ImGui::SameLine();

            ImGui::TextUnformatted("  ");

            ImGui::SameLine();

            ascii[3] = 0;
            ImGui::TextUnformatted(ascii);

            ImGui::SameLine();

            ImGui::TextUnformatted("  ");

            ImGui::SameLine();

            ImGui::Text("%s ", di->mnemonic);

            //            ImGui::Text("%04x  %c%c %c%c %c%c  %c%c%c  %s ",
            //                        line_addr.w,
            //                        HEX_CHARS_LC[opcode>>4&15],
            //                        HEX_CHARS_LC[opcode&15],
            //                        di->num_bytes>=2?HEX_CHARS_LC[operand.b.l>>4&15]:' ',
            //                        di->num_bytes>=2?HEX_CHARS_LC[operand.b.l&15]:' ',
            //                        di->num_bytes>=3?HEX_CHARS_LC[operand.b.h>>4&15]:' ',
            //                        di->num_bytes>=3?HEX_CHARS_LC[operand.b.h&15]:' ',
            //                        opcode>=32&&opcode<127?opcode:' ',
            //                        operand.b.l>=32&&operand.b.l<127?operand.b.l:' ',
            //                        operand.b.h>=32&&operand.b.h<127?operand.b.h:' ',
            //                        di->mnemonic);

            switch (di->mode) {
            default:
                ASSERT(0);
                // fall through
            case M6502AddrMode_IMP:
                break;

            case M6502AddrMode_REL:
                {
                    M6502Word dest;
                    dest.w = addr + (uint16_t)(int16_t)(int8_t)operand.b.l;
                    this->AddWord("", dest.w, false, "");
                    this->AddBranchTakenIndicator(IsBranchTaken((M6502Condition)di->branch_condition, p));
                }
                break;

            case M6502AddrMode_IMM:
                {
                    char label[3] = {
                        HEX_CHARS_LC[operand.b.l >> 4 & 15],
                        HEX_CHARS_LC[operand.b.l & 15],
                    };

                    M6502Word imm_addr = {operand.b.l};
                    const DebugBigPage *imm_dbp = this->GetDebugBigPageForAddress(imm_addr, false);
                    this->DoClickableAddress("#$", label, "", imm_dbp, imm_addr);
                }
                break;

            case M6502AddrMode_ZPG:
                this->AddByte("", operand.b.l, false, "");
                break;

            case M6502AddrMode_ZPX:
                this->AddByte("", operand.b.l, false, ",X");
                this->AddByte(IND_PREFIX, operand.b.l + cpu->x, mos, "");
                break;

            case M6502AddrMode_ZPY:
                this->AddByte("", operand.b.l, false, ",Y");
                this->AddByte(IND_PREFIX, operand.b.l + cpu->y, mos, "");
                break;

            case M6502AddrMode_ABS:
                // TODO the MOS flag shouldn't apply to JSR or JMP.
                this->AddWord("", operand.w, mos, "");
                break;

            case M6502AddrMode_ABX:
                this->AddWord("", operand.w, mos, ",X");
                this->AddWord(IND_PREFIX, operand.w + cpu->x, mos, "");
                break;

            case M6502AddrMode_ABY:
                this->AddWord("", operand.w, mos, ",Y");
                this->AddWord(IND_PREFIX, operand.w + cpu->y, mos, "");
                break;

            case M6502AddrMode_INX:
                this->AddByte("(", operand.b.l, false, ",X)");
                this->DoIndirect((operand.b.l + cpu->x) & 0xff, mos, 0xff, 0);
                break;

            case M6502AddrMode_INY:
                this->AddByte("(", operand.b.l, false, "),Y");
                this->DoIndirect(operand.b.l, mos, 0xff, cpu->y);
                break;

            case M6502AddrMode_IND:
                this->AddWord("(", operand.w, false, ")");
                // doesn't handle the 6502 page crossing bug...
                this->DoIndirect(operand.w, mos, 0xffff, 0);
                break;

            case M6502AddrMode_ACC:
                ImGui::SameLine(0.f, 0.f);
                ImGui::TextUnformatted("A");
                break;

            case M6502AddrMode_INZ:
                this->AddByte("(", operand.b.l, false, ")");
                this->DoIndirect(operand.b.l, mos, 0xff, 0);
                break;

            case M6502AddrMode_INDX:
                this->AddWord("(", operand.w, false, ",X)");
                this->DoIndirect(operand.w + cpu->x, mos, 0xffff, 0);
                break;

            case M6502AddrMode_ZPG_REL_ROCKWELL:
                {
                    M6502Word dest;
                    dest.w = addr + (uint16_t)(int16_t)(int8_t)operand.b.h;
                    this->AddByte("", operand.b.l, false, "");
                    this->AddWord(",", dest.w, false, "");

                    //bool taken = false;//TODO
                    uint8_t value;
                    if (this->ReadByte(&value, nullptr, nullptr, operand.b.l, false)) {
                        uint8_t bit;
                        bool set;
                        if (di->branch_condition >= M6502Condition_BR0 && di->branch_condition <= M6502Condition_BR7) {
                            bit = di->branch_condition - M6502Condition_BR0;
                            set = false;
                        } else {
                            ASSERT(di->branch_condition >= M6502Condition_BS0 && di->branch_condition <= M6502Condition_BS7);
                            bit = di->branch_condition - M6502Condition_BS0;
                            set = true;
                        }

                        this->AddBranchTakenIndicator(!!(value & 1 << bit) == set);
                    }
                }
                break;
            }
        }

        if (ImGui::IsWindowHovered()) {
            ImGuiIO &io = ImGui::GetIO();

            m_wheel += io.MouseWheel;

            int wheel = (int)m_wheel;

            m_wheel -= (float)wheel;

            wheel *= 5; //wild guess...

            if (wheel < 0) {
                this->Down(cpu->config, -wheel);
            } else if (wheel > 0) {
                this->Up(cpu->config, wheel);
            }
        }
    }

  private:
    static const char IND_PREFIX[];

    uint16_t m_addr = 0;
    bool m_track_pc = true;
    int32_t m_old_pc = -1;
    char m_address_text[100] = {};
    //std::vector<uint16_t> m_line_addrs;
    std::vector<uint16_t> m_history;
    int m_num_lines = 0;
    //char m_disassembly_text[100];
    float m_wheel = 0;

    static bool IsBranchTaken(M6502Condition condition, M6502P p) {
        switch (condition) {
        case M6502Condition_Always:
            return true;

        case M6502Condition_CC:
            return !p.bits.c;

        case M6502Condition_CS:
            return !!p.bits.c;

        case M6502Condition_VC:
            return !p.bits.v;

        case M6502Condition_VS:
            return !!p.bits.v;

        case M6502Condition_NE:
            return !p.bits.z;

        case M6502Condition_EQ:
            return !!p.bits.z;

        case M6502Condition_PL:
            return !p.bits.n;

        case M6502Condition_MI:
            return !!p.bits.n;

        default:
            ASSERT(false);
            return false;
        }
    }

    static void AddBranchTakenIndicator(bool taken) {
        if (taken) {
            ImGui::SameLine(0.f, 0.f);
            ImGui::TextUnformatted(" (taken)");
        }
    }

    void PRINTF_LIKE(4, 5) TextWithBreakpointBackground(uint8_t addr_flags,
                                                        uint8_t byte_flags,
                                                        const char *fmt, ...) {
        char text[100];
        va_list v;

        va_start(v, fmt);
        vsnprintf(text, sizeof text, fmt, v);
        va_end(v);

        if ((addr_flags | byte_flags) & (BBCMicroByteDebugFlag_BreakExecute |
                                         BBCMicroByteDebugFlag_BreakRead |
                                         BBCMicroByteDebugFlag_BreakWrite)) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::CalcTextSize(text);

            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            draw_list->AddRectFilled(pos, pos + size, IM_COL32(128, 0, 0, 255));
        }

        ImGui::TextUnformatted(text);
    }

    void DoIndirect(uint16_t address, bool mos, uint16_t mask, uint16_t post_index) {
        M6502Word addr;

        this->ReadByte(&addr.b.l, nullptr, nullptr, address, mos);

        ++address;
        address &= mask;

        this->ReadByte(&addr.b.h, nullptr, nullptr, address, mos);

        addr.w += post_index;

        this->AddWord(IND_PREFIX, addr.w, mos, "");
    }

    void AddWord(const char *prefix, uint16_t w, bool mos, const char *suffix) {
        const DebugBigPage *dbp = this->GetDebugBigPageForAddress({w}, mos);

        char label[100];
        snprintf(label, sizeof label, "$%04x%c%s", w, ADDRESS_SUFFIX_SEPARATOR, dbp->bp.metadata->minimal_codes);

        //static_assert(sizeof dbp->bp.metadata->codes == 3);
        //char label[] = {
        //    dbp->bp.metadata->codes[0],
        //    dbp->bp.metadata->codes[1],
        //    ADDRESS_PREFIX_SEPARATOR,
        //    '$',
        //    HEX_CHARS_LC[w >> 12 & 15],
        //    HEX_CHARS_LC[w >> 8 & 15],
        //    HEX_CHARS_LC[w >> 4 & 15],
        //    HEX_CHARS_LC[w & 15],
        //    0,
        //};

        this->DoClickableAddress(prefix, label, suffix, dbp, {w});
    }

    void AddByte(const char *prefix, uint8_t value, bool mos, const char *suffix) {
        const DebugBigPage *dbp = this->GetDebugBigPageForAddress({value}, mos);

        char label[100];
        snprintf(label, sizeof label, "$%02x%c%s", value, ADDRESS_SUFFIX_SEPARATOR, dbp->bp.metadata->minimal_codes);

        //static_assert(sizeof dbp->bp.metadata->codes == 3);
        //char label[] = {
        //    dbp->bp.metadata->codes[0],
        //    dbp->bp.metadata->codes[1],
        //    ADDRESS_PREFIX_SEPARATOR,
        //    '$',
        //    HEX_CHARS_LC[value >> 4 & 15],
        //    HEX_CHARS_LC[value & 15],
        //    0,
        //};

        this->DoClickableAddress(prefix, label, suffix, dbp, {value});
    }

    void DoClickableAddress(const char *prefix,
                            const char *label,
                            const char *suffix,
                            const DebugBigPage *dbp,
                            M6502Word addr) {
        if (prefix[0] != 0) {
            ImGui::SameLine(0.f, 0.f);
            ImGui::TextUnformatted(prefix);
        }

        ImGui::SameLine(0.f, 0.f);

        {
            // No point using SmallButton - it doesn't set the
            // horizontal frame padding to 0.

            ImGuiStyleVarPusher pusher(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));

            if (ImGui::ButtonEx(label, ImVec2(0.f, 0.f), ImGuiButtonFlags_AlignTextBaseLine)) {
                this->GoTo(addr.w); //TODO...
            }
        }

        this->DoBytePopupGui(dbp, addr);

        if (suffix[0] != 0) {
            ImGui::SameLine(0.f, 0.f);
            ImGui::TextUnformatted(suffix);
        }
    }

    void GoTo(uint16_t address) {
        if (m_history.empty() || m_addr != m_history.back()) {
            m_history.push_back(m_addr);
        }
        m_track_pc = false;
        m_addr = address;
    }

    void Up(const M6502Config *config, int n) {
        for (int i = 0; i < n; ++i) {
            uint8_t opcode;

            if (!this->ReadByte(&opcode, nullptr, nullptr, m_addr - 1, false)) {
                --m_addr;
                continue;
            }

            if (config->disassembly_info[opcode].num_bytes == 1) {
                --m_addr;
                continue;
            }

            if (!this->ReadByte(&opcode, nullptr, nullptr, m_addr - 2, false)) {
                --m_addr;
                continue;
            }

            if (config->disassembly_info[opcode].num_bytes == 2) {
                m_addr -= 2;
                continue;
            }

            m_addr -= 3;
        }
    }

    void Down(const M6502Config *config, int n) {
        for (int i = 0; i < n; ++i) {
            uint8_t opcode;
            if (!this->ReadByte(&opcode, nullptr, nullptr, m_addr, false)) {
                ++m_addr;
            } else {
                m_addr += config->disassembly_info[opcode].num_bytes;
            }
        }
    }

    void ByteRegUI(const char *name, uint8_t value) {
        ImGui::Text("%s=", name);
        ImGui::SameLine(0, 0);
        ImGui::Text("$%02x", value);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            this->ByteRegPopupContentsUI(value);
            ImGui::EndTooltip();
        }
    }

    void ByteRegPopupContentsUI(uint8_t value) {
        ImGui::Text("% 3d %3uu $%02x %s", (int8_t)value, value, value, BINARY_BYTE_STRINGS[value]);
    }

    void WordRegUI(const char *name, M6502Word value) {
        ImGui::Text("%s=", name);
        ImGui::SameLine(0, 0);
        ImGui::Text("$%04x", value.w);
    }

    void StepOver() {
        m_beeb_window->DebugStepOver(m_dso);
    }

    void StepIn() {
        m_beeb_window->DebugStepIn(m_dso);
    }

    bool IsRunEnabled() const {
        return m_beeb_window->DebugIsRunEnabled();
    }
};

const char DisassemblyDebugWindow::IND_PREFIX[] = " --> $";

std::unique_ptr<SettingsUI> CreateHostDisassemblyDebugWindow(BeebWindow *beeb_window,
                                                             bool initial_track_pc) {
    auto ui = CreateDebugUI<DisassemblyDebugWindow>(beeb_window);

    ui->SetTrackPC(initial_track_pc);

    return ui;
}

std::unique_ptr<SettingsUI> CreateParasiteDisassemblyDebugWindow(BeebWindow *beeb_window,
                                                                 bool initial_track_pc) {
    auto ui = CreateParasiteDebugUI<DisassemblyDebugWindow>(beeb_window);

    ui->SetTrackPC(initial_track_pc);

    return ui;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CRTCDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        const CRTC *c = &m_beeb_state->crtc;

        uint16_t cursor_address = this->GetBeebAddressFromCRTCAddress(c->m_registers.bits.cursorh, c->m_registers.bits.cursorl);
        uint16_t display_address = this->GetBeebAddressFromCRTCAddress(c->m_registers.bits.addrh, c->m_registers.bits.addrl);
        uint16_t char_addr = this->GetBeebAddressFromCRTCAddress(c->m_st.char_addr.b.h, c->m_st.char_addr.b.l);
        uint16_t line_addr = this->GetBeebAddressFromCRTCAddress(c->m_st.line_addr.b.h, c->m_st.line_addr.b.l);
        uint16_t next_line_addr = this->GetBeebAddressFromCRTCAddress(c->m_st.next_line_addr.b.h, c->m_st.next_line_addr.b.l);

        if (ImGui::CollapsingHeader("Register Values")) {
            ImGui::Text("Address = $%02x %03u", c->m_address, c->m_address);
            for (size_t i = 0; i < 18; ++i) {
                ImGui::Text("R%zu = $%02x %03u %s", i, c->m_registers.values[i], c->m_registers.values[i], BINARY_BYTE_STRINGS[c->m_registers.values[i]]);
            }
            ImGui::Separator();
        }

        ImGui::Text("H Displayed = %u, Total = %u", c->m_registers.bits.nhd, c->m_registers.bits.nht);
        ImGui::Text("V Displayed = %u, Total = %u", c->m_registers.bits.nvd, c->m_registers.bits.nvt);
        ImGui::Text("Scanlines = %u * %u + %u = %u", c->m_registers.bits.nvd, c->m_registers.bits.nr + 1, c->m_registers.bits.nadj, c->m_registers.bits.nvd * (c->m_registers.bits.nr + 1) + c->m_registers.bits.nadj);
        ImGui::Text("Address = $%04x", display_address);
        ImGui::Text("(Wrap Adjustment = $%04x)", BBCMicro::SCREEN_WRAP_ADJUSTMENTS[m_beeb_state->addressable_latch.bits.screen_base] << 3);
        ImGui::Separator();
        ImGui::Text("HSync Pos = %u, Width = %u", c->m_registers.bits.nhsp, c->m_registers.bits.nsw.bits.wh);
        ImGui::Text("VSync Pos = %u, Width = %u", c->m_registers.bits.nvsp, c->m_registers.bits.nsw.bits.wv);
        ImGui::Text("Interlace Sync = %s, Video = %s", BOOL_STR(c->m_registers.bits.r8.bits.s), BOOL_STR(c->m_registers.bits.r8.bits.v));
        ImGui::Text("Delay Mode = %s", DELAY_NAMES[c->m_registers.bits.r8.bits.d]);
        ImGui::Separator();
        ImGui::Text("Cursor Start = %u, End = %u, Mode = %s", c->m_registers.bits.ncstart.bits.start, c->m_registers.bits.ncend, GetCRTCCursorModeEnumName(c->m_registers.bits.ncstart.bits.mode));
        ImGui::Text("Cursor Delay Mode = %s", DELAY_NAMES[c->m_registers.bits.r8.bits.c]);
        ImGui::Text("Cursor Address = $%04x", cursor_address);
        ImGui::Separator();
        ImGui::Text("Column = %u, hdisp=%s", c->m_st.column, BOOL_STR(c->m_st.hdisp));
        ImGui::Text("Row = %u, Raster = %u, vdisp=%s", c->m_st.row, c->m_st.raster, BOOL_STR(c->m_st.vdisp));
        ImGui::Text("Char address = $%04X (CRTC) / $%04X (BBC)", c->m_st.char_addr.w, char_addr);
        ImGui::Text("Line address = $%04X (CRTC) / $%04X (BBC)", c->m_st.line_addr.w, line_addr);
        ImGui::Text("Next line address = $%04X (CRTC) / $%04X (BBC)", c->m_st.next_line_addr.w, next_line_addr);
        ImGui::Text("DISPEN queue = %%%s", BINARY_BYTE_STRINGS[c->m_st.skewed_display]);
        ImGui::Text("CUDISP queue = %%%s", BINARY_BYTE_STRINGS[c->m_st.skewed_cudisp]);
        ImGui::Text("VSync counter = %d", c->m_st.vsync_counter);
        ImGui::Text("HSync counter = %d", c->m_st.hsync_counter);
        ImGui::Text("VAdj counter = %d", c->m_st.vadj_counter);
        ImGui::Text("check_vadj = %s", BOOL_STR(c->m_st.check_vadj));
        ImGui::Text("in_vadj = %s", BOOL_STR(c->m_st.in_vadj));
        ImGui::Text("end_of_vadj_latched = %s", BOOL_STR(c->m_st.end_of_vadj_latched));
        ImGui::Text("had_vsync_this_row = %s", BOOL_STR(c->m_st.had_vsync_this_row));
        ImGui::Text("end_of_main_latched = %s", BOOL_STR(c->m_st.end_of_main_latched));
        ImGui::Text("do_even_frame_logic = %s", BOOL_STR(c->m_st.do_even_frame_logic));
        ImGui::Text("first_scanline = %s", BOOL_STR(c->m_st.first_scanline));
        ImGui::Text("in_dummy_raster = %s", BOOL_STR(c->m_st.in_dummy_raster));
        ImGui::Text("end_of_frame_latched = %s", BOOL_STR(c->m_st.end_of_frame_latched));
        ImGui::Text("cursor = %s", BOOL_STR(c->m_st.cursor));
    }

  private:
    static const char *const INTERLACE_NAMES[];
    static const char *const DELAY_NAMES[];

    uint16_t GetBeebAddressFromCRTCAddress(uint8_t h, uint8_t l) {
        M6502Word addr;
        addr.b.h = h;
        addr.b.l = l;

        if (addr.w & 0x2000) {
            uint16_t base = 0x7c00;
            if (!(addr.w >> 11 & 1)) {
                if (CanDisplayTeletextAt3C00(m_beeb_state->type->type_id)) {
                    base = 0x3c00;
                }
            }

            return (addr.w & 0x3ff) | base;
        } else {
            if (addr.w & 0x1000) {
                addr.w -= BBCMicro::SCREEN_WRAP_ADJUSTMENTS[m_beeb_state->addressable_latch.bits.screen_base];
                addr.w &= ~0x1000u;
            }

            return addr.w << 3;
        }
    }
};

const char *const CRTCDebugWindow::INTERLACE_NAMES[] = {"Normal", "Normal", "Interlace sync", "Interlace sync+video"};

const char *const CRTCDebugWindow::DELAY_NAMES[] = {"0", "1", "2", "Disabled"};

std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<CRTCDebugWindow>(beeb_window, ImVec2(375, 450));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoULADebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        //VideoULA::Control control;
        //uint8_t palette[16];
        //VideoDataPixel nula_palette[16];
        //uint8_t nula_flash[16];
        //uint8_t nula_direct_palette;
        //uint8_t nula_disable_a1;
        //uint8_t nula_scroll_offset;
        //uint8_t nula_blanking_size;
        //VideoULA::NuLAAttributeMode nula_attribute_mode;
        //bool nula;

        //{
        //    std::unique_lock<Mutex> lock;
        //    const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

        //    const VideoULA *u = m->DebugGetVideoULA();

        //    control = u->control;
        //    memcpy(palette, u->m_palette, 16);
        //    memcpy(nula_palette, u->output_palette, 16 * sizeof(VideoDataPixel));
        //    memcpy(nula_flash, u->m_flash, 16);
        //    nula_direct_palette = u->m_direct_palette;
        //    nula_disable_a1 = u->m_disable_a1;
        //    nula_scroll_offset = u->m_scroll_offset;
        //    nula_blanking_size = u->m_blanking_size;
        //    nula_attribute_mode = u->m_attribute_mode;
        //    nula = u->nula;
        //}

        const VideoULA *u = &m_beeb_state->video_ula;

        if (ImGui::CollapsingHeader("Register Values")) {
            ImGui::Text("Control = $%02x %03u %s", u->control.value, u->control.value, BINARY_BYTE_STRINGS[u->control.value]);
            for (size_t i = 0; i < 16; ++i) {
                uint8_t p = u->m_palette[i];
                ImGui::Text("Palette[%zu] = $%01x %02u %s ", i, p, p, BINARY_BYTE_STRINGS[p] + 4);

                uint8_t colour = p & 7;
                if (p & 8) {
                    if (u->control.bits.flash) {
                        colour ^= 7;
                    }
                }

                ImGui::SameLine();

                ImGui::ColorButton(COLOUR_NAMES[colour], COLOUR_COLOURS[colour]);

                ImGui::SameLine();

                if (p & 8) {
                    ImGui::Text("(%s/%s)", COLOUR_NAMES[p & 7], COLOUR_NAMES[(p & 7) ^ 7]);
                } else {
                    ImGui::Text("(%s)", COLOUR_NAMES[colour]);
                }
            }
            ImGui::Separator();
        }

        ImGui::Text("Flash colour = %u", u->control.bits.flash);
        ImGui::Text("Teletext output = %s", BOOL_STR(u->control.bits.teletext));
        ImGui::Text("Chars per line = %u", (1 << u->control.bits.line_width) * 10);
        ImGui::Text("6845 clock = %u MHz", 1 + u->control.bits.fast_6845);
        ImGui::Text("Cursor Shape = %s", CURSOR_SHAPES[u->control.bits.cursor]);

        for (uint8_t i = 0; i < 16; i += 4) {
            ImGui::Text("Palette:");
            ImGuiStyleVarPusher vpusher(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));

            for (uint8_t j = 0; j < 4; ++j) {
                uint8_t index = i + j;
                uint8_t entry = u->m_palette[index];

                uint8_t colour = entry & 7;
                if (entry & 8) {
                    if (u->control.bits.flash) {
                        colour ^= 7;
                    }
                }

                ImGui::SameLine();
                ImGui::Text(" %x=%x", index, entry);
                ImGui::SameLine();
                ImGui::ColorButton(COLOUR_NAMES[colour], COLOUR_COLOURS[colour]);
            }
        }

        ImGuiTreeNodeFlags nula_section_flags = 0;
        if (!u->m_disable_a1) {
            nula_section_flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        if (u->nula) {
            if (ImGui::CollapsingHeader("Video NuLA", nula_section_flags)) {
                ImGui::Text("Enabled: %s", BOOL_STR(!u->m_disable_a1));

                ImGui::Text("Direct palette mode: %s", BOOL_STR(u->m_direct_palette));
                ImGui::Text("Scroll offset: %u", u->m_scroll_offset);
                ImGui::Text("Blanking size: %u", u->m_blanking_size);
                ImGui::Text("Attribute mode: %s", BOOL_STR(u->m_attribute_mode.bits.enabled));
                ImGui::Text("Text attribute mode: %s", BOOL_STR(u->m_attribute_mode.bits.text));

                for (uint8_t i = 0; i < 16; i += 4) {
                    ImGui::Text("Palette:");

                    ImGuiStyleVarPusher var_pusher(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));

                    for (uint8_t j = 0; j < 4; ++j) {
                        uint8_t index = i + j;
                        const VideoDataPixel *e = &u->output_palette[index];

                        ImGuiIDPusher id_pusher(index);

                        ImGui::SameLine();
                        ImGui::Text(" %x=%x%x%x", index, e->bits.r, e->bits.g, e->bits.b);

                        ImGui::SameLine();
                        ImGui::ColorButton("",
                                           ImVec4(e->bits.r / 15.f,
                                                  e->bits.g / 15.f,
                                                  e->bits.b / 15.f,
                                                  1.f),
                                           ImGuiColorEditFlags_NoAlpha);
                    }
                }
            }
        }
    }

  private:
    static const char *const COLOUR_NAMES[];
    static const ImVec4 COLOUR_COLOURS[];
    static const char *const CURSOR_SHAPES[];
};

const char *const VideoULADebugWindow::COLOUR_NAMES[] = {
    "Black",
    "Red",
    "Green",
    "Yellow",
    "Blue",
    "Magenta",
    "Cyan",
    "White",
};

const ImVec4 VideoULADebugWindow::COLOUR_COLOURS[] = {
    {0.f, 0.f, 0.f, 1.f},
    {1.f, 0.f, 0.f, 1.f},
    {0.f, 1.f, 0.f, 1.f},
    {1.f, 1.f, 0.f, 1.f},
    {0.f, 0.f, 1.f, 1.f},
    {1.f, 0.f, 1.f, 1.f},
    {0.f, 1.f, 1.f, 1.f},
    {1.f, 1.f, 1.f, 1.f},
};

const char *const VideoULADebugWindow::CURSOR_SHAPES[] = {
    "....", ".**.", ".*..", ".***", "*...", "*.**", "**..", "****"};

std::unique_ptr<SettingsUI> CreateVideoULADebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<VideoULADebugWindow>(beeb_window, ImVec2(356, 400));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class R6522DebugWindow : public DebugUI {
  public:
    R6522DebugWindow() {
        this->SetDefaultSize(ImVec2(422, 450));
    }

  protected:
    void DoRegisterValuesGui(const R6522 &via, const std::shared_ptr<const BBCMicro::DebugState> &debug_state, R6522::IRQ BBCMicro::HardwareDebugState::*irq_mptr) {
        this->DoPortRegisterValuesGui('A', via.a);
        this->DoPortRegisterValuesGui('B', via.b);

        M6502Word t1l;
        t1l.b.l = via.m_t1ll;
        t1l.b.h = via.m_t1lh;

        ImGui::Text("T1 : $%04x %05d %s%s", via.m_t1, via.m_t1, BINARY_BYTE_STRINGS[via.m_t1 >> 8 & 0xff], BINARY_BYTE_STRINGS[via.m_t1 & 0xff]);
        ImGui::Text("T1L: $%04x %05d %s%s", t1l.w, t1l.w, BINARY_BYTE_STRINGS[t1l.b.h], BINARY_BYTE_STRINGS[t1l.b.l]);
        ImGui::Text("T2 : $%04x %05d %s%s", via.m_t2, via.m_t2, BINARY_BYTE_STRINGS[via.m_t2 >> 8 & 0xff], BINARY_BYTE_STRINGS[via.m_t2 & 0xff]);
        ImGui::Text("SR : $%02x %03d %s", via.m_sr, via.m_sr, BINARY_BYTE_STRINGS[via.m_sr]);
        ImGui::Text("ACR: PA latching = %s", BOOL_STR(via.m_acr.bits.pa_latching));
        ImGui::Text("ACR: PB latching = %s", BOOL_STR(via.m_acr.bits.pb_latching));
        ImGui::Text("ACR: Shift mode = %s", ACR_SHIFT_MODES[via.m_acr.bits.sr]);
        ImGui::Text("ACR: T2 mode = %s", via.m_acr.bits.t2_count_pb6 ? "Count PB6 pulses" : "Timed interrupt");
        ImGui::Text("ACR: T1 continuous = %s, output PB7 = %s", BOOL_STR(via.m_acr.bits.t1_continuous), BOOL_STR(via.m_acr.bits.t1_output_pb7));
        ImGui::Text("PCR: CA1 = %cve edge", via.m_pcr.bits.ca1_pos_irq ? '+' : '-');
        ImGui::Text("PCR: CA2 = %s", PCR_CONTROL_MODES[via.m_pcr.bits.ca2_mode]);
        ImGui::Text("PCR: CB1 = %cve edge", via.m_pcr.bits.cb1_pos_irq ? '+' : '-');
        ImGui::Text("PCR: CB2 = %s", PCR_CONTROL_MODES[via.m_pcr.bits.cb2_mode]);

        ImGui::Text("     [%-3s][%-3s][%-3s][%-3s][%-3s][%-3s][%-3s]", IRQ_NAMES[6], IRQ_NAMES[5], IRQ_NAMES[4], IRQ_NAMES[3], IRQ_NAMES[2], IRQ_NAMES[1], IRQ_NAMES[0]);
        ImGui::Text("IFR:  %2u   %2u   %2u   %2u   %2u   %2u   %2u", via.ifr.value & 1 << 6, via.ifr.value & 1 << 5, via.ifr.value & 1 << 4, via.ifr.value & 1 << 3, via.ifr.value & 1 << 2, via.ifr.value & 1 << 1, via.ifr.value & 1 << 0);
        ImGui::Text("IER:  %2u   %2u   %2u   %2u   %2u   %2u   %2u", via.ier.value & 1 << 6, via.ier.value & 1 << 5, via.ier.value & 1 << 4, via.ier.value & 1 << 3, via.ier.value & 1 << 2, via.ier.value & 1 << 1, via.ier.value & 1 << 0);

        if (!!debug_state) {
            ImGui::Separator();

            bool changed = false;

            BBCMicro::HardwareDebugState hw = debug_state->hw;
            R6522::IRQ *irq = &(hw.*irq_mptr);

            ImGui::Text("Break: ");

            for (uint8_t i = 0; i < 7; ++i) {
                uint8_t bit = 6 - i;
                uint8_t mask = 1 << bit;

                ImGui::SameLine();

                bool value = !!(irq->value & mask);
                if (ImGui::Checkbox(IRQ_NAMES[bit], &value)) {
                    irq->value &= ~mask;
                    if (value) {
                        irq->value |= mask;
                    }
                    changed = true;
                }
            }

            if (changed) {
                m_beeb_thread->Send(std::make_shared<BeebThread::CallbackMessage>(
                    [hw](BBCMicro *m) -> void {
                        m->SetHardwareDebugState(hw);
                    }));
            }
        }
    }

  private:
    void DoPortRegisterValuesGui(char port, const R6522::Port &p) {
        ImGui::Text("Port %c: Pins = $%02x %03d %s", port, p.p, p.p, BINARY_BYTE_STRINGS[p.p]);
        ImGui::Text("Port %c: DDR%c = $%02x %03d %s", port, port, p.ddr, p.ddr, BINARY_BYTE_STRINGS[p.ddr]);
        ImGui::Text("Port %c: OR%c  = $%02x %03d %s", port, port, p.or_, p.or_, BINARY_BYTE_STRINGS[p.or_]);
        ImGui::Text("Port %c: C%c1 = %s C%c2 = %s", port, port, BOOL_STR(p.c1), port, BOOL_STR(p.c2));
    }

    static const char *const ACR_SHIFT_MODES[];
    static const char *const PCR_CONTROL_MODES[];
    static const char *const IRQ_NAMES[];
};

// indexed by bit
const char *const R6522DebugWindow::IRQ_NAMES[] = {
    "CA2",
    "CA1",
    "SR",
    "CB2",
    "CB1",
    "T2",
    "T1",
};

const char *const R6522DebugWindow::ACR_SHIFT_MODES[] = {
    "Off",
    "In, T2",
    "In, clock",
    "In, CB1",
    "Out, free, T2",
    "Out, T2",
    "Out, clock",
    "Out, CB1",
};

const char *const R6522DebugWindow::PCR_CONTROL_MODES[] = {
    "Input -ve edge",
    "Indep. IRQ input -ve edge",
    "Input +ve edge",
    "Indep. IRQ input +ve edge",
    "Handshake output",
    "Pulse output",
    "Low output",
    "High output",
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SystemVIADebugWindow : public R6522DebugWindow {
  public:
  protected:
    void DoImGui2() override {
        const MC146818 *rtc = m_beeb_state->DebugGetRTC();
        const PCD8572 *eeprom = m_beeb_state->DebugGetEEPROM();

        this->DoRegisterValuesGui(m_beeb_state->system_via, m_beeb_debug_state, &BBCMicro::HardwareDebugState::system_via_irq_breakpoints);

        ImGui::Separator();

        BBCMicroState::SystemVIAPB pb;
        pb.value = m_beeb_state->system_via.b.p;

        ImGui::Text("Port B inputs:");

        if (HasADC(m_beeb_state->type->type_id)) {
            ImGui::BulletText("Joystick 0 Fire = %s", BOOL_STR(!pb.bits.not_joystick0_fire));
            ImGui::BulletText("Joystick 1 Fire = %s", BOOL_STR(!pb.bits.not_joystick1_fire));
        }

        if (HasSpeech(m_beeb_state->type->type_id)) {
            ImGui::BulletText("Speech Ready = %u, IRQ = %u", pb.b_bits.speech_ready, pb.b_bits.speech_interrupt);
        }

        ImGui::Separator();

        ImGui::Text("Port B outputs:");

        ImGui::BulletText("Latch Bit = %u, Value = %u", pb.bits.latch_index, pb.bits.latch_value);

        if (rtc) {
            ImGui::BulletText("RTC CS = %u, AS = %u",
                              pb.m128_bits.rtc_chip_select,
                              pb.m128_bits.rtc_address_strobe);
            ImGui::BulletText("(FYI: RTC address register value = %u)", rtc->GetAddress());
        } else if (eeprom) {
            ImGui::BulletText("EEPROM clock = %u, data = %u", pb.mcompact_bits.clk, pb.mcompact_bits.data);
            ImGui::BulletText("(FYI: EEPROM address: %u ($%02X))", eeprom->addr, eeprom->addr);
        }

        ImGui::Separator();

        BBCMicroState::AddressableLatch latch = m_beeb_state->addressable_latch;
        ImGui::Text("Addressable latch:");
        ImGui::Text("Value: %%%s ($%02x) (%03u)", BINARY_BYTE_STRINGS[latch.value], latch.value, latch.value);

        ImGui::BulletText("Keyboard Write = %s", BOOL_STR(!latch.bits.not_kb_write));
        ImGui::BulletText("Sound Write = %s", BOOL_STR(!latch.bits.not_sound_write));
        ImGui::BulletText("Screen Wrap Size = $%04x", BBCMicro::SCREEN_WRAP_ADJUSTMENTS[latch.bits.screen_base] << 3);
        ImGui::BulletText("Caps Lock LED = %s", BOOL_STR(latch.bits.caps_lock_led));
        ImGui::BulletText("Shift Lock LED = %s", BOOL_STR(latch.bits.shift_lock_led));

        if (rtc) {
            ImGui::BulletText("RTC Read = %u, DS = %u", latch.m128_bits.rtc_read, latch.m128_bits.rtc_data_strobe);
        } else {
            ImGui::BulletText("Speech Read = %u, Write = %u", latch.b_bits.speech_read, latch.b_bits.speech_write);
        }
    }

  private:
};

std::unique_ptr<SettingsUI> CreateSystemVIADebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<SystemVIADebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class UserVIADebugWindow : public R6522DebugWindow {
  public:
  protected:
    void DoImGui2() override {
        this->DoRegisterValuesGui(m_beeb_state->user_via, m_beeb_debug_state, &BBCMicro::HardwareDebugState::user_via_irq_breakpoints);
    }

  private:
};

std::unique_ptr<SettingsUI> CreateUserVIADebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<UserVIADebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class NVRAMDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        if (const MC146818 *rtc = m_beeb_state->DebugGetRTC()) {
            this->DoNVRAMUI(rtc->m_regs.bits.ram, sizeof rtc->m_regs.bits.ram);
        } else if (const PCD8572 *eeprom = m_beeb_state->DebugGetEEPROM()) {
            this->DoNVRAMUI(eeprom->ram, sizeof eeprom->ram);
        } else {
            ImGui::Text("This computer has no non-volatile RAM.");
        }
    }

  private:
    void
    DoNVRAMUI(const uint8_t *nvram, size_t nvram_size_bytes) const {
        if (ImGui::CollapsingHeader("NVRAM contents")) {
            for (size_t i = 0; i < nvram_size_bytes; ++i) {
                uint8_t value = nvram[i];
                ImGui::Text("%3zu. %3d %3uu $%02x %%%s", i, value, value, value, BINARY_BYTE_STRINGS[value]);
            }
        }
        ImGui::Separator();

        if (nvram_size_bytes >= 50) {
            ImGui::Text("Econet station number: $%02X\n", nvram[0]);
            ImGui::Text("File server station number: $%02X\n", nvram[1]);
            ImGui::Text("File server network number: $%02X\n", nvram[2]);
            ImGui::Text("Printer server station number: $%02X\n", nvram[3]);
            ImGui::Text("Printer server station number: $%02X\n", nvram[4]);
            ImGui::Text("Default ROMs: Filing system: %d\n", nvram[5] & 15);
            ImGui::Text("              Language: %d\n", nvram[5] >> 4);
            {
                char roms_str[17] = "0123456789ABCDEF";

                uint16_t tmp = nvram[6] | nvram[7] << 8;
                for (size_t i = 0; i < 16; ++i) {
                    if (!(tmp & 1 << i)) {
                        roms_str[i] = '_';
                    }
                }

                ImGui::Text("Inserted ROMs: %s", roms_str);
            }
            ImGui::Text("EDIT ROM byte: $%02X (%d)\n", nvram[8], nvram[8]);
            ImGui::Text("Telecommunication applications byte: $%02X (%d)\n", nvram[9], nvram[9]);
            ImGui::Text("Default MODE: %d\n", nvram[10] & 7);
            ImGui::Text("Default Shadow RAM: %s\n", BOOL_STR(nvram[10] & 8));
            ImGui::Text("Default Interlace: %s\n", BOOL_STR((nvram[10] & 16) == 0));
            ImGui::Text("Default *TV: %d\n", (nvram[10] >> 5 & 3) - (nvram[10] >> 5 & 4));
            ImGui::Text("Default FDRIVE: %d\n", nvram[11] & 7);
            ImGui::Text("Default Shift lock: %s\n", BOOL_STR(nvram[11] & 8));
            ImGui::Text("Default No lock: %s\n", BOOL_STR(nvram[11] & 16));
            ImGui::Text("Default Caps lock: %s\n", BOOL_STR(nvram[11] & 32));
            ImGui::Text("Default ADFS load dir: %s\n", BOOL_STR(nvram[11] & 64));
            // nvram[11] contrary to what NAUG says...
            ImGui::Text("Default drive: %s\n", nvram[11] & 128 ? "floppy drive" : "hard drive");
            ImGui::Text("Keyboard auto-repeat delay: %d\n", nvram[12]);
            ImGui::Text("Keyboard auto-repeat rate: %d\n", nvram[13]);
            ImGui::Text("Printer ignore char: %d (0x%02X)\n", nvram[14], nvram[14]);
            ImGui::Text("Tube on: %s\n", BOOL_STR(nvram[15] & 1));
            ImGui::Text("Use printer ignore char: %s\n", BOOL_STR((nvram[15] & 2) == 0));
            ImGui::Text("Serial baud rate index: %d\n", nvram[15] >> 2 & 7);
            ImGui::Text("*FX5 setting: %d\n", nvram[15] >> 5 & 7);
            // 16 bit 0 unused
            ImGui::Text("Default beep volume: %s\n", nvram[16] & 2 ? "loud" : "quiet");
            ImGui::Text("Default Tube: %s\n", nvram[16] & 4 ? "external" : "internal");
            ImGui::Text("Default scrolling: %s\n", nvram[16] & 8 ? "protected" : "enabled");
            ImGui::Text("Default boot mode: %s\n", nvram[16] & 16 ? "auto boot" : "no boot");
            ImGui::Text("Default serial data format: %d\n", nvram[16] >> 5 & 7);
            ImGui::Text("Unused byte 17: %u ($%02X)", nvram[17], nvram[17]);
            ImGui::Text("Compact joystick mode: %s", nvram[18] & 0x20 ? "Switched" : "Proportional");
            ImGui::Text("Compact STICK value: %d", nvram[18] & 0xf);
            ImGui::Text("Compact country code: %u ($%02X)", nvram[19], nvram[19]);
        }
    }
};

std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<NVRAMDebugWindow>(beeb_window, ImVec2(400, 400));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SN76489DebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        const SN76489 *sn = &m_beeb_state->sn76489;

        ImGui::Text("Write Enable: %s", BOOL_STR(!m_beeb_state->addressable_latch.bits.not_sound_write));

        for (int i = 0; i < 3; ++i) {
            const SN76489::ChannelValues *values = &sn->m_state.channels[i].values;

            ImGui::Text("Tone %d: vol=%-2d freq=%-5u (0x%04x) (%uHz)",
                        i, values->vol, values->freq, values->freq, GetHz(values->freq));
        }

        {
            const char *type;
            if (sn->m_state.channels[3].values.freq & 4) {
                type = "White";
            } else {
                type = "Periodic";
            }

            uint16_t sn_freq;
            const char *suffix = "";
            switch (sn->m_state.channels[3].values.freq & 3) {
            default:
            case 0:
                sn_freq = 0x10;
                break;

            case 1:
                sn_freq = 0x20;
                break;

            case 2:
                sn_freq = 0x40;
                break;

            case 3:
                suffix = " (Tone 2)";
                sn_freq = sn->m_state.channels[2].values.freq;
                break;
            }

            ImGui::Text("Noise : vol=%-2d freq=%-5u (0x%04x) (%uHz)",
                        sn->m_state.channels[3].values.vol, sn_freq, sn_freq, GetHz(sn_freq));
            ImGui::Text("        %s%s", type, suffix);
            ImGui::Text("        seed: $%04x %%%s%s",
                        sn->m_state.noise_seed, BINARY_BYTE_STRINGS[sn->m_state.noise_seed >> 8], BINARY_BYTE_STRINGS[sn->m_state.noise_seed & 0xff]);
        }
    }

  private:
    static uint32_t GetHz(uint16_t sn_freq) {
        if (sn_freq == 0) {
            sn_freq = 1024;
        }
        return 4000000u / (sn_freq * 32u);
    }
};

std::unique_ptr<SettingsUI> CreateSN76489DebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<SN76489DebugWindow>(beeb_window, ImVec2(350, 150));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t ALL_USER_MEM_BIG_PAGES[16] = {};

class PagingDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        this->DoDebugPageOverrideImGui();

        PagingState paging = m_beeb_state->paging;
        (*m_beeb_state->type->apply_dso_fn)(&paging, m_dso);

        MemoryBigPageTables tables;
        uint32_t paging_flags;
        (*m_beeb_state->type->get_mem_big_page_tables_fn)(&tables, &paging_flags, paging);

        bool all_user = memcmp(tables.pc_mem_big_pages_set, ALL_USER_MEM_BIG_PAGES, 16) == 0;

        ImGui::Separator();

        ImGui::Columns(3, "paging_columns");

        ImGui::Text("Range");
        ImGui::NextColumn();
        ImGui::Text("User code sees...");
        ImGui::NextColumn();
        ImGui::Text("MOS code sees...");
        ImGui::NextColumn();

        ImGui::Separator();

        for (size_t i = 0; i < 16; ++i) {
            ImGui::Text("%04zx - %04zx", i << 12, i << 12 | 0xfff);
            ImGui::NextColumn();

            this->DoTypeColumn(m_beeb_state->type, tables, paging_flags, 0, i);
            ImGui::NextColumn();

            if (all_user) {
                ImGui::TextUnformatted("N/A");
            } else {
                this->DoTypeColumn(m_beeb_state->type, tables, paging_flags, 1, i);
            }
            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        ImGui::Separator();

        ImGui::Text("Display memory: %s", paging_flags & PagingFlags_DisplayShadow ? "Shadow RAM" : "Main RAM");

        if (all_user) {
            ImGui::TextUnformatted("Paging setup does not distinguish between user code and MOS code");
        } else {
            std::string regions;

            size_t a = 0;
            while (a < 16) {
                if (tables.pc_mem_big_pages_set[a]) {
                    size_t b = a;
                    while (b < 16 && tables.pc_mem_big_pages_set[b]) {
                        ++b;
                    }

                    if (!regions.empty()) {
                        regions += "; ";
                    }

                    regions += strprintf("$%04zx - $%04zx", a << 12, (b << 12) - 1);
                    a = b;
                } else {
                    ++a;
                }
            }

            ImGui::Text("MOS code regions: %s", regions.c_str());
        }
    }

  private:
    void DoTypeColumn(const std::shared_ptr<const BBCMicroType> &type, const MemoryBigPageTables &tables, uint32_t paging_flags, size_t index, size_t mem_big_page_index) {
        BigPageIndex big_page_index = tables.mem_big_pages[index][mem_big_page_index];
        const BigPageMetadata *metadata = &type->big_pages_metadata[big_page_index.i];

        ImGui::Text("%s (%u)", metadata->description.c_str(), metadata->debug_flags_index.i);
        if (big_page_index.i == MOS_BIG_PAGE_INDEX.i + 3 && !(paging_flags & PagingFlags_ROMIO)) {
            ImGui::SameLine();
            ImGui::Text(" + I/O (%s)", paging_flags & PagingFlags_IFJ ? "IFJ" : "XFJ");
        }
    }
};

std::unique_ptr<SettingsUI> CreatePagingDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<PagingDebugWindow>(beeb_window, ImVec2(475, 400));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BreakpointsDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        bool changed = false;

        if (m_beeb_debug_state) {
            if (m_beeb_debug_state->breakpoints_changed_counter != m_breakpoints_change_counter) {
                //m->DebugGetDebugFlags(m_host_address_debug_flags, m_parasite_address_debug_flags, &m_big_page_debug_flags[0][0]);

                static_assert(sizeof m_host_address_debug_flags == sizeof m_beeb_debug_state->host_address_debug_flags);
                memcpy(m_host_address_debug_flags, m_beeb_debug_state->host_address_debug_flags, sizeof m_host_address_debug_flags);

                static_assert(sizeof m_parasite_address_debug_flags == sizeof m_beeb_debug_state->parasite_address_debug_flags);
                memcpy(m_parasite_address_debug_flags, m_beeb_debug_state->parasite_address_debug_flags, sizeof m_parasite_address_debug_flags);

                static_assert(sizeof m_big_page_debug_flags == sizeof m_beeb_debug_state->big_pages_byte_debug_flags);
                memcpy(m_big_page_debug_flags, m_beeb_debug_state->big_pages_byte_debug_flags, sizeof m_big_page_debug_flags);

                m_breakpoints_change_counter = m_beeb_debug_state->breakpoints_changed_counter;
                ++m_num_updates;
                changed = true;
            }
        }

        if (changed) {
            this->Update();
        }

#if BUILD_TYPE_Debug
        ImGui::Text("size=%zu", sizeof *this);
        ImGui::Text("%" PRIu64 " update(s)", m_num_updates);

        if (ImGui::Button("Populate")) {
            for (size_t i = 0; i < 100; ++i) {
                M6502Word addr = {(uint16_t)i};
                m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetAddressDebugFlags>(addr,
                                                                                            0,
                                                                                            (uint8_t)BBCMicroByteDebugFlag_BreakExecute));
            }
        }
#endif

        int num_rows = (int)m_breakpoints.size();
        float line_height = ImGui::GetTextLineHeight();
        {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove);

            ImGui::Columns(2, "breakpoints_columns");

            ImGuiListClipper clipper;
            clipper.Begin(num_rows, line_height);
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    Breakpoint *bp = &m_breakpoints[(size_t)i];

                    if (bp->big_page.i == HOST_ADDRESS_BREAKPOINT_BIG_PAGE) {
                        if (uint8_t *flags = this->Row(bp, "$%04x Host", bp->offset)) {

                            M6502Word addr = {bp->offset};
                            m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetAddressDebugFlags>(addr, 0, *flags));
                        }
                    } else if (bp->big_page.i == PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE) {
                        if (uint8_t *flags = this->Row(bp, "$%04x Parasite", bp->offset)) {

                            M6502Word addr = {bp->offset};
                            m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetAddressDebugFlags>(addr, BBCMicroDebugStateOverride_Parasite, *flags));
                        }
                    } else {
                        // Byte breakpoint
                        //                ASSERT(bp->big_page<BBCMicro::NUM_BIG_PAGES);
                        //                ASSERT(bp->offset<BIG_PAGE_SIZE_BYTES);
                        //                uint8_t *flags=&m_big_page_debug_flags[bp->big_page][bp->offset];

                        const BigPageMetadata *metadata = &m_beeb_state->type->big_pages_metadata[bp->big_page.i];

                        if (uint8_t *flags = this->Row(bp,
                                                       "$%04x%c%s",
                                                       metadata->addr + bp->offset,
                                                       ADDRESS_SUFFIX_SEPARATOR,
                                                       metadata->aligned_codes)) {
                            m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetByteDebugFlags>(bp->big_page,
                                                                                                     bp->offset,
                                                                                                     *flags));
                        }
                    }
                }
            }

            ImGui::EndChild();
        }
    }

  private:
    static constexpr BigPageIndex::Type HOST_ADDRESS_BREAKPOINT_BIG_PAGE = NUM_BIG_PAGES + 0;
    static_assert(HOST_ADDRESS_BREAKPOINT_BIG_PAGE >= NUM_BIG_PAGES); //overflow check

    static constexpr BigPageIndex::Type PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE = NUM_BIG_PAGES + 1;
    static_assert(PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE >= NUM_BIG_PAGES); //overflow check

    struct Breakpoint {
        BigPageIndex big_page;
        uint16_t offset;
    };
    static_assert(sizeof(Breakpoint) == 4, "");

    uint64_t m_breakpoints_change_counter = 0;
    uint64_t m_num_updates = 0;

    // This is quite a large object, by BBC standards at least.
    uint8_t m_host_address_debug_flags[65536] = {};
    uint8_t m_parasite_address_debug_flags[65536] = {};
    uint8_t m_big_page_debug_flags[NUM_BIG_PAGES][BIG_PAGE_SIZE_BYTES] = {};

    // The retain flag indicates that the byte should be listed even if there's
    // no breakpoint set. This prevents rows disappearing when you untick all
    // the entries.
    uint8_t m_host_address_debug_flags_retain[65536 >> 3] = {};
    uint8_t m_parasite_address_debug_flags_retain[65536 >> 3] = {};
    uint8_t m_big_page_debug_flags_retain[NUM_BIG_PAGES][BIG_PAGE_SIZE_BYTES >> 3] = {};

    std::vector<Breakpoint> m_breakpoints;

    uint8_t *PRINTF_LIKE(3, 4) Row(Breakpoint *bp, const char *fmt, ...) {
        ImGuiIDPusher pusher(bp);

        bool changed = false;

        va_list v;
        va_start(v, fmt);
        ImGui::TextV(fmt, v);
        va_end(v);

        ImGui::NextColumn();

        uint8_t *flags, *retain;
        if (bp->big_page.i == HOST_ADDRESS_BREAKPOINT_BIG_PAGE) {
            flags = &m_host_address_debug_flags[bp->offset];
            retain = &m_host_address_debug_flags_retain[bp->offset >> 3];
        } else if (bp->big_page.i == PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE) {
            flags = &m_parasite_address_debug_flags[bp->offset];
            retain = &m_parasite_address_debug_flags_retain[bp->offset >> 3];
        } else {
            ASSERT(bp->big_page.i < NUM_BIG_PAGES);
            ASSERT(bp->offset < BIG_PAGE_SIZE_BYTES);
            flags = &m_big_page_debug_flags[bp->big_page.i][bp->offset];
            retain = &m_big_page_debug_flags_retain[bp->big_page.i][bp->offset >> 3];
        }

        if (ImGuiCheckboxFlags("Read", flags, BBCMicroByteDebugFlag_BreakRead)) {
            changed = true;
        }

        ImGui::SameLine();

        if (ImGuiCheckboxFlags("Write", flags, BBCMicroByteDebugFlag_BreakWrite)) {
            changed = true;
        }

        ImGui::SameLine();

        if (ImGuiCheckboxFlags("Execute", flags, BBCMicroByteDebugFlag_BreakExecute)) {
            changed = true;
        }

        if (changed) {
            // Set the retain flag. It's a bit weird to have a row disappear
            // after manipulating it via this UI.
            *retain |= 1 << (bp->offset & 7);
        }

        if (*flags == 0) {
            // Allow explicit row removal.
            ImGui::SameLine();

            if (ImGui::Button("x")) {
                *retain &= ~(1 << (bp->offset & 7));

                // and force an update.
                m_breakpoints_change_counter = 0;
            }
        }

        ImGui::NextColumn();

        if (changed) {
            return flags;
        } else {
            return nullptr;
        }
    }

    //    static inline bool BreakpointLessThanByAddress(const Breakpoint &a,
    //                                                   const Breakpoint &b)
    //    {
    //        uint32_t a_value=(uint32_t)a.big_page<<16|a.offset;
    //        uint32_t b_value=(uint32_t)b.big_page<<16|b.offset;
    //
    //        return a_value<b_value;
    //    }

    void Update() {
        m_breakpoints.clear();

        UpdateAddressBreakpoints(m_host_address_debug_flags, m_host_address_debug_flags_retain, HOST_ADDRESS_BREAKPOINT_BIG_PAGE);
        UpdateAddressBreakpoints(m_parasite_address_debug_flags, m_parasite_address_debug_flags_retain, PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE);

        for (BigPageIndex::Type i = 0; i < NUM_BIG_PAGES; ++i) {
            const uint8_t *big_page_debug_flags = m_big_page_debug_flags[i];

            for (size_t j = 0; j < BIG_PAGE_SIZE_BYTES; ++j) {
                if (big_page_debug_flags[j] || m_big_page_debug_flags_retain[i][j >> 3] & 1 << (j & 7)) {
                    m_breakpoints.push_back({{i}, (uint16_t)j});
                }
            }
        }
    }

    void UpdateAddressBreakpoints(uint8_t *address_debug_flags, uint8_t *address_debug_flags_retain, BigPageIndex::Type big_page_index) {
        for (size_t i = 0; i < 65536; ++i) {
            if (address_debug_flags[i] != 0 || address_debug_flags_retain[i >> 3] & 1 << (i & 7)) {
                m_breakpoints.push_back({{big_page_index}, (uint16_t)i});
            }
        }
    }
};

std::unique_ptr<SettingsUI> CreateBreakpointsDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<BreakpointsDebugWindow>(beeb_window, ImVec2(450, 450));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if VIDEO_TRACK_METADATA
class PixelMetadataUI : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        if (const VideoDataUnit *unit = m_beeb_window->GetVideoDataUnitForMousePixel()) {
            if (unit->metadata.flags & VideoDataUnitMetadataFlag_HasAddress) {
                // The debug stuff is oriented around the CPU's view of memory,
                // but the video unit's address is from the CRTC's perspective.

                M6502Word crtc_addr = {unit->metadata.address};

                const BigPageMetadata *metadata = &m_beeb_state->type->big_pages_metadata[crtc_addr.p.p];

                m_dso &= metadata->dso_mask;
                m_dso |= metadata->dso_value;

                M6502Word cpu_addr = {(uint16_t)(metadata->addr + crtc_addr.p.o)};

                ImGui::Text("Address: $%04x%c%s", cpu_addr.w, ADDRESS_SUFFIX_SEPARATOR, metadata->minimal_codes);
                ImGui::Text("CRTC Address: $%04x", unit->metadata.crtc_address);

                const DebugBigPage *cpu_dbp = this->GetDebugBigPageForAddress(cpu_addr, false);
                this->DoBytePopupGui(cpu_dbp, cpu_addr);
            } else {
                ImGui::TextUnformatted("Address:");
            }

            if (unit->metadata.flags & VideoDataUnitMetadataFlag_HasValue) {
                uint8_t x = unit->metadata.value;

                char str[4];
                if (x >= 32 && x < 127) {
                    str[0] = '\'';
                    str[1] = (char)x;
                    str[2] = '\'';
                } else {
                    str[2] = str[1] = str[0] = '-';
                }
                str[3] = 0;

                ImGui::Text("Value: %s %-3u ($%02x) (%%%s)", str, x, x, BINARY_BYTE_STRINGS[x]);
            } else {
                ImGui::TextUnformatted("Value:");
            }

            ImGui::Text("%s cycle", unit->metadata.flags & VideoDataUnitMetadataFlag_OddCycle ? "Odd" : "Even");

            ImGui::Text("6845:%s%s%s",
                        unit->metadata.flags & VideoDataUnitMetadataFlag_6845DISPEN ? " DISPEN" : "",
                        unit->metadata.flags & VideoDataUnitMetadataFlag_6845CUDISP ? " CUDISP" : "",
                        unit->metadata.flags & VideoDataUnitMetadataFlag_6845Raster0 ? " Raster0" : "");
        }
    }

  private:
};
#endif

#if VIDEO_TRACK_METADATA
std::unique_ptr<SettingsUI> CreatePixelMetadataDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<PixelMetadataUI>(beeb_window, ImVec2(250, 120));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class StackDebugWindow : public DebugUI {
  public:
    StackDebugWindow() {
        this->SetDefaultSize(ImVec2(430, 475));
    }

  protected:
    void DoImGui2() override {
        if (this->IsStateUnavailableImGui()) {
            return;
        }

        this->DoDebugPageOverrideImGui();

        const M6502 *cpu = m_beeb_state->DebugGetM6502(m_dso);
        uint8_t s = cpu->s.b.l;

        const DebugBigPage *value_dbp = this->GetDebugBigPageForAddress({0}, false);

        ImGui::BeginTable("stack_table", 9, ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY);

        ImGui::TableSetupColumn("S", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("Int", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("Byte", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("Char", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("Hex", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("Bin", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("rts", ImGuiTableColumnFlags_WidthFixed, 0.f);
        ImGui::TableSetupColumn("jsr", ImGuiTableColumnFlags_WidthFixed, 0.f);

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        {
            ImVec4 disabled_colour = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            ImGuiStyleColourPusher colour_pusher;
            bool in_active_stack = true;

            for (int offset = 255; offset >= 0; --offset) {
                ImGui::TableNextRow();

                if (offset == s) {
                    ImGui::Separator();
                    colour_pusher.Push(ImGuiCol_Text, disabled_colour);
                    in_active_stack = false;
                }

                M6502Word value_addr = {(uint16_t)(0x100 + offset)};
                uint8_t value = value_dbp->bp.r[value_addr.w];

                ImGui::TableNextColumn();
                ImGui::Text("$%04x", value_addr.w);
                this->DoBytePopupGui(value_dbp, value_addr);

                ImGui::TableNextColumn();
                ImGui::Text("%4d", (int8_t)value);

                ImGui::TableNextColumn();
                ImGui::Text("%3u", value);

                ImGui::TableNextColumn();
                if (value >= 32 && value < 127) {
                    ImGui::Text("%c", (char)value);
                }

                ImGui::TableNextColumn();
                ImGui::Text("$%02x", value);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(BINARY_BYTE_STRINGS[value]);

                M6502Word addr;
                addr.b.l = value;
                addr.b.h = value_dbp->bp.r[0x100 + ((offset + 1) & 0xff)];

                ImGuiIDPusher pusher(offset);

                {
                    bool was_jsr = false;
                    ImGuiStyleColourPusher colour_pusher2;

                    if (in_active_stack) {
                        // Does this look like it was probably pushed by a jsr?
                        //
                        // (Paging can interfere with this! The paging overrides UI is
                        // available if you need it.)
                        uint8_t possible_jsr;
                        if (this->ReadByte(&possible_jsr, nullptr, nullptr, addr.w - 2, false)) {
                            if (possible_jsr == 0x20) {
                                was_jsr = true;
                            }
                        }
                    }

                    ImGui::TableNextColumn();
                    this->AddressColumn("Address", addr, 0);

                    if (!was_jsr) {
                        colour_pusher2.Push(ImGuiCol_Text, disabled_colour);
                    }

                    ImGui::TableNextColumn();
                    this->AddressColumn("Return address", addr, 1);

                    ImGui::TableNextColumn();
                    this->AddressColumn("Call address", addr, -2);
                }
            }
        }

        ImGui::EndTable();
    }

  private:
    static const char ADDR_CONTEXT_POPUP_NAME[];

    void AddressColumn(const char *header, M6502Word addr, int delta) {
        ImGuiIDPusher id_pusher(delta);

        uint16_t actual_addr = (uint16_t)(addr.w + delta);

        ImGui::Text("$%04x", actual_addr);

        if (ImGui::IsMouseClicked(1)) {
            if (ImGui::IsItemHovered()) {
                ImGui::OpenPopup(ADDR_CONTEXT_POPUP_NAME);
            }
        }

        if (ImGui::BeginPopup(ADDR_CONTEXT_POPUP_NAME)) {
            {
                ImGuiStyleColourPusher colour_pusher;
                colour_pusher.PushDefault(ImGuiCol_Text);

                ImGuiHeader(header);

                const DebugBigPage *dbp = this->GetDebugBigPageForAddress({actual_addr}, false);
                this->DoByteDebugGui(dbp, {actual_addr});
            }
            ImGui::EndPopup();
        }
    }
};

const char StackDebugWindow::ADDR_CONTEXT_POPUP_NAME[] = "stack_addr_context_popup";

std::unique_ptr<SettingsUI> CreateHostStackDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<StackDebugWindow>(beeb_window);
}

std::unique_ptr<SettingsUI> CreateParasiteStackDebugWindow(BeebWindow *beeb_window) {
    return CreateParasiteDebugUI<StackDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TubeDebugWindow : public DebugUI {
  public:
    TubeDebugWindow() {
        this->SetDefaultSize(ImVec2(535, 450));

        for (uint8_t i = 0; i < TUBE_FIFO1_SIZE_BYTES; ++i) {
            snprintf(m_fifo1_header + i * 3, 4, "%-3u", i);
        }
    }

  protected:
    void DoImGui2() override {
        const Tube *tube = m_beeb_state->DebugGetTube();
        if (!tube) {
            ImGui::Text("No Tube");
            return;
        }

        ImGui::Checkbox("Sort FIFOs by index", &m_sort_by_index);

        ImGuiHeader("Tube Status");

        ImGui::BulletText("Parasite: IRQ=%d NMI=%d", tube->pirq.bits.pirq, tube->pirq.bits.pnmi);
        ImGui::BulletText("Host: IRQ=%d", tube->hirq.bits.hirq);
        ImGui::Text("Status: $%02x (%%%s) (T=%d P=%d V=%d M=%d J=%d I=%d Q=%d)",
                    tube->status.value,
                    BINARY_BYTE_STRINGS[tube->status.value],
                    tube->status.bits.t,
                    tube->status.bits.p,
                    tube->status.bits.v,
                    tube->status.bits.m,
                    tube->status.bits.j,
                    tube->status.bits.i,
                    tube->status.bits.q);
        ImGui::BulletText("FIFO1: PIRQ=%s", BOOL_STR(tube->status.bits.i));
        ImGui::BulletText("FIFO3: PNMI=%s", BOOL_STR(tube->status.bits.m));
        ImGui::BulletText("FIFO4: PIRQ=%s HIRQ=%s", BOOL_STR(tube->status.bits.j), BOOL_STR(tube->status.bits.q));

        if (m_sort_by_index) {
            this->DoFIFO1P2HGui(tube);
            this->DoFIFO1H2PGui(tube);
            this->DoFIFO2P2HGui(tube);
            this->DoFIFO2H2PGui(tube);
            this->DoFIFO3P2HGui(tube);
            this->DoFIFO3H2PGui(tube);
            this->DoFIFO4P2HGui(tube);
            this->DoFIFO4H2PGui(tube);
        } else {
            this->DoFIFO1P2HGui(tube);
            this->DoFIFO2P2HGui(tube);
            this->DoFIFO3P2HGui(tube);
            this->DoFIFO4P2HGui(tube);
            this->DoFIFO1H2PGui(tube);
            this->DoFIFO2H2PGui(tube);
            this->DoFIFO3H2PGui(tube);
            this->DoFIFO4H2PGui(tube);
        }
    }

  private:
    static constexpr size_t FIFO1_STRING_SIZE = TUBE_FIFO1_SIZE_BYTES * 3 + 1;
    char m_fifo1_header[FIFO1_STRING_SIZE];
    bool m_sort_by_index = true;

    void DoFIFO1P2HGui(const Tube *t) {
        this->Header(1, true);

        this->DoStatusImGui(1, true, t->pstatus1, t->hstatus1);

        char hex_buffer[FIFO1_STRING_SIZE], *hex = hex_buffer;
        char ascii_buffer[FIFO1_STRING_SIZE], *ascii = ascii_buffer;

        for (unsigned i = 0; i < t->p2h1_n; ++i) {
            uint8_t byte = t->p2h1[(t->p2h1_rindex + i) % TUBE_FIFO1_SIZE_BYTES];

            *hex++ = HEX_CHARS_LC[byte >> 4];
            *hex++ = HEX_CHARS_LC[byte & 0xf];
            *hex++ = ' ';

            *ascii++ = byte >= 32 && byte < 127 ? (char)byte : ' ';
            *ascii++ = ' ';
            *ascii++ = ' ';
        }

        ASSERT(hex <= hex_buffer + sizeof hex_buffer);
        *hex = 0;

        ASSERT(ascii <= ascii_buffer + sizeof ascii_buffer);
        *ascii = 0;

        ImGui::TextUnformatted(m_fifo1_header);
        ImGui::TextUnformatted(hex_buffer);
        ImGui::TextUnformatted(ascii_buffer);
    }

    void DoFIFO1H2PGui(const Tube *t) {
        this->Header(1, false);

        this->DoStatusImGui(1, false, t->pstatus1, t->hstatus1);

        this->DoLatchImGui(t->h2p1, t->pstatus1.bits.available);
    }

    void DoFIFO2P2HGui(const Tube *t) {
        this->Header(2, true);

        this->DoStatusImGui(2, true, t->pstatus2, t->hstatus2);

        this->DoLatchImGui(t->p2h2, t->hstatus2.bits.available);
    }

    void DoFIFO2H2PGui(const Tube *t) {
        this->Header(2, false);

        this->DoStatusImGui(2, false, t->pstatus2, t->hstatus2);

        this->DoLatchImGui(t->h2p2, t->pstatus2.bits.available);
    }

    void DoFIFO3P2HGui(const Tube *t) {
        this->Header(3, true);

        this->DoStatusImGui(3, true, t->pstatus3, t->hstatus3);

        this->DoFIFO3ImGui(t->status.bits.v, t->p2h3, t->p2h3_n);
    }

    void DoFIFO3H2PGui(const Tube *t) {
        this->Header(3, false);

        this->DoStatusImGui(3, false, t->pstatus3, t->hstatus3);

        this->DoFIFO3ImGui(t->status.bits.v, t->h2p3, t->h2p3_n);
    }

    void DoFIFO4P2HGui(const Tube *t) {
        this->Header(4, true);

        this->DoStatusImGui(4, true, t->pstatus4, t->hstatus4);

        this->DoLatchImGui(t->p2h4, t->hstatus4.bits.available);
    }

    void DoFIFO4H2PGui(const Tube *t) {
        this->Header(4, false);

        this->DoStatusImGui(4, false, t->pstatus4, t->hstatus4);

        this->DoLatchImGui(t->h2p4, t->pstatus4.bits.available);
    }

    void DoLatchImGui(uint8_t value, bool available) {
        if (available) {
            this->DoDataImGui("Data", value);
        } else {
            ImGui::Text("Data: *empty*");
        }
    }

    void DoFIFO3ImGui(bool two_bytes, const uint8_t *values, uint8_t n) {
        const char *data1_empty = two_bytes ? "*empty*" : "*N/A*";

        if (n == 0) {
            ImGui::Text("Data 0: *empty*");
            ImGui::Text("Data 1: %s", data1_empty);
        } else if (n == 1) {
            this->DoDataImGui("Data 0", values[0]);
            ImGui::Text("Data 1: %s", data1_empty);
        } else {
            this->DoDataImGui("Data 0", values[0]);
            this->DoDataImGui("Data 1", values[1]);
        }
    }

    void DoDataImGui(const char *prefix, uint8_t value) {
        char ch[5];
        if (value >= 32 && value <= 126) {
            snprintf(ch, sizeof ch, " '%c'", value);
        } else {
            ch[0] = 0;
        }

        ImGui::Text("%s: %3d %3uu%s ($%02x) (%%%s)", prefix, (int8_t)value, value, ch, value, BINARY_BYTE_STRINGS[value]);
    }

    void Header(int fifo, bool parasite) {
        char str[100];
        snprintf(str, sizeof str, "FIFO %d %s->%s", fifo, this->GetName(parasite), this->GetName(!parasite));
        ImGuiHeader(str);
    }

    const char *GetName(bool parasite) const {
        return parasite ? "Parasite" : "Host";
    }

    void DoStatusImGui(int fifo, bool parasite, TubeFIFOStatus pstatus, TubeFIFOStatus hstatus) {
        bool not_full, available;
        if (parasite) {
            not_full = pstatus.bits.not_full;
            available = hstatus.bits.available;
        } else {
            not_full = hstatus.bits.not_full;
            available = pstatus.bits.available;
        }

        const char *here = this->GetName(parasite), *there = this->GetName(!parasite);
        ImGui::BulletText("FIFO %d %s->%s Full: %s", fifo, here, there, BOOL_STR(!not_full));
        ImGui::BulletText("FIFO %d %s->%s Data: %s", fifo, here, there, BOOL_STR(available));
    }
};

std::unique_ptr<SettingsUI> CreateTubeDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<TubeDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ADCDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        const ADC *adc = m_beeb_state->DebugGetADC();
        if (!adc) {
            ImGui::Text("No ADC");
            return;
        }

        ImGui::Text("ADC address: $%04x", m_beeb_state->type->adc_addr);

        ImGuiHeader("\"Analogue\" values");
        for (int ch = 0; ch < 4; ++ch) {
            uint16_t avalue = m_beeb_state->analogue_channel_values[ch];
            ImGui::BulletText("%d: %-4u $%03x %.5f", ch, avalue, avalue, avalue / 65535.);
        }

        ImGuiHeader("Status");
        ImGui::Text("Status: %3u $%02x %%%s", adc->m_status.value, adc->m_status.value, BINARY_BYTE_STRINGS[adc->m_status.value]);
        ImGui::Text("Conversion time left: %d " MICROSECONDS_UTF8, adc->m_timer);
        ImGui::BulletText("Sampled value: %u", adc->m_avalue);
        ImGui::BulletText("Channel: %u", adc->m_status.bits.channel);
        ImGui::BulletText("Flag: %u", adc->m_status.bits.flag);
        ImGui::BulletText("Precision: %d bits", adc->m_status.bits.prec_10_bit ? 10 : 8);
        ImGui::BulletText("MSB: %u", adc->m_status.bits.msb);
        ImGui::BulletText("Busy: %s", BOOL_STR(!adc->m_status.bits.not_busy));
        ImGui::BulletText("EOC: %s", BOOL_STR(!adc->m_status.bits.not_eoc));
    }

  private:
};

std::unique_ptr<SettingsUI> CreateADCDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<ADCDebugWindow>(beeb_window, ImVec2(225, 315));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DigitalJoystickDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        int adji_dip = m_beeb_state->DebugGetADJIDIPSwitches();
        if (adji_dip >= 0) {
            ImGuiHeader("Retro Hardware ADJI cartridge");
            ImGui::Text("Address: $%04x", BBCMicro::ADJI_ADDRESSES[adji_dip & 3]);
            this->State();
        } else if (m_beeb_state->type->type_id == BBCMicroTypeID_MasterCompact) {
            ImGuiHeader("Master Compact digital joystick");
            this->State();
        } else {
            ImGui::Text("No digital joystick");
        }
    }

  private:
    void Checkbox(const char *label, bool value) {
        ImGui::Checkbox(label, &value);
    }

    void State() {
        BBCMicroState::DigitalJoystickInputBits bits = m_beeb_state->digital_joystick_state.bits;

        this->Checkbox("Up", bits.up);
        this->Checkbox("Down", bits.down);
        this->Checkbox("Left", bits.left);
        this->Checkbox("Right", bits.right);
        this->Checkbox("Fire 1", bits.fire0);
        this->Checkbox("Fire 2", bits.fire1);
    }
};

std::unique_ptr<SettingsUI> CreateDigitalJoystickDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<DigitalJoystickDebugWindow>(beeb_window, ImVec2(250, 200));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class KeyboardDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        ImGuiHeader("Keyboard State");
        ImGui::Text("Auto scan: %s", BOOL_STR(m_beeb_state->addressable_latch.bits.not_kb_write));
        ImGui::Text("Auto scan column: %u (0x%x)", m_beeb_state->key_scan_column, m_beeb_state->key_scan_column);
        ImGuiHeader("Keyboard Matrix");
        ImGui::TextUnformatted("    | 0 1 2 3 4 5 6 7 8 9 A B C D E F");
        ImGui::TextUnformatted("----+--------------------------------");

        char text[100];
        for (uint8_t row = 0; row < 8; ++row) {
            char *p = text;
            *p++ = '$';
            *p++ = (char)('0' + row);
            *p++ = 'x';
            *p++ = ' ';
            *p++ = '|';

            for (uint8_t col = 0; col < 16; ++col) {
                *p++ = ' ';
                *p++ = m_beeb_state->key_columns[col] & 1 << row ? '*' : '.';
            }
            *p++ = 0;
            ImGui::TextUnformatted(text);
        }

        ImGuiHeader("Keys Pressed");
        bool any = false;
        for (uint8_t row = 1; row < 8; ++row) {
            for (uint8_t col = 0; col < 16; ++col) {
                if (m_beeb_state->key_columns[col] & 1 << row) {
                    uint8_t code = row << 4 | col;
                    ImGui::BulletText("$%02x: %s", code, GetBeebKeyEnumName(code));
                    any = true;
                }
            }
        }
        if (!any) {
            ImGui::TextUnformatted("(None)");
        }
    }

  private:
};

std::unique_ptr<SettingsUI> CreateKeyboardDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<KeyboardDebugWindow>(beeb_window, ImVec2(280, 450));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MouseDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        if (!m_beeb_thread->HasMouse()) {
            ImGui::TextUnformatted("Mouse not enabled");
        } else {
            uint8_t buttons = m_beeb_state->DebugGetMouseButtons();

            ImGuiHeader("Mouse State");

            ImGuiCheckboxFlags("Left", &buttons, BBCMicroMouseButton_Left);
            ImGuiCheckboxFlags("Middle", &buttons, BBCMicroMouseButton_Middle);
            ImGuiCheckboxFlags("Right", &buttons, BBCMicroMouseButton_Right);

            ImGui::Text("X Delta: %d", m_beeb_state->mouse_dx);
            ImGui::Text("Y Delta: %d", m_beeb_state->mouse_dy);

            ImGuiHeader("Debug Mouse \"Control\"");
            this->MouseMotion("Left", -1, 0);
            this->MouseMotion("Right", 1, 0);
            this->MouseMotion("Up", 0, -1);
            this->MouseMotion("Down", 0, 1);
        }
    }

  private:
    void MouseMotion(const char *name, int dx, int dy) {
        if (ImGui::ButtonEx(name, ImVec2(0, 0), ImGuiButtonFlags_Repeat)) {
            m_beeb_thread->Send(std::make_shared<BeebThread::MouseMotionMessage>(dx, dy));
        }
    }
};

std::unique_ptr<SettingsUI> CreateMouseDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<MouseDebugWindow>(beeb_window, ImVec2(200, 300));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class WD1770DebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        if (!m_beeb_state->disc_interface) {
            ImGui::TextUnformatted("No disk interface");
            return;
        }

        const WD1770 *fdc;
        DiscInterfaceControl control;
        if (!m_beeb_state->DebugGetWD1770(&fdc, &control)) {
            ImGui::Text("Not 1770 interface: %s", m_beeb_state->disc_interface->display_name.c_str());
            return;
        }

        ImGui::Text("Disk interface: %s", m_beeb_state->disc_interface->display_name.c_str());
        ImGui::Text("FDC type: %s", fdc->m_is1772 ? "1772" : "1770");
        //if (ImGui::CollapsingHeader("Register Values")) {
        //    Register("Status ", fdc->m_status.value);
        //    Register("Command", fdc->m_command.value);
        //    Register("Track  ", fdc->m_track);
        //    Register("Sector ", fdc->m_sector);
        //    Register("Data   ", fdc->m_data);

        //    ImGui::Separator();
        //}

        bool type1 = !(fdc->m_command.value & 0x80);

        Register("Track  ", fdc->m_track);
        Register("Sector ", fdc->m_sector);
        Register("Data   ", fdc->m_data);

        ImGuiHeader("Status Register");
        Register("Status ", fdc->m_status.value);
        ImGui::BulletText("Motor On: %d", fdc->m_status.bits.motor_on);
        ImGui::BulletText("Write Protect: %d", fdc->m_status.bits.write_protect);
        ImGui::BulletText("%s: %d", type1 ? "Spin-up" : "Record Type", fdc->m_status.bits.deleted_or_spinup);
        ImGui::BulletText("Record Not Found: %d", fdc->m_status.bits.rnf);
        ImGui::BulletText("CRC Error: %d", fdc->m_status.bits.crc_error);
        ImGui::BulletText("%s: %d", type1 ? "Track 00" : "Lost Data", fdc->m_status.bits.lost_or_track0);
        ImGui::BulletText("%s: %d", type1 ? "Index" : "Data Request", fdc->m_status.bits.drq_or_idx);
        ImGui::BulletText("Busy: %d", fdc->m_status.bits.busy);

        ImGuiHeader("Command");
        Register("Command", fdc->m_command.value);
        if (type1) {
            bool step = false;
            const char *name;
            if ((fdc->m_command.value & 0xe0) == 0) {
                name = fdc->m_command.value & 0x10 ? "Seek" : "Restore";
                step = false;
            } else {
                static const char *NAMES[] = {nullptr, "Step", "Step In", "Step Out"};
                name = NAMES[fdc->m_command.bits_step.cmd];
                ASSERT(name);
                step = true;
            }

            CommandName(name);
            if (step) {
                ImGui::BulletText("Update Track Register: %s", BOOL_STR(fdc->m_command.bits_step.u));
            }

            HBit(fdc->m_command.bits_i.h);
            ImGui::BulletText("Verify: %u", fdc->m_command.bits_i.v);

            const int *step_rates = fdc->m_is1772 ? WD1770::STEP_RATES_MS_1772 : WD1770::STEP_RATES_MS_1770;
            ImGui::BulletText("Step Rate: %d ms", step_rates[fdc->m_command.bits_i.r]);
        } else if ((fdc->m_command.value & 0xc0) == 0x80) {
            // Type II
            ImGui::BulletText("Command: %s", fdc->m_command.value & 0x20 ? "Write Sector" : "Read Sector");
            ImGui::BulletText("Multiple Sectors: %s", BOOL_STR(fdc->m_command.bits_ii.m));
            HBit(fdc->m_command.bits_ii.h);
            EBit(fdc->m_command.bits_ii.e);
            PBit(fdc->m_command.bits_ii.p);
            ImGui::BulletText("Write Deleted Data: %s", BOOL_STR(fdc->m_command.bits_ii.a0));
        } else if ((fdc->m_command.value & 0xf0) == 0xd0) {
            // Type IV
            ImGui::BulletText("Command: Reset");
            ImGui::BulletText("Index Pulse: %s", BOOL_STR(fdc->m_command.bits_iv.index));
            ImGui::BulletText("Immediate: %s", BOOL_STR(fdc->m_command.bits_iv.immediate));
            ImGui::BulletText("No Interrupt: %s", BOOL_STR((fdc->m_command.value & 0x0f) == 0x00));
        } else {
            // Type III
            ImGui::BulletText("Command: %s", fdc->m_command.value & 0x10 ? "Write Track" : "Read Track");
            HBit(fdc->m_command.bits_iii.h);
            EBit(fdc->m_command.bits_iii.e);
            PBit(fdc->m_command.bits_iii.p);
        }

        ImGuiHeader("Control");
        ImGui::BulletText("Drive: %d", control.drive);
        ImGui::BulletText("Density: %s", control.dden ? "Double" : "Single");
        ImGui::BulletText("Side: %d", control.side);
        ImGui::BulletText("Reset: %d", control.reset);
    }

  private:
    static void PBit(uint8_t p) {
        ImGui::BulletText("Write Precompensation: %s", BOOL_STR(p));
    }
    static void EBit(uint8_t e) {
        ImGui::BulletText("Settling Delay: %s", BOOL_STR(e));
    }

    static void HBit(uint8_t h) {
        ImGui::BulletText("Spin Up: %s", BOOL_STR(!h));
    }

    static void CommandName(const char *name) {
        ImGui::BulletText("Command: %s", name);
    }

    static void Register(const char *name, uint8_t value) {
        ImGui::Text("%s: $%02x %-3d %%%s", name, value, value, BINARY_BYTE_STRINGS[value]);
    }
};

std::unique_ptr<SettingsUI> CreateWD1770DebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<WD1770DebugWindow>(beeb_window, ImVec2(300, 300));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#else

std::unique_ptr<SettingsUI> CreateSystemDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateHost6502DebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateParasite6502DebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateHostMemoryDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateParasiteMemoryDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateExtMemoryDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateHostDisassemblyDebugWindow(BeebWindow *, bool) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateParasiteDisassemblyDebugWindow(BeebWindow *, bool) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateVideoULADebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateSystemVIADebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateUserVIADebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateSN76489DebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreatePagingDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateBreakpointsDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreatePixelMetadataDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateHostStackDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateParasiteStackDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateTubeDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateADCDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateDigitalJoystickDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateKeyboardDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateMouseDebugWindow(BeebWindow *) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateWD1770DebugWindow(BeebWindow *) {
    return nullptr;
}

#endif
