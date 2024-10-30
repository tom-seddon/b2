#ifndef HEADER_38BDE8E10F8A41E7A984762F46CBD9C5 // -*- mode:c++ -*-
#define HEADER_38BDE8E10F8A41E7A984762F46CBD9C5

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Log;

#include <memory>

#include "conf.h"
#include "6502.h"
#include "adc.h"
#include "PCD8572.h"
#include "crtc.h"
#include "ExtMem.h"
#include "1770.h"
#include "6522.h"
#include "SN76489.h"
#include "MC146818.h"
#include "VideoULA.h"
#include "teletext.h"
#include "DiscInterface.h"
#include "tube.h"
#include "video.h"
#include "type.h"
#include "BBCMicroParasiteType.h"

#include <shared/enum_decl.h>
#include "BBCMicroState.inl"
#include <shared/enum_end.h>

class DiscImage;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// All the shared_ptr<vector<uint8_t>> point to vectors that are never resized
// once created. So in principle it's safe(ish) to call size() and operator[]
// from multiple threads without bothering to lock.
//
// The ExtMem has a shared_ptr<vector<uint8_t>> inside it, and that follows the
// same rules.

// The access specifiers are a bit of a random jumble.
//
// BBCMicro is a friend class, so it can always access anything.
//
// Anything public is safe to access from a const BBCMicroState (or derived
// class).
//
// Anything protected is either never used outside BBCMicro, or requires a
// getter for some reason (optional hardware, access controlled by a debug state
// override, etc.).

class BBCMicroState {
  public:
#include <shared/pushwarn_bitfields.h>
    struct SystemVIAPBBits {
        static const uint8_t NOT_JOYSTICK0_FIRE_BIT = 4;
        static const uint8_t NOT_JOYSTICK1_FIRE_BIT = 5;
        uint8_t latch_index : 3, latch_value : 1, not_joystick0_fire : 1, not_joystick1_fire : 1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct BSystemVIAPBBits {
        uint8_t _ : 6, speech_ready : 1, speech_interrupt : 1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct Master128SystemVIAPBBits {
        uint8_t _ : 6, rtc_chip_select : 1, rtc_address_strobe : 1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct MasterCompactSystemVIAPBBits {
        static constexpr uint8_t DATA_BIT = 4;
        uint8_t _ : 4, data : 1, clk : 1;
    };
#include <shared/popwarn.h>

    union SystemVIAPB {
        uint8_t value;
        SystemVIAPBBits bits;
        BSystemVIAPBBits b_bits;
        Master128SystemVIAPBBits m128_bits;
        MasterCompactSystemVIAPBBits mcompact_bits;
    };

#include <shared/pushwarn_bitfields.h>
    struct BAddressableLatchBits {
        uint8_t _ : 1, speech_read : 1, speech_write : 1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct AddressableLatchBits {
        uint8_t not_sound_write : 1, _ : 2, not_kb_write : 1, screen_base : 2, caps_lock_led : 1, shift_lock_led : 1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct Master128AddressableLatchBits {
        uint8_t _ : 1, rtc_read : 1, rtc_data_strobe : 1;
    };
#include <shared/popwarn.h>

    union AddressableLatch {
        uint8_t value;
        AddressableLatchBits bits;
        BAddressableLatchBits b_bits;
        Master128AddressableLatchBits m128_bits;
    };

    struct AMXMouseDataBits {
        uint8_t x : 1;
        uint8_t _1 : 1;
        uint8_t y : 1;
        uint8_t _2 : 2;
        uint8_t l : 1;
        uint8_t m : 1;
        uint8_t r : 1;
    };
    CHECK_SIZEOF(AMXMouseDataBits, 1);

    struct CompactMouseDataBits {
        uint8_t l : 1;
        uint8_t m : 1;
        uint8_t r : 1;
        uint8_t x : 1;
        uint8_t y : 1;
    };
    CHECK_SIZEOF(CompactMouseDataBits, 1);

    union MouseData {
        uint8_t value;
        AMXMouseDataBits amx_bits;
        CompactMouseDataBits compact_bits;
    };
    CHECK_SIZEOF(MouseData, 1);

    struct DiscDrive {
        bool motor = false;
        uint8_t track = 0;
        int step_sound_index = -1;

        DiscDriveSound seek_sound = DiscDriveSound_EndValue;
        size_t seek_sound_index = 0;

        DiscDriveSound spin_sound = DiscDriveSound_EndValue;
        size_t spin_sound_index = 0;

        float noise = 0.f;
        std::shared_ptr<DiscImage> disc_image;
        bool is_write_protected = false;
    };

    // Somewhere better for this? Maybe?
    //
    // Not coincidentally, the layout for this is identical to the
    // ADJI/Slogger/First Byte interface hardware.
    struct DigitalJoystickInputBits {
        bool up : 1;
        bool down : 1;
        bool left : 1;
        bool right : 1;
        bool fire0 : 1;
        bool fire1 : 1;
    };

    union DigitalJoystickInput {
        uint8_t value;
        DigitalJoystickInputBits bits;
    };

    // There's deliberately not much you can publicly do with one of these
    // other than default-construct it or copy it.
    explicit BBCMicroState() = delete;
    explicit BBCMicroState(const BBCMicroState &) = default;
    BBCMicroState &operator=(const BBCMicroState &) = delete;
    explicit BBCMicroState(BBCMicroState &&) = delete;
    BBCMicroState &operator=(BBCMicroState &&) = delete;

#if BBCMICRO_DEBUGGER
    const M6502 *DebugGetM6502(uint32_t dso) const;
    const ExtMem *DebugGetExtMem() const;
    const MC146818 *DebugGetRTC() const;
    const Tube *DebugGetTube() const;
    const ADC *DebugGetADC() const;
    const PCD8572 *DebugGetEEPROM() const;
    uint64_t DebugGetCPUCycless(uint32_t dso, CycleCount n) const;
    int DebugGetADJIDIPSwitches() const;
    uint8_t DebugGetMouseButtons() const;
#endif

    std::shared_ptr<const DiscImage> GetDiscImage(int drive) const;

    std::shared_ptr<const BBCMicroType> type;

    const uint32_t init_flags = 0;

    const BBCMicroParasiteType parasite_type = BBCMicroParasiteType_None;

    // 6845
    CRTC crtc;

  protected:
    CRTC::Output crtc_last_output = {};

  public:
    // Video output
    VideoULA video_ula;

    SAA5050 saa5050;

  protected:
    uint8_t ic15_byte = 0;

    // 0x8000 to display shadow RAM; 0x0000 to display normal RAM.
    uint16_t shadow_select_mask = 0x0000;
    uint8_t cursor_pattern = 0;

  public:
    SN76489 sn76489;

    // Number of emulated system cycles elapsed. Used to regulate sound
    // output and measure (for informational purposes) time between vsyncs.
    CycleCount cycle_count = {0};

    // Addressable latch.
    AddressableLatch addressable_latch = {0xff};

  protected:
    // Previous values, for detecting edge transitions.
    AddressableLatch old_addressable_latch = {0xff};

  public:
    M6502 cpu = {};

  protected:
    uint8_t stretch = 0;
    bool resetting = false;

  public:
    R6522 system_via;

  protected:
    SystemVIAPB old_system_via_pb;
    uint8_t system_via_irq_pending = 0;

  public:
    R6522 user_via;

  protected:
    uint8_t user_via_irq_pending = 0;

  public:
    PagingState paging;

    // Key states
    uint8_t key_columns[16] = {};
    uint8_t key_scan_column = 0;

  protected:
    int num_keys_down = 0;
    //BeebKey auto_reset_key=BeebKey_None;

    // Disk stuff
    const DiscInterface *const disc_interface = nullptr;
    WD1770 fdc;
    DiscInterfaceControl disc_control = {};
    DiscDrive drives[NUM_DRIVES];
    std::shared_ptr<DiscInterfaceExtraHardwareState> disc_interface_extra_hardware;

    // RTC
    MC146818 rtc;

    // EEPROM
    PCD8572 eeprom;

    // ADC
    ADC adc;

  public:
    DigitalJoystickInput digital_joystick_state = {};
    MouseData mouse_data = {0xff};
    uint8_t mouse_signal_x = 0;
    uint8_t mouse_signal_y = 0;
    int mouse_dx = 0;
    int mouse_dy = 0;

  protected:
    // Parallel printer
    bool printer_enabled = false;
    uint16_t printer_busy_counter = 0;

    // Joystick states.
    //
    // Suitable for ORing into the PB value - only the two joystick button
    // bits are ever set (indicating button unpressed).
    uint8_t not_joystick_buttons = 1 << 4 | 1 << 5;

  public:
    uint16_t analogue_channel_values[4] = {};

  protected:
    // External 1MHz bus RAM.
    ExtMem ext_mem;

    CycleCount last_vsync_cycle_count = {0};
    CycleCount last_frame_cycle_count = {0};

    std::shared_ptr<std::vector<uint8_t>> ram_buffer;

    std::shared_ptr<const std::array<uint8_t, 16384>> os_buffer;
    std::shared_ptr<const std::vector<uint8_t>> sideways_rom_buffers[16];
    // Each element is either a copy of the ROMData contents, or null.
    std::shared_ptr<std::array<uint8_t, 16384>> sideways_ram_buffers[16];

    // Combination of BBCMicroHackFlag.
    uint32_t hack_flags = 0;

    // Current paste data, if any.
    //
    // (This has to be part of the BBCMicro state - suppose a
    // state is saved mid-paste. The initiating event is in the
    // past, and won't be included in the replay data, but when
    // starting a replay from that state then the rest of the
    // paste needs to be performed.)
    BBCMicroPasteState paste_state = BBCMicroPasteState_None;
    std::shared_ptr<const std::string> paste_text;
    size_t paste_index = 0;
    uint64_t paste_wait_end = 0;

    // Tube stuff.
    bool parasite_accessible = false;

    M6502 parasite_cpu = {};

  protected:
    // Disallow values of base type. Disallow delete of pointer to base type.
    ~BBCMicroState() = default;

  protected:
    std::shared_ptr<const std::array<uint8_t, 2048>> parasite_rom_buffer;
    std::shared_ptr<std::vector<uint8_t>> parasite_ram_buffer;
    bool parasite_boot_mode = true;
    Tube parasite_tube;

    explicit BBCMicroState(std::shared_ptr<const BBCMicroType> type,
                           const DiscInterface *disc_interface,
                           BBCMicroParasiteType parasite_type,
                           const std::vector<uint8_t> &nvram_contents,
                           uint32_t init_flags,
                           const tm *rtc_time,
                           CycleCount initial_cycle_count);

    friend class BBCMicro;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A unique state has its own copy of all the shared_ptr'd buffers and all the
// Handler and Trace pointers (etc.) are valid.

class BBCMicroUniqueState : public BBCMicroState {
  public:
    using BBCMicroState::BBCMicroState;

    explicit BBCMicroUniqueState(const BBCMicroUniqueState &src);
    ~BBCMicroUniqueState() = default;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// A read-only state is potentially a simple value copy of an existing state,
// possibly itself a read-only one too. Read-only states may share buffers with
// other states, so the following apply:
//
// - the various ROM and RAM shared_ptr'd buffers can refer to the same buffers
//   as the original state. They may be out of sync relative to the rest of the
//   hardware state!
// - any hardware Handler or Trace pointers (etc.) are not necessarily valid
//
// This means that a read-only state is kind of useless! They're there purely
// for the debugger to use for updating its UI without having to have each
// window take a lock (or have the debugger copy all of BBC memory).
//
// The potential discrepancy between RAM contents and hardware state doesn't
// matter; when the BBC is running, it's impossible to tell, and when it's
// stopped, the two are actually in sync.
//
// (Any actual modification to the BBCMicro state is done via BeebThread
// messages.)

class BBCMicroReadOnlyState : public BBCMicroState {
  public:
    using BBCMicroState::BBCMicroState;

    BBCMicroReadOnlyState(const BBCMicroUniqueState &src);
    ~BBCMicroReadOnlyState() = default;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
