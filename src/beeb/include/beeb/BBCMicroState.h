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

#include <shared/enum_decl.h>
#include "BBCMicroState.inl"
#include <shared/enum_end.h>

class DiscImage;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// All the shared_ptr<vector<uint8_t>> point to vectors that are never
// resized once created. So in principle it's safe(ish) to call size() and
// operator[] from multiple threads without bothering to lock.
//
// The ExtMem has a shared_ptr<vector<uint8_t>> inside it, and that follows
// the same rules.
//
// This will get improved.
struct BBCMicroState {
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

    struct DiscDrive {
        bool motor = false;
        uint8_t track = 0;
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        int step_sound_index = -1;

        DiscDriveSound seek_sound = DiscDriveSound_EndValue;
        size_t seek_sound_index = 0;

        DiscDriveSound spin_sound = DiscDriveSound_EndValue;
        size_t spin_sound_index = 0;

        float noise = 0.f;
#endif
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
    explicit BBCMicroState() = default;
    explicit BBCMicroState(const BBCMicroState &) = default;
    BBCMicroState &operator=(const BBCMicroState &) = default;

    const M6502 *DebugGetM6502(uint32_t dso) const;
    const ExtMem *DebugGetExtMem() const;
    const MC146818 *DebugGetRTC() const;
    const Tube *DebugGetTube() const;
    const ADC *DebugGetADC() const;
    const PCD8572 *DebugGetEEPROM() const;

    int DebugGetADJIDIPSwitches() const;

    const BBCMicroType *type = nullptr;

  private:
    uint32_t init_flags = 0;

  public:
    BBCMicroParasiteType parasite_type = BBCMicroParasiteType_None;

    // 6845
    CRTC crtc;

  private:
    CRTC::Output crtc_last_output = {};

  public:
    // Video output
    VideoULA video_ula;

    SAA5050 saa5050;

  private:
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

  private:
    // Previous values, for detecting edge transitions.
    AddressableLatch old_addressable_latch = {0xff};

  public:
    M6502 cpu = {};

  private:
    uint8_t stretch = 0;
    bool resetting = false;

  public:
    R6522 system_via;

  private:
    SystemVIAPB old_system_via_pb;
    uint8_t system_via_irq_pending = 0;

  public:
    R6522 user_via;

  private:
    uint8_t user_via_irq_pending = 0;

  public:
    ROMSEL romsel = {};
    ACCCON acccon = {};

    // Key states
    uint8_t key_columns[16] = {};
    uint8_t key_scan_column = 0;

  private:
    int num_keys_down = 0;
    //BeebKey auto_reset_key=BeebKey_None;

    // Disk stuff
    const DiscInterface *const disc_interface = nullptr;
    WD1770 fdc;
    DiscInterfaceControl disc_control = {};
    DiscDrive drives[NUM_DRIVES];
    DiscInterfaceExtraHardwareState *disc_interface_extra_hardware = nullptr;

    // RTC
    MC146818 rtc;

    // EEPROM
    PCD8572 eeprom;

    // ADC
    ADC adc;

  public:
    DigitalJoystickInput digital_joystick_state = {};

  private:
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

  private:
    // External 1MHz bus RAM.
    ExtMem ext_mem;

    CycleCount last_vsync_cycle_count = {0};
    CycleCount last_frame_cycle_count = {0};

    std::shared_ptr<std::vector<uint8_t>> ram_buffer;

    std::shared_ptr<const std::array<uint8_t, 16384>> os_buffer;
    std::shared_ptr<const std::array<uint8_t, 16384>> sideways_rom_buffers[16];
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

  private:
    std::shared_ptr<const std::array<uint8_t, 2048>> parasite_rom_buffer;
    std::shared_ptr<std::vector<uint8_t>> parasite_ram_buffer;
    bool parasite_boot_mode = true;
    Tube parasite_tube;

    explicit BBCMicroState(const BBCMicroType *type,
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

#endif
