#include <shared/system.h>
#include "debugger.h"

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
                         const BBCMicroType *type,
                         const char *text) {
    uint32_t dso = 0;
    uint16_t addr;

    // Hmm, not really sure about this.
    uint32_t dso_fixed_bits_mask = BBCMicroDebugStateOverride_Parasite;
    uint32_t dso_fixed_bits = *dso_ptr & dso_fixed_bits_mask;

    const char *sep = strchr(text, ADDRESS_PREFIX_SEPARATOR);
    if (sep) {
        if (!ParseAddressPrefix(&dso, type, text, sep, nullptr)) {
            return false;
        }

        text = sep + 1;
    }

    if (!GetUInt16FromString(&addr, text)) {
        return false;
    }

    *addr_ptr = addr;
    *dso_ptr = (dso & ~dso_fixed_bits_mask) | dso_fixed_bits;

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class RevealTargetUI;

class DebugUI : public SettingsUI {
  public:
    struct DebugBigPage {
        // The big page this refers to.
        const BigPageMetadata *metadata = nullptr;

        // points to this->ram_buffer, or NULL.
        const uint8_t *r = nullptr;

        // set if writeable - use one of the thread messages to actually do
        // the writing.
        bool writeable = false;

        // points to this->byte_flags_buffer, or NULL.
        const uint8_t *byte_flags = nullptr;

        // The address flags are per-address, not per big page - it's just
        // convenient to have them as part of the same struct.
        //
        // Points to this->addr_flags_buffer, or NULL.
        const uint8_t *addr_flags = nullptr;

        // buffers for the above.
        uint8_t ram_buffer[BBCMicro::BIG_PAGE_SIZE_BYTES] = {};
        uint8_t addr_flags_buffer[BBCMicro::BIG_PAGE_SIZE_BYTES] = {};
        uint8_t byte_flags_buffer[BBCMicro::BIG_PAGE_SIZE_BYTES] = {};
    };

    bool OnClose() override;

    void DoImGui() override final;

    void SetBeebWindow(BeebWindow *beeb_window);

    uint32_t GetDebugStateOverrides() const;
    void SetDebugStateOverrides(uint32_t dso);

  protected:
    BeebWindow *m_beeb_window = nullptr;
    std::shared_ptr<BeebThread> m_beeb_thread;
    uint32_t m_dso = 0;

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

    // If required state implied by m_dso is unavailable, display a suitable
    // message.
    bool IsStateUnavailableImGui(const BBCMicro *m) const;

  private:
    std::unique_ptr<DebugBigPage> m_dbps[2][16]; // [mos][mem big page]
    uint32_t m_popup_id = 0;                     //salt for byte popup gui IDs

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
            m_dbps[i][j].reset();
        }
    }

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

    if (!dbp->r) {
        return false;
    }

    *value = dbp->r[addr.p.o];

    if (addr_flags) {
        if (dbp->addr_flags) {
            *addr_flags = dbp->addr_flags[addr.p.o];
        } else {
            *addr_flags = 0;
        }
    }

    if (byte_flags) {
        if (dbp->byte_flags) {
            *byte_flags = dbp->byte_flags[addr.p.o];
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
    static const char SHADOW_POPUP[] = "shadow_popup";
    static const char ANDY_POPUP[] = "andy_popup";
    static const char HAZEL_POPUP[] = "hazel_popup";
    static const char OS_POPUP[] = "os_popup";
    static const char PARASITE_ROM_POPUP[] = "parasite_rom_popup";

    uint32_t dso_mask;
    uint32_t dso_current;
    {
        std::unique_lock<Mutex> lock;
        const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
        dso_mask = m->DebugGetStateOverrideMask();
        dso_current = m->DebugGetCurrentPageOverride();
    }

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
                ImGui::Text("%x!", m_dso & BBCMicroDebugStateOverride_ROM);
            } else {
                ImGui::Text("%x", dso_current & BBCMicroDebugStateOverride_ROM);
            }

            if (ImGui::BeginPopup(ROM_POPUP)) {
                if (ImGui::Button("Use current")) {
                    m_dso &= ~(uint32_t)BBCMicroDebugStateOverride_OverrideROM;
                    m_dso &= ~(uint32_t)BBCMicroDebugStateOverride_ROM;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Text("Force");

                for (uint8_t i = 0; i < 16; ++i) {
                    ImGui::SameLine();

                    char text[10];
                    snprintf(text, sizeof text, "%X", i);

                    if (ImGui::Button(text)) {
                        m_dso |= BBCMicroDebugStateOverride_OverrideROM;
                        m_dso = (m_dso & ~(uint32_t)BBCMicroDebugStateOverride_ROM) | i;
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
    const char *cpu = dbp->metadata->is_parasite ? "Parasite" : "Host";

    ImGui::Text("Address: $%04x (%s)", addr.w, cpu);

    if (!dbp) {
        ImGui::Separator();
        ImGui::Text("Byte: *unknown*");
    } else {
        if (dbp->addr_flags) {
            char addr_str[50];
            snprintf(addr_str, sizeof addr_str, "$%04x (%s)", addr.w, cpu);

            uint8_t addr_flags = dbp->addr_flags[addr.p.o];
            if (this->DoDebugByteFlagsGui(addr_str, &addr_flags)) {
                uint32_t dso = dbp->metadata->is_parasite ? BBCMicroDebugStateOverride_Parasite : 0;
                m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetAddressDebugFlags>(addr, dso, addr_flags));
                //                std::unique_lock<Mutex> lock;
                //                BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);
                //                m->DebugSetAddressDebugFlags(addr,addr_flags);
                //                m_beeb_thread->InvalidateDebugBigPageForAddress(addr);
            }

            if (RevealTargetUI *reveal_target_ui = this->DoRevealAddressGui()) {
                reveal_target_ui->RevealAddress(addr);
            }
        }

        ImGui::Separator();

        char byte_str[10];
        snprintf(byte_str, sizeof byte_str, "%c%c$%04x", dbp->metadata->code, ADDRESS_PREFIX_SEPARATOR, addr.w);

        ImGui::Text("Byte: %s (%s)",
                    byte_str,
                    dbp->metadata->description.c_str());

        if (dbp->byte_flags) {
            uint8_t byte_flags = dbp->byte_flags[addr.p.o];
            if (this->DoDebugByteFlagsGui(byte_str, &byte_flags)) {
                m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetByteDebugFlags>(dbp->metadata->index,
                                                                                         (uint16_t)addr.p.o,
                                                                                         byte_flags));
                //                std::unique_lock<Mutex> lock;
                //                BBCMicro *m=m_beeb_thread->LockMutableBeeb(&lock);
                //                m->DebugSetByteDebugFlags(dbp->bp,addr.p.o,byte_flags);
                //                m_beeb_thread->InvalidateDebugBigPageForAddress(addr);
            }
        }

        if (RevealTargetUI *reveal_target_ui = this->DoRevealByteGui()) {
            reveal_target_ui->RevealByte(dbp, addr);
        }

        ImGui::Separator();

        if (dbp->r) {
            uint8_t value = dbp->r[addr.p.o];
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
    m_dso &= dbp->metadata->dso_mask;
    m_dso |= dbp->metadata->dso_value;
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

    if (!m_dbps[mos][addr.p.p]) {
        auto dbp = std::make_unique<DebugBigPage>();

        std::unique_lock<Mutex> lock;
        const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

        const BBCMicro::BigPage *bp = m->DebugGetBigPageForAddress(addr, mos, m_dso);
        dbp->metadata = bp->metadata;

        if (bp->r) {
            memcpy(dbp->ram_buffer, bp->r, BBCMicro::BIG_PAGE_SIZE_BYTES);
            dbp->r = dbp->ram_buffer;
        } else {
            dbp->r = nullptr;
        }

        if (bp->w && bp->r) {
            dbp->writeable = true;
        } else {
            dbp->writeable = false;
        }

        if (bp->byte_debug_flags) {
            ASSERT(bp->address_debug_flags);

            memcpy(dbp->byte_flags_buffer, bp->byte_debug_flags, BBCMicro::BIG_PAGE_SIZE_BYTES);
            dbp->byte_flags = dbp->byte_flags_buffer;

            memcpy(dbp->addr_flags_buffer, bp->address_debug_flags, BBCMicro::BIG_PAGE_SIZE_BYTES);
            dbp->addr_flags = dbp->addr_flags_buffer;
        } else {
            ASSERT(!bp->address_debug_flags);

            dbp->byte_flags = nullptr;
            dbp->addr_flags = nullptr;
        }

        m_dbps[mos][addr.p.p] = std::move(dbp);
    }

    return m_dbps[mos][addr.p.p].get();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DebugUI::IsStateUnavailableImGui(const BBCMicro *m) const {
    if (m_dso & BBCMicroDebugStateOverride_Parasite) {
        if (!(m->DebugGetStateOverrideMask() & BBCMicroDebugStateOverride_Parasite)) {
            ImGui::TextUnformatted("State not available");
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
        ImGui::Text("%s!", m_dso & flag_mask ? "on" : "off");
    } else {
        ImGui::Text("%s", current & flag_mask ? "on" : "off");
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
static std::unique_ptr<DerivedType> CreateDebugUI(BeebWindow *beeb_window) {
    std::unique_ptr<DerivedType> ptr = std::make_unique<DerivedType>();

    ptr->SetBeebWindow(beeb_window);

    return ptr;
}

template <class DerivedType>
static std::unique_ptr<DerivedType> CreateParasiteDebugUI(BeebWindow *beeb_window) {
    std::unique_ptr<DerivedType> ptr = CreateDebugUI<DerivedType>(beeb_window);

    uint32_t dso = ptr->GetDebugStateOverrides();
    dso |= BBCMicroDebugStateOverride_Parasite;
    ptr->SetDebugStateOverrides(dso);

    return ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class M6502DebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        //m_beeb_thread->SendUpdate6502StateMessage();

        bool halted;
        M6502State host_state, parasite_state;
        BBCMicroParasiteType parasite_type;
        const CycleCount *cycles;
        char halt_reason[1000];

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            const M6502 *host_cpu = m->GetM6502();
            this->GetStateForCPU(&host_state, host_cpu);

            parasite_type = m->GetParasiteType();

            if (parasite_type != BBCMicroParasiteType_None) {
                const M6502 *parasite_cpu = m->DebugGetM6502(BBCMicroDebugStateOverride_Parasite);
                this->GetStateForCPU(&parasite_state, parasite_cpu);
            }

            halted = m->DebugIsHalted();
            if (const char *tmp = m->DebugGetHaltReason()) {
                strlcpy(halt_reason, tmp, sizeof halt_reason);
            } else {
                halt_reason[0] = 0;
            }
            cycles = m->GetCycleCountPtr();
        }

        ImGuiHeader("System state");

        char cycles_str[MAX_UINT64_THOUSANDS_LEN];

        switch (parasite_type) {
        default:
            ASSERT(false);
            // fall through
        case BBCMicroParasiteType_None:
            break;

        case BBCMicroParasiteType_External3MHz6502:
            GetThousandsString(cycles_str, BBCMicro::Get3MHzCycleCount(*cycles));
            ImGui::Text("3 MHz Cycles = %s", cycles_str);
            break;

        case BBCMicroParasiteType_MasterTurbo:
            GetThousandsString(cycles_str, cycles->n >> RSHIFT_CYCLE_COUNT_TO_4MHZ);
            ImGui::Text("4 MHz Cycles = %s", cycles_str);
            break;
        }

        GetThousandsString(cycles_str, cycles->n >> RSHIFT_CYCLE_COUNT_TO_2MHZ);
        ImGui::Text("2 MHz Cycles = %s", cycles_str);

        if (halted) {
            if (halt_reason[0] == 0) {
                ImGui::TextUnformatted("State = halted");
            } else {
                ImGui::Text("State = halted: %s", halt_reason);
            }
        } else {
            ImGui::TextUnformatted("State = running");
        }

        ImGuiHeader("Host CPU");

        this->StateImGui(host_state);

        if (parasite_type != BBCMicroParasiteType_None) {
            ImGuiHeader("Parasite CPU");
            this->StateImGui(parasite_state);
        }
    }

  private:
    struct M6502State {
        uint16_t pc, abus;
        uint8_t a, x, y, sp, opcode, read, dbus;
        M6502P p;
        M6502Fn tfn, ifn;
        const M6502Config *config;
    };

    void StateImGui(const M6502State &state) {
        this->Reg("A", state.a);
        this->Reg("X", state.x);
        this->Reg("Y", state.y);
        ImGui::Text("PC = $%04x", state.pc);
        ImGui::Text("S = $01%02X", state.sp);
        const char *mnemonic = state.config->disassembly_info[state.opcode].mnemonic;
        const char *mode_name = M6502AddrMode_GetName(state.config->disassembly_info[state.opcode].mode);

        char pstr[9];
        ImGui::Text("P = $%02x %s", state.p.value, M6502P_GetString(pstr, state.p));

        ImGui::Text("Opcode = $%02X %03d - %s %s", state.opcode, state.opcode, mnemonic, mode_name);
        ImGui::Text("tfn = %s", GetFnName(state.tfn));
        ImGui::Text("ifn = %s", GetFnName(state.ifn));
        ImGui::Text("Address = $%04x; Data = $%02x %03d %s", state.abus, state.dbus, state.dbus, BINARY_BYTE_STRINGS[state.dbus]);
        ImGui::Text("Access = %s", M6502ReadType_GetName(state.read));
    }

    void GetStateForCPU(M6502State *state, const M6502 *cpu) {
        state->config = cpu->config;
        state->a = cpu->a;
        state->x = cpu->x;
        state->y = cpu->y;
        state->pc = cpu->opcode_pc.w;
        state->sp = cpu->s.b.l;
        state->read = cpu->read;
        state->abus = cpu->abus.w;
        state->dbus = cpu->dbus;
        state->p = M6502_GetP(cpu);
        state->tfn = cpu->tfn;
        state->ifn = cpu->ifn;
        state->opcode = M6502_GetOpcode(cpu);
    }

    void Reg(const char *name, uint8_t value) {
        ImGui::Text("%s = $%02x %03d %s", name, value, value, BINARY_BYTE_STRINGS[value]);
    }
};

std::unique_ptr<SettingsUI> Create6502DebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<M6502DebugWindow>(beeb_window);
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

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            if (this->IsStateUnavailableImGui(m)) {
                return;
            }

            m_type = m->GetType();
        }

        this->DoDebugPageOverrideImGui();

        m_hex_editor.DoImGui();

        m_type = nullptr;
    }

  private:
    class Handler : public HexEditorHandler {
      public:
        explicit Handler(MemoryDebugWindow *window)
            : m_window(window) {
        }

        void ReadByte(HexEditorByte *byte, size_t offset) override {
            M6502Word addr = {(uint16_t)offset};

            const DebugBigPage *dbp = m_window->GetDebugBigPageForAddress(addr, false);

            if (!dbp || !dbp->r) {
                byte->got_value = false;
            } else {
                byte->got_value = true;
                byte->value = dbp->r[addr.p.o];
                byte->can_write = dbp->writeable;
            }
        }

        void WriteByte(size_t offset, uint8_t value) override {
            std::vector<uint8_t> data;
            data.resize(1);
            data[0] = value;

            m_window->m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetBytesMessage>((uint16_t)offset,
                                                                                             m_window->m_dso,
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

                        end = std::min(end, 0x10000u);

                        std::vector<uint8_t> buffer;
                        for (uint16_t addr = begin; addr != end; ++addr) {
                            uint8_t value;
                            if (!m_window->ReadByte(&value, nullptr, nullptr, addr, false)) {
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
            const DebugBigPage *dbp = m_window->GetDebugBigPageForAddress({(uint16_t)offset}, false);

            snprintf(text,
                     text_size,
                     upper_case ? "%c%c$%04X" : "%c%c$%04x",
                     dbp->metadata->code,
                     ADDRESS_PREFIX_SEPARATOR,
                     (unsigned)offset);
        }

        bool ParseAddressText(size_t *offset, const char *text) override {
            uint16_t addr;
            if (!ParseAddress(&addr, &m_window->m_dso, m_window->m_type, text)) {
                return false;
            }

            *offset = addr;
            return true;
        }

        int GetNumAddressChars() override {
            return 7;
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

    Handler m_handler;
    HexEditor m_hex_editor;
    const BBCMicroType *m_type = nullptr;
};

std::unique_ptr<SettingsUI> CreateHostMemoryDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<MemoryDebugWindow>(beeb_window);
}

std::unique_ptr<SettingsUI> CreateParasiteMemoryDebugWindow(BeebWindow *beeb_window) {
    return CreateParasiteDebugUI<MemoryDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ExtMemoryDebugWindow : public DebugUI {
  public:
    ExtMemoryDebugWindow() {
        m_memory_editor.ReadFn = &MemoryEditorRead;
        m_memory_editor.WriteFn = &MemoryEditorWrite;
    }

  protected:
    void DoImGui2() override {
        bool enabled;
        uint8_t l, h;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            if (const ExtMem *s = m->DebugGetExtMem()) {
                enabled = true;
                l = s->GetAddressL();
                h = s->GetAddressH();
            } else {
                enabled = false;
                h = l = 0; //inhibit spurious unused variable warning.
            }
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

        std::unique_lock<Mutex> lock;
        const BBCMicro *m = self->m_beeb_thread->LockBeeb(&lock);
        const ExtMem *s = m->DebugGetExtMem();
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
    uint32_t GetExtraImGuiWindowFlags() const override {
        // The bottom line of the disassembly should just be clipped
        // if it runs off the bottom... only drawing whole lines just
        // looks weird. But when that happens, dear imgui
        // automatically adds a scroll bar. And that's even weirder.
        return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }

    const CommandTable *GetCommandTable() const override {
        return &ms_command_table;
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
        CommandContext cc(this, this->GetCommandTable());

        const M6502Config *config;
        uint16_t pc;
        uint8_t a, x, y;
        M6502Word sp;
        M6502P p;
        uint8_t pc_is_mos[16];
        const BBCMicroType *type;
        bool halted;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            const M6502 *s = m->DebugGetM6502(m_dso);
            if (this->IsStateUnavailableImGui(m)) {
                return;
            }

            type = m->GetType();
            halted = m->DebugIsHalted();

            config = s->config;
            pc = s->opcode_pc.w;
            a = s->a;
            x = s->x;
            y = s->y;
            p = M6502_GetP(s);
            sp = s->s;
            m->DebugGetMemBigPageIsMOSTable(pc_is_mos, m_dso);
        }

        float maxY = ImGui::GetCurrentWindow()->Size.y; //-ImGui::GetTextLineHeight()-GImGui->Style.WindowPadding.y*2.f;

        this->DoDebugPageOverrideImGui();

        this->ByteRegUI("A", a);
        ImGui::SameLine();
        this->ByteRegUI("X", x);
        ImGui::SameLine();
        this->ByteRegUI("Y", y);
        ImGui::SameLine();
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
        this->WordRegGui("S", sp);
        ImGui::SameLine();
        this->WordRegGui("PC", {pc});
        ImGui::SameLine();
        cc.DoToggleCheckboxUI("toggle_track_pc");

        cc.DoButton("go_back");

        if (ImGui::InputText("Address",
                             m_address_text, sizeof m_address_text,
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            uint16_t addr;
            if (ParseAddress(&addr, &m_dso, type, m_address_text)) {
                this->GoTo(addr);
            }
        }

        cc.DoButton("step_over");
        ImGui::SameLine();
        cc.DoButton("step_in");

        if (m_track_pc) {
            if (halted) {
                if (m_old_pc != pc) {
                    // well, *something* happened since last time...
                    m_addr = pc;
                    m_old_pc = pc;
                }
            } else {
                m_addr = pc;
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

            const M6502DisassemblyInfo *di = &config->disassembly_info[opcode];

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

            if (line_addr.w == pc) {
                pusher.Push(ImGuiCol_Text, ImVec4(1.f, 1.f, 0.f, 1.f));
            }

            const DebugBigPage *line_dbp = this->GetDebugBigPageForAddress(line_addr, false);
            ImGui::Text("%c%c$%04x", line_dbp->metadata->code, ADDRESS_PREFIX_SEPARATOR, line_addr.w);
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
                this->AddByte(IND_PREFIX, operand.b.l + x, mos, "");
                break;

            case M6502AddrMode_ZPY:
                this->AddByte("", operand.b.l, false, ",Y");
                this->AddByte(IND_PREFIX, operand.b.l + y, mos, "");
                break;

            case M6502AddrMode_ABS:
                // TODO the MOS flag shouldn't apply to JSR or JMP.
                this->AddWord("", operand.w, mos, "");
                break;

            case M6502AddrMode_ABX:
                this->AddWord("", operand.w, mos, ",X");
                this->AddWord(IND_PREFIX, operand.w + x, mos, "");
                break;

            case M6502AddrMode_ABY:
                this->AddWord("", operand.w, mos, ",Y");
                this->AddWord(IND_PREFIX, operand.w + y, mos, "");
                break;

            case M6502AddrMode_INX:
                this->AddByte("(", operand.b.l, false, ",X)");
                this->DoIndirect((operand.b.l + x) & 0xff, mos, 0xff, 0);
                break;

            case M6502AddrMode_INY:
                this->AddByte("(", operand.b.l, false, "),Y");
                this->DoIndirect(operand.b.l, mos, 0xff, y);
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
                this->DoIndirect(operand.w + x, mos, 0xffff, 0);
                break;

            case M6502AddrMode_ZPG_REL_ROCKWELL:
                {
                    M6502Word dest;
                    dest.w = addr + (uint16_t)(int16_t)(int8_t)operand.b.h;
                    this->AddByte("", operand.b.l, false, "");
                    this->AddWord(",", dest.w, false, "");
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
                this->Down(config, -wheel);
            } else if (wheel > 0) {
                this->Up(config, wheel);
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

        char label[] = {
            dbp->metadata->code,
            ADDRESS_PREFIX_SEPARATOR,
            '$',
            HEX_CHARS_LC[w >> 12 & 15],
            HEX_CHARS_LC[w >> 8 & 15],
            HEX_CHARS_LC[w >> 4 & 15],
            HEX_CHARS_LC[w & 15],
            0,
        };

        this->DoClickableAddress(prefix, label, suffix, dbp, {w});
    }

    void AddByte(const char *prefix, uint8_t value, bool mos, const char *suffix) {
        const DebugBigPage *dbp = this->GetDebugBigPageForAddress({value}, mos);

        char label[] = {
            dbp->metadata->code,
            ADDRESS_PREFIX_SEPARATOR,
            '$',
            HEX_CHARS_LC[value >> 4 & 15],
            HEX_CHARS_LC[value & 15],
            0,
        };

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

    bool IsTrackingPC() const {
        return m_track_pc;
    }

    void ToggleTrackPC() {
        m_track_pc = !m_track_pc;

        if (m_track_pc) {
            // force snap to current PC if it's halted.
            m_old_pc = -1;
        }
    }

    bool IsBackEnabled() const {
        return !m_history.empty();
    }

    void Back() {
        if (!m_history.empty()) {
            m_track_pc = false;
            m_addr = m_history.back();
            m_history.pop_back();
        }
    }

    bool IsMoveEnabled() const {
        if (m_track_pc) {
            return false;
        }

        //if(m_line_addrs.empty()) {
        //    return false;
        //}

        return true;
    }

    void PageUp() {
        const M6502Config *config = this->Get6502Config();

        this->Up(config, m_num_lines - 2);
    }

    void PageDown() {
        const M6502Config *config = this->Get6502Config();

        this->Down(config, m_num_lines - 2);
    }

    void Up() {
        const M6502Config *config = this->Get6502Config();

        this->Up(config, 1);
    }

    void Down() {
        const M6502Config *config = this->Get6502Config();

        this->Down(config, 1);
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

    const M6502Config *Get6502Config() {
        std::unique_lock<Mutex> lock;
        const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
        const M6502 *s = m->GetM6502();

        return s->config;
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

    void WordRegGui(const char *name, M6502Word value) {
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

    static ObjectCommandTable<DisassemblyDebugWindow> ms_command_table;
};

const char DisassemblyDebugWindow::IND_PREFIX[] = " --> $";

ObjectCommandTable<DisassemblyDebugWindow> DisassemblyDebugWindow::ms_command_table(
    "Disassembly Window",
    {
        {CommandDef("toggle_track_pc", "Track PC").Shortcut(SDLK_t), &DisassemblyDebugWindow::ToggleTrackPC, &DisassemblyDebugWindow::IsTrackingPC, nullptr},
        {CommandDef("back", "Back").Shortcut(SDLK_BACKSPACE), &DisassemblyDebugWindow::Back, nullptr, &DisassemblyDebugWindow::IsBackEnabled},
        {CommandDef("up", "Up").Shortcut(SDLK_UP), &DisassemblyDebugWindow::Up, &DisassemblyDebugWindow::IsMoveEnabled},
        {CommandDef("down", "Down").Shortcut(SDLK_DOWN), &DisassemblyDebugWindow::Down, &DisassemblyDebugWindow::IsMoveEnabled},
        {CommandDef("page_up", "Page Up").Shortcut(SDLK_PAGEUP), &DisassemblyDebugWindow::PageUp, &DisassemblyDebugWindow::IsMoveEnabled},
        {CommandDef("page_down", "Page Down").Shortcut(SDLK_PAGEDOWN), &DisassemblyDebugWindow::PageDown, &DisassemblyDebugWindow::IsMoveEnabled},
        {CommandDef("step_over", "Step Over").Shortcut(SDLK_F10), &DisassemblyDebugWindow::StepOver, nullptr, &DisassemblyDebugWindow::IsRunEnabled},
        {CommandDef("step_in", "Step In").Shortcut(SDLK_F11), &DisassemblyDebugWindow::StepIn, nullptr, &DisassemblyDebugWindow::IsRunEnabled},
    });

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
        CRTC::Registers registers;
        uint8_t address;
        uint16_t cursor_address;
        uint16_t display_address;
        BBCMicro::AddressableLatch latch;
        CRTC::InternalState st;
        uint16_t char_addr, line_addr, next_line_addr;

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            const CRTC *c = m->DebugGetCRTC();
            //const VideoULA *u=m->DebugGetVideoULA();

            registers = c->m_registers;
            address = c->m_address;
            st = c->m_st;

            cursor_address = m->DebugGetBeebAddressFromCRTCAddress(registers.bits.cursorh, registers.bits.cursorl);
            display_address = m->DebugGetBeebAddressFromCRTCAddress(registers.bits.addrh, registers.bits.addrl);
            char_addr = m->DebugGetBeebAddressFromCRTCAddress(st.char_addr.b.h, st.char_addr.b.l);
            line_addr = m->DebugGetBeebAddressFromCRTCAddress(st.line_addr.b.h, st.line_addr.b.l);
            next_line_addr = m->DebugGetBeebAddressFromCRTCAddress(st.next_line_addr.b.h, st.next_line_addr.b.l);

            latch = m->DebugGetAddressableLatch();

            //ucontrol=u->control;
            //memcpy(upalette,u->m_palette,16);
        }

        if (ImGui::CollapsingHeader("Register Values")) {
            ImGui::Text("Address = $%02x %03u", address, address);
            for (size_t i = 0; i < 18; ++i) {
                ImGui::Text("R%zu = $%02x %03u %s", i, registers.values[i], registers.values[i], BINARY_BYTE_STRINGS[registers.values[i]]);
            }
            ImGui::Separator();
        }

        ImGui::Text("H Displayed = %u, Total = %u", registers.bits.nhd, registers.bits.nht);
        ImGui::Text("V Displayed = %u, Total = %u", registers.bits.nvd, registers.bits.nvt);
        ImGui::Text("Scanlines = %u * %u + %u = %u", registers.bits.nvd, registers.bits.nr + 1, registers.bits.nadj, registers.bits.nvd * (registers.bits.nr + 1) + registers.bits.nadj);
        ImGui::Text("Address = $%04x", display_address);
        ImGui::Text("(Wrap Adjustment = $%04x)", BBCMicro::SCREEN_WRAP_ADJUSTMENTS[latch.bits.screen_base] << 3);
        ImGui::Separator();
        ImGui::Text("HSync Pos = %u, Width = %u", registers.bits.nhsp, registers.bits.nsw.bits.wh);
        ImGui::Text("VSync Pos = %u, Width = %u", registers.bits.nvsp, registers.bits.nsw.bits.wv);
        ImGui::Text("Interlace Sync = %s, Video = %s", BOOL_STR(registers.bits.r8.bits.s), BOOL_STR(registers.bits.r8.bits.v));
        ImGui::Text("Delay Mode = %s", DELAY_NAMES[registers.bits.r8.bits.d]);
        ImGui::Separator();
        ImGui::Text("Cursor Start = %u, End = %u, Mode = %s", registers.bits.ncstart.bits.start, registers.bits.ncend, GetCRTCCursorModeEnumName(registers.bits.ncstart.bits.mode));
        ImGui::Text("Cursor Delay Mode = %s", DELAY_NAMES[registers.bits.r8.bits.c]);
        ImGui::Text("Cursor Address = $%04x", cursor_address);
        ImGui::Separator();
        ImGui::Text("Column = %u, hdisp=%s", st.column, BOOL_STR(st.hdisp));
        ImGui::Text("Row = %u, Raster = %u, vdisp=%s", st.row, st.raster, BOOL_STR(st.vdisp));
        ImGui::Text("Char address = $%04X (CRTC) / $%04X (BBC)", st.char_addr.w, char_addr);
        ImGui::Text("Line address = $%04X (CRTC) / $%04X (BBC)", st.line_addr.w, line_addr);
        ImGui::Text("Next line address = $%04X (CRTC) / $%04X (BBC)", st.next_line_addr.w, next_line_addr);
        ImGui::Text("DISPEN queue = %%%s", BINARY_BYTE_STRINGS[st.skewed_display]);
        ImGui::Text("CUDISP queue = %%%s", BINARY_BYTE_STRINGS[st.skewed_cudisp]);
        ImGui::Text("VSync counter = %d", st.vsync_counter);
        ImGui::Text("HSync counter = %d", st.hsync_counter);
        ImGui::Text("VAdj counter = %d", st.vadj_counter);
        ImGui::Text("check_vadj = %s", BOOL_STR(st.check_vadj));
        ImGui::Text("in_vadj = %s", BOOL_STR(st.in_vadj));
        ImGui::Text("end_of_vadj_latched = %s", BOOL_STR(st.end_of_vadj_latched));
        ImGui::Text("had_vsync_this_row = %s", BOOL_STR(st.had_vsync_this_row));
        ImGui::Text("end_of_main_latched = %s", BOOL_STR(st.end_of_main_latched));
        ImGui::Text("do_even_frame_logic = %s", BOOL_STR(st.do_even_frame_logic));
        ImGui::Text("first_scanline = %s", BOOL_STR(st.first_scanline));
        ImGui::Text("in_dummy_raster = %s", BOOL_STR(st.in_dummy_raster));
        ImGui::Text("end_of_frame_latched = %s", BOOL_STR(st.end_of_frame_latched));
        ImGui::Text("cursor = %s", BOOL_STR(st.cursor));
    }

  private:
    static const char *const INTERLACE_NAMES[];
    static const char *const DELAY_NAMES[];
};

const char *const CRTCDebugWindow::INTERLACE_NAMES[] = {"Normal", "Normal", "Interlace sync", "Interlace sync+video"};

const char *const CRTCDebugWindow::DELAY_NAMES[] = {"0", "1", "2", "Disabled"};

std::unique_ptr<SettingsUI> CreateCRTCDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<CRTCDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class VideoULADebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        VideoULA::Control control;
        uint8_t palette[16];
        VideoDataPixel nula_palette[16];
        uint8_t nula_flash[16];
        uint8_t nula_direct_palette;
        uint8_t nula_disable_a1;
        uint8_t nula_scroll_offset;
        uint8_t nula_blanking_size;
        VideoULA::NuLAAttributeMode nula_attribute_mode;
        bool nula;

        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            const VideoULA *u = m->DebugGetVideoULA();

            control = u->control;
            memcpy(palette, u->m_palette, 16);
            memcpy(nula_palette, u->output_palette, 16 * sizeof(VideoDataPixel));
            memcpy(nula_flash, u->m_flash, 16);
            nula_direct_palette = u->m_direct_palette;
            nula_disable_a1 = u->m_disable_a1;
            nula_scroll_offset = u->m_scroll_offset;
            nula_blanking_size = u->m_blanking_size;
            nula_attribute_mode = u->m_attribute_mode;
            nula = u->nula;
        }

        if (ImGui::CollapsingHeader("Register Values")) {
            ImGui::Text("Control = $%02x %03u %s", control.value, control.value, BINARY_BYTE_STRINGS[control.value]);
            for (size_t i = 0; i < 16; ++i) {
                uint8_t p = palette[i];
                ImGui::Text("Palette[%zu] = $%01x %02u %s ", i, p, p, BINARY_BYTE_STRINGS[p] + 4);

                uint8_t colour = p & 7;
                if (p & 8) {
                    if (control.bits.flash) {
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

        ImGui::Text("Flash colour = %u", control.bits.flash);
        ImGui::Text("Teletext output = %s", BOOL_STR(control.bits.teletext));
        ImGui::Text("Chars per line = %u", (1 << control.bits.line_width) * 10);
        ImGui::Text("6845 clock = %u MHz", 1 + control.bits.fast_6845);
        ImGui::Text("Cursor Shape = %s", CURSOR_SHAPES[control.bits.cursor]);

        for (uint8_t i = 0; i < 16; i += 4) {
            ImGui::Text("Palette:");
            ImGuiStyleVarPusher vpusher(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));

            for (uint8_t j = 0; j < 4; ++j) {
                uint8_t index = i + j;
                uint8_t entry = palette[index];

                uint8_t colour = entry & 7;
                if (entry & 8) {
                    if (control.bits.flash) {
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
        if (!nula_disable_a1) {
            nula_section_flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        if (nula) {
            if (ImGui::CollapsingHeader("Video NuLA", nula_section_flags)) {
                ImGui::Text("Enabled: %s", BOOL_STR(!nula_disable_a1));

                ImGui::Text("Direct palette mode: %s", BOOL_STR(nula_direct_palette));
                ImGui::Text("Scroll offset: %u", nula_scroll_offset);
                ImGui::Text("Blanking size: %u", nula_blanking_size);
                ImGui::Text("Attribute mode: %s", BOOL_STR(nula_attribute_mode.bits.enabled));
                ImGui::Text("Text attribute mode: %s", BOOL_STR(nula_attribute_mode.bits.text));

                for (uint8_t i = 0; i < 16; i += 4) {
                    ImGui::Text("Palette:");

                    ImGuiStyleVarPusher var_pusher(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));

                    for (uint8_t j = 0; j < 4; ++j) {
                        uint8_t index = i + j;
                        const VideoDataPixel *e = &nula_palette[index];

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
    return CreateDebugUI<VideoULADebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class R6522DebugWindow : public DebugUI {
  public:
    struct PortState {
        uint8_t or_, ddr, p, c1, c2;
    };

    struct State {
        uint16_t t1, t2;
        M6502Word t1l;
        uint8_t t2ll;
        uint8_t sr;
        R6522::ACR acr;
        R6522::PCR pcr;
        R6522::IRQ ifr, ier;
        PortState a, b;
    };

  protected:
    void DoRegisterValuesGui(const State &s, bool has_debug_state, BBCMicro::HardwareDebugState hw, R6522::IRQ BBCMicro::HardwareDebugState::*irq_mptr) {
        this->DoPortRegisterValuesGui('A', s.a);
        this->DoPortRegisterValuesGui('B', s.b);
        ImGui::Text("T1 : $%04x %05d %s%s", s.t1, s.t1, BINARY_BYTE_STRINGS[s.t1 >> 8 & 0xff], BINARY_BYTE_STRINGS[s.t1 & 0xff]);
        ImGui::Text("T1L: $%04x %05d %s%s", s.t1l.w, s.t1l.w, BINARY_BYTE_STRINGS[s.t1l.b.h], BINARY_BYTE_STRINGS[s.t1l.b.l]);
        ImGui::Text("T2 : $%04x %05d %s%s", s.t2, s.t2, BINARY_BYTE_STRINGS[s.t2 >> 8 & 0xff], BINARY_BYTE_STRINGS[s.t2 & 0xff]);
        ImGui::Text("SR : $%02x %03d %s", s.sr, s.sr, BINARY_BYTE_STRINGS[s.sr]);
        ImGui::Text("ACR: PA latching = %s", BOOL_STR(s.acr.bits.pa_latching));
        ImGui::Text("ACR: PB latching = %s", BOOL_STR(s.acr.bits.pb_latching));
        ImGui::Text("ACR: Shift mode = %s", ACR_SHIFT_MODES[s.acr.bits.sr]);
        ImGui::Text("ACR: T2 mode = %s", s.acr.bits.t2_count_pb6 ? "Count PB6 pulses" : "Timed interrupt");
        ImGui::Text("ACR: T1 continuous = %s, output PB7 = %s", BOOL_STR(s.acr.bits.t1_continuous), BOOL_STR(s.acr.bits.t1_output_pb7));
        ImGui::Text("PCR: CA1 = %cve edge", s.pcr.bits.ca1_pos_irq ? '+' : '-');
        ImGui::Text("PCR: CA2 = %s", PCR_CONTROL_MODES[s.pcr.bits.ca2_mode]);
        ImGui::Text("PCR: CB1 = %cve edge", s.pcr.bits.cb1_pos_irq ? '+' : '-');
        ImGui::Text("PCR: CB2 = %s", PCR_CONTROL_MODES[s.pcr.bits.cb2_mode]);

        ImGui::Text("     [%-3s][%-3s][%-3s][%-3s][%-3s][%-3s][%-3s]", IRQ_NAMES[6], IRQ_NAMES[5], IRQ_NAMES[4], IRQ_NAMES[3], IRQ_NAMES[2], IRQ_NAMES[1], IRQ_NAMES[0]);
        ImGui::Text("IFR:  %2u   %2u   %2u   %2u   %2u   %2u   %2u", s.ifr.value & 1 << 6, s.ifr.value & 1 << 5, s.ifr.value & 1 << 4, s.ifr.value & 1 << 3, s.ifr.value & 1 << 2, s.ifr.value & 1 << 1, s.ifr.value & 1 << 0);
        ImGui::Text("IER:  %2u   %2u   %2u   %2u   %2u   %2u   %2u", s.ier.value & 1 << 6, s.ier.value & 1 << 5, s.ier.value & 1 << 4, s.ier.value & 1 << 3, s.ier.value & 1 << 2, s.ier.value & 1 << 1, s.ier.value & 1 << 0);

        if (has_debug_state) {
            ImGui::Separator();

            bool changed = false;

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
                std::unique_lock<Mutex> lock;
                BBCMicro *m = m_beeb_thread->LockMutableBeeb(&lock);
                m->SetHardwareDebugState(hw);
            }
        }
    }

    void GetState(State *state, const R6522 *via) {
        state->t1 = (uint16_t)via->m_t1;
        state->t2 = (uint16_t)via->m_t2;
        state->t1l.b.l = via->m_t1ll;
        state->t1l.b.h = via->m_t1lh;
        state->t2ll = via->m_t2ll;
        state->sr = via->m_sr;
        state->acr = via->m_acr;
        state->pcr = via->m_pcr;
        state->ifr = via->ifr;
        state->ier = via->ier;

        state->a.or_ = via->a.or_;
        state->a.ddr = via->a.ddr;
        state->a.p = via->a.p;
        state->a.c1 = via->a.c1;
        state->a.c2 = via->a.c2;

        state->b.or_ = via->b.or_;
        state->b.ddr = via->b.ddr;
        state->b.p = via->b.p;
        state->b.c1 = via->b.c1;
        state->b.c2 = via->b.c2;
    }

  private:
    void DoPortRegisterValuesGui(char port, const PortState &p) {
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
        State state;
        BBCMicro::AddressableLatch latch;
        const BBCMicroType *type;
        bool has_debug_state;
        BBCMicro::HardwareDebugState hw;
        uint8_t rtc_addr = 0;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
            this->GetState(&state, m->DebugGetSystemVIA());
            latch = m->DebugGetAddressableLatch();
            type = m->GetType();
            has_debug_state = m->HasDebugState();
            hw = m->GetHardwareDebugState();

            if (type->flags & BBCMicroTypeFlag_HasRTC) {
                const MC146818 *rtc = m->DebugGetRTC();

                rtc_addr = rtc->GetAddress();
            }
        }

        this->DoRegisterValuesGui(state, has_debug_state, hw, &BBCMicro::HardwareDebugState::system_via_irq_breakpoints);

        ImGui::Separator();

        BBCMicro::SystemVIAPB pb;
        pb.value = state.b.p;

        ImGui::Text("Port B inputs:");

        ImGui::BulletText("Joystick 0 Fire = %s", BOOL_STR(!pb.bits.not_joystick0_fire));
        ImGui::BulletText("Joystick 1 Fire = %s", BOOL_STR(!pb.bits.not_joystick1_fire));
        if (!(type->flags & BBCMicroTypeFlag_HasRTC)) {
            ImGui::BulletText("Speech Ready = %u, IRQ = %u", pb.b_bits.speech_ready, pb.b_bits.speech_interrupt);
        }

        ImGui::Separator();

        ImGui::Text("Port B outputs:");

        ImGui::BulletText("Latch Bit = %u, Value = %u", pb.bits.latch_index, pb.bits.latch_value);

        if (type->flags & BBCMicroTypeFlag_HasRTC) {
            ImGui::BulletText("RTC CS = %u, AS = %u",
                              pb.m128_bits.rtc_chip_select,
                              pb.m128_bits.rtc_address_strobe);
            ImGui::BulletText("(FYI: RTC address register value = %u)", rtc_addr);
        }

        ImGui::Separator();

        ImGui::Text("Addressable latch:");
        ImGui::Text("Value: %%%s ($%02x) (%03u)", BINARY_BYTE_STRINGS[latch.value], latch.value, latch.value);

        ImGui::BulletText("Keyboard Write = %s", BOOL_STR(!latch.bits.not_kb_write));
        ImGui::BulletText("Sound Write = %s", BOOL_STR(!latch.bits.not_sound_write));
        ImGui::BulletText("Screen Wrap Size = $%04x", BBCMicro::SCREEN_WRAP_ADJUSTMENTS[latch.bits.screen_base] << 3);
        ImGui::BulletText("Caps Lock LED = %s", BOOL_STR(latch.bits.caps_lock_led));
        ImGui::BulletText("Shift Lock LED = %s", BOOL_STR(latch.bits.shift_lock_led));

        if (type->flags & BBCMicroTypeFlag_HasRTC) {
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
        State state;
        bool has_debug_state;
        BBCMicro::HardwareDebugState hw;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
            this->GetState(&state, m->DebugGetUserVIA());
            has_debug_state = m->HasDebugState();
            hw = m->GetHardwareDebugState();
        }

        this->DoRegisterValuesGui(state, has_debug_state, hw, &BBCMicro::HardwareDebugState::user_via_irq_breakpoints);
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
        std::vector<uint8_t> nvram;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            nvram = m->GetNVRAM();
        }

        if (nvram.empty()) {
            ImGui::Text("This computer has no non-volatile RAM.");
        } else {
            if (ImGui::CollapsingHeader("NVRAM contents")) {
                for (size_t i = 0; i < nvram.size(); ++i) {
                    uint8_t value = nvram[i];
                    ImGui::Text("%zu. %3d %3uu $%02x %%%s", i, value, value, value, BINARY_BYTE_STRINGS[value]);
                }
                ImGui::Separator();
            }

            if (nvram.size() >= 50) {
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
            }
        }
    }

  private:
};

std::unique_ptr<SettingsUI> CreateNVRAMDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<NVRAMDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SN76489DebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        SN76489::ChannelValues values[4];
        uint16_t seed;
        bool we;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
            const SN76489 *sn = m->DebugGetSN76489();
            sn->GetState(values, &seed);

            BBCMicro::AddressableLatch latch = m->DebugGetAddressableLatch();
            we = !latch.bits.not_sound_write;
        }

        ImGui::Text("Write Enable: %s", BOOL_STR(we));

        Tone(values[0], 0);
        Tone(values[1], 1);
        Tone(values[2], 2);

        {
            const char *type;
            if (values[3].freq & 4) {
                type = "White";
            } else {
                type = "Periodic";
            }

            uint16_t sn_freq;
            const char *suffix = "";
            switch (values[3].freq & 3) {
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
                sn_freq = values[2].freq;
                break;
            }

            ImGui::Text("Noise : vol=%-2d freq=%-5u (0x%04x) (%uHz)",
                        values[3].vol, sn_freq, sn_freq, GetHz(sn_freq));
            ImGui::Text("        %s%s", type, suffix);
            ImGui::Text("        seed: $%04x %%%s%s",
                        seed, BINARY_BYTE_STRINGS[seed >> 8], BINARY_BYTE_STRINGS[seed & 0xff]);
        }
    }

  private:
    static uint32_t GetHz(uint16_t sn_freq) {
        if (sn_freq == 0) {
            sn_freq = 1024;
        }
        return 4000000u / (sn_freq * 32u);
    }

    static void Tone(const SN76489::ChannelValues &values, int n) {
        ImGui::Text("Tone %d: vol=%-2d freq=%-5u (0x%04x) (%uHz)",
                    n, values.vol, values.freq, values.freq, GetHz(values.freq));
    }
};

std::unique_ptr<SettingsUI> CreateSN76489DebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<SN76489DebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t ALL_USER_MEM_BIG_PAGES[16] = {};

class PagingDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        this->DoDebugPageOverrideImGui();

        ROMSEL romsel;
        ACCCON acccon;
        const BBCMicroType *type;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
            m->DebugGetPaging(&romsel, &acccon);
            type = m->GetType();
        }

        (*type->apply_dso_fn)(&romsel, &acccon, m_dso);

        MemoryBigPageTables tables;
        bool io, crt_shadow;
        (*type->get_mem_big_page_tables_fn)(&tables, &io, &crt_shadow, romsel, acccon);

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

            this->DoTypeColumn(type, tables, io, 0, i);
            ImGui::NextColumn();

            if (all_user) {
                ImGui::TextUnformatted("N/A");
            } else {
                this->DoTypeColumn(type, tables, io, 1, i);
            }
            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        ImGui::Separator();

        ImGui::Text("Display memory: %s", crt_shadow ? "Shadow RAM" : "Main RAM");

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
    void DoTypeColumn(const BBCMicroType *type, const MemoryBigPageTables &tables, bool io, size_t index, size_t mem_big_page_index) {
        uint8_t big_page_index = tables.mem_big_pages[index][mem_big_page_index];
        const BigPageMetadata *metadata = &type->big_pages_metadata[big_page_index];

        if (big_page_index == MOS_BIG_PAGE_INDEX + 3 && io) {
            ImGui::Text("%s + I/O", metadata->description.c_str());
        } else {
            ImGui::TextUnformatted(metadata->description.c_str());
        }
    }
};

std::unique_ptr<SettingsUI> CreatePagingDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<PagingDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BreakpointsDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        bool changed = false;
        const BBCMicroType *type;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);

            uint64_t breakpoints_change_counter = m->DebugGetBreakpointsChangeCounter();
            if (breakpoints_change_counter != m_breakpoints_change_counter) {
                m->DebugGetDebugFlags(m_host_address_debug_flags, m_parasite_address_debug_flags, &m_big_page_debug_flags[0][0]);
                m_breakpoints_change_counter = breakpoints_change_counter;
                ++m_num_updates;
                changed = true;
            }

            type = m->GetType();
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

                    if (bp->big_page == HOST_ADDRESS_BREAKPOINT_BIG_PAGE) {
                        if (uint8_t *flags = this->Row(bp, "$%04x Host", bp->offset)) {

                            M6502Word addr = {bp->offset};
                            m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetAddressDebugFlags>(addr, 0, *flags));
                        }
                    } else if (bp->big_page == PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE) {
                        if (uint8_t *flags = this->Row(bp, "$%04x Parasite", bp->offset)) {

                            M6502Word addr = {bp->offset};
                            m_beeb_thread->Send(std::make_shared<BeebThread::DebugSetAddressDebugFlags>(addr, BBCMicroDebugStateOverride_Parasite, *flags));
                        }
                    } else {
                        // Byte breakpoint
                        //                ASSERT(bp->big_page<BBCMicro::NUM_BIG_PAGES);
                        //                ASSERT(bp->offset<BBCMicro::BIG_PAGE_SIZE_BYTES);
                        //                uint8_t *flags=&m_big_page_debug_flags[bp->big_page][bp->offset];

                        const BigPageMetadata *metadata = &type->big_pages_metadata[bp->big_page];

                        if (uint8_t *flags = this->Row(bp,
                                                       "%c%c$%04x",
                                                       metadata->code,
                                                       ADDRESS_PREFIX_SEPARATOR,
                                                       metadata->addr + bp->offset)) {
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
    static constexpr uint8_t HOST_ADDRESS_BREAKPOINT_BIG_PAGE = NUM_BIG_PAGES + 0;
    static constexpr uint8_t PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE = NUM_BIG_PAGES + 1;

    struct Breakpoint {
        uint8_t big_page;
        uint16_t offset;
    };
    static_assert(sizeof(Breakpoint) == 4, "");

    uint64_t m_breakpoints_change_counter = 0;
    uint64_t m_num_updates = 0;

    // This is quite a large object, by BBC standards at least.
    uint8_t m_host_address_debug_flags[65536] = {};
    uint8_t m_parasite_address_debug_flags[65536] = {};
    uint8_t m_big_page_debug_flags[NUM_BIG_PAGES][BBCMicro::BIG_PAGE_SIZE_BYTES] = {};

    // The retain flag indicates that the byte should be listed even if there's
    // no breakpoint set. This prevents rows disappearing when you untick all
    // the entries.
    uint8_t m_host_address_debug_flags_retain[65536 >> 3] = {};
    uint8_t m_parasite_address_debug_flags_retain[65536 >> 3] = {};
    uint8_t m_big_page_debug_flags_retain[NUM_BIG_PAGES][BBCMicro::BIG_PAGE_SIZE_BYTES >> 3] = {};

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
        if (bp->big_page == HOST_ADDRESS_BREAKPOINT_BIG_PAGE) {
            flags = &m_host_address_debug_flags[bp->offset];
            retain = &m_host_address_debug_flags_retain[bp->offset >> 3];
        } else if (bp->big_page == PARASITE_ADDRESS_BREAKPOINT_BIG_PAGE) {
            flags = &m_parasite_address_debug_flags[bp->offset];
            retain = &m_parasite_address_debug_flags_retain[bp->offset >> 3];
        } else {
            ASSERT(bp->big_page < NUM_BIG_PAGES);
            ASSERT(bp->offset < BBCMicro::BIG_PAGE_SIZE_BYTES);
            flags = &m_big_page_debug_flags[bp->big_page][bp->offset];
            retain = &m_big_page_debug_flags_retain[bp->big_page][bp->offset >> 3];
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

        for (uint8_t i = 0; i < NUM_BIG_PAGES; ++i) {
            const uint8_t *big_page_debug_flags = m_big_page_debug_flags[i];

            for (size_t j = 0; j < BBCMicro::BIG_PAGE_SIZE_BYTES; ++j) {
                if (big_page_debug_flags[j] || m_big_page_debug_flags_retain[i][j >> 3] & 1 << (j & 7)) {
                    m_breakpoints.push_back({i, (uint16_t)j});
                }
            }
        }
    }

    void UpdateAddressBreakpoints(uint8_t *address_debug_flags, uint8_t *address_debug_flags_retain, uint8_t big_page_index) {
        for (size_t i = 0; i < 65536; ++i) {
            if (address_debug_flags[i] != 0 || address_debug_flags_retain[i >> 3] & 1 << (i & 7)) {
                m_breakpoints.push_back({big_page_index, (uint16_t)i});
            }
        }
    }
};

std::unique_ptr<SettingsUI> CreateBreakpointsDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<BreakpointsDebugWindow>(beeb_window);
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
                const BBCMicroType *type;
                {
                    std::unique_lock<Mutex> lock;
                    const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
                    type = m->GetType();
                }

                // The debug stuff is oriented around the CPU's view of memory,
                // but the video unit's address is from the CRTC's perspective.

                M6502Word crtc_addr = {unit->metadata.address};

                const BigPageMetadata *metadata = &type->big_pages_metadata[crtc_addr.p.p];

                m_dso &= metadata->dso_mask;
                m_dso |= metadata->dso_value;

                M6502Word cpu_addr = {(uint16_t)(metadata->addr + crtc_addr.p.o)};

                ImGui::Text("Address: %c%c$%04x", metadata->code, ADDRESS_PREFIX_SEPARATOR, cpu_addr.w);
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
    return CreateDebugUI<PixelMetadataUI>(beeb_window);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class StackDebugWindow : public DebugUI {
  public:
  protected:
    void DoImGui2() override {
        static const char ADDR_CONTEXT_POPUP_NAME[] = "stack_addr_context_popup";

        uint8_t s;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
            if (this->IsStateUnavailableImGui(m)) {
                return;
            }
            const M6502 *cpu = m->DebugGetM6502(m_dso);
            s = cpu->s.b.l;
        }

        const DebugBigPage *value_dbp = this->GetDebugBigPageForAddress({0}, false);

        ImGui::Columns(7, "stack_columns");
        ImGui::TextUnformatted("");
        ImGui::NextColumn();
        ImGui::TextUnformatted("%d");
        ImGui::NextColumn();
        ImGui::TextUnformatted("%u");
        ImGui::NextColumn();
        ImGui::TextUnformatted("%c");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Hex");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Binary");
        ImGui::NextColumn();
        ImGui::TextUnformatted("Addr");
        ImGui::NextColumn();
        ImGui::Separator();

        ImGuiStyleColourPusher colour_pusher;

        for (int offset = 255; offset >= 0; --offset) {
            if (offset == s) {
                colour_pusher.Push(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }

            M6502Word value_addr = {(uint16_t)(0x100 + offset)};
            uint8_t value = value_dbp->r[value_addr.w];

            ImGui::Text("$%04x", value_addr.w);
            this->DoBytePopupGui(value_dbp, value_addr);
            ImGui::NextColumn();

            ImGui::Text("% 3d", (int8_t)value);
            ImGui::NextColumn();

            ImGui::Text("%3u", value);
            ImGui::NextColumn();

            if (value >= 32 && value < 127) {
                ImGui::Text("%c", (char)value);
            }
            ImGui::NextColumn();

            ImGui::Text("$%02x", value);
            ImGui::NextColumn();

            ImGui::TextUnformatted(BINARY_BYTE_STRINGS[value]);
            ImGui::NextColumn();

            if (offset == 255) {
                ImGui::TextUnformatted("-");
            } else {
                M6502Word addr;
                addr.b.l = value;
                addr.b.h = value_dbp->r[0x100 + offset + 1];
                ImGui::Text("$%04x", addr.w);

                ImGuiIDPusher pusher(offset);

                // Check for opening popup here.
                //
                // Don't use ImGui::BeginPopupContextItem(), as that doesn't work properly
                // for text items.
                if (ImGui::IsMouseClicked(1)) {
                    if (ImGui::IsItemHovered()) {
                        ImGui::OpenPopup(ADDR_CONTEXT_POPUP_NAME);
                    }
                }

                if (ImGui::BeginPopup(ADDR_CONTEXT_POPUP_NAME)) {
                    {
                        ImGuiIDPusher pusher2(0);

                        const DebugBigPage *dbp = this->GetDebugBigPageForAddress(addr, false);
                        this->DoByteDebugGui(dbp, addr);
                    }

                    ImGui::Separator();

                    {
                        ImGuiIDPusher pusher2(1);

                        const DebugBigPage *dbp = this->GetDebugBigPageForAddress({(uint16_t)(addr.w + 1)}, false);
                        this->DoByteDebugGui(dbp, {(uint16_t)(addr.w + 1)});
                    }

                    ImGui::EndPopup();
                }
            }
            ImGui::NextColumn();
        }
    }

  private:
};

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
        for (uint8_t i = 0; i < TUBE_FIFO1_SIZE_BYTES; ++i) {
            snprintf(m_fifo1_header + i * 3, 4, "%-3u", i);
        }
    }

  protected:
    void DoImGui2() override {
        Tube tube;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
            const Tube *t = m->DebugGetTube();
            if (!t) {
                ImGui::Text("No Tube");
                return;
            }

            tube = *t; // the whole thing is 64 bytes
        }

        ImGui::Checkbox("Sort FIFOs by index", &m_sort_by_index);

        ImGuiHeader("Tube Status");

        ImGui::BulletText("Parasite: IRQ=%d NMI=%d", tube.pirq.bits.pirq, tube.pirq.bits.pnmi);
        ImGui::BulletText("Host: IRQ=%d", tube.hirq.bits.hirq);
        ImGui::Text("Status: $%02x (%%%s) (T=%d P=%d V=%d M=%d J=%d I=%d Q=%d)",
                    tube.status.value,
                    BINARY_BYTE_STRINGS[tube.status.value],
                    tube.status.bits.t,
                    tube.status.bits.p,
                    tube.status.bits.v,
                    tube.status.bits.m,
                    tube.status.bits.j,
                    tube.status.bits.i,
                    tube.status.bits.q);
        ImGui::BulletText("FIFO1: PIRQ=%s", BOOL_STR(tube.status.bits.i));
        ImGui::BulletText("FIFO3: PNMI=%s", BOOL_STR(tube.status.bits.m));
        ImGui::BulletText("FIFO4: PIRQ=%s HIRQ=%s", BOOL_STR(tube.status.bits.j), BOOL_STR(tube.status.bits.q));

        if (m_sort_by_index) {
            this->DoFIFO1P2HGui(&tube);
            this->DoFIFO1H2PGui(&tube);
            this->DoFIFO2P2HGui(&tube);
            this->DoFIFO2H2PGui(&tube);
            this->DoFIFO3P2HGui(&tube);
            this->DoFIFO3H2PGui(&tube);
            this->DoFIFO4P2HGui(&tube);
            this->DoFIFO4H2PGui(&tube);
        } else {
            this->DoFIFO1P2HGui(&tube);
            this->DoFIFO2P2HGui(&tube);
            this->DoFIFO3P2HGui(&tube);
            this->DoFIFO4P2HGui(&tube);
            this->DoFIFO1H2PGui(&tube);
            this->DoFIFO2H2PGui(&tube);
            this->DoFIFO3H2PGui(&tube);
            this->DoFIFO4H2PGui(&tube);
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

        for (uint8_t i = 0; i < t->p2h1_n; ++i) {
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
        ADCStatusRegister status;
        uint16_t addr;
        uint16_t avalues[4];
        uint16_t timer;
        {
            std::unique_lock<Mutex> lock;
            const BBCMicro *m = m_beeb_thread->LockBeeb(&lock);
            const ADC *adc = m->DebugGetADC();

            addr = m->GetType()->adc_addr;
            status = adc->m_status;
            timer=adc->m_timer;
            adc->DebugGetChannelValues(avalues);
        }

        ImGui::Text("ADC address: $%04x", addr);

        ImGuiHeader("\"Analogue\" values");
        for (int ch = 0; ch < 4; ++ch) {
            uint16_t avalue = avalues[ch];
            ImGui::BulletText("%d: %-5u $%04x %.5f", ch, avalue, avalue, avalue / 65535.);
        }

        ImGuiHeader("Status");
        ImGui::Text("Status: %3u $%02x %%%s", status.value, status.value, BINARY_BYTE_STRINGS[status.value]);
        ImGui::Text("Conversion time left: %d " MICROSECONDS_UTF8, timer);
        ImGui::BulletText("Channel: %u", status.bits.channel);
        ImGui::BulletText("Flag: %u", status.bits.flag);
        ImGui::BulletText("Precision: %d bits", status.bits.prec_10_bit ? 10 : 8);
        ImGui::BulletText("MSB: %u", status.bits.msb);
        ImGui::BulletText("Busy: %s", BOOL_STR(!status.bits.not_busy));
        ImGui::BulletText("EOC: %s", BOOL_STR(!status.bits.not_eoc));
    }

  private:
};

std::unique_ptr<SettingsUI> CreateADCDebugWindow(BeebWindow *beeb_window) {
    return CreateDebugUI<ADCDebugWindow>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
