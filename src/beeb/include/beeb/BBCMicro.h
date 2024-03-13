#ifndef HEADER_5515E213300440068BB90FD401AD3AA2
#define HEADER_5515E213300440068BB90FD401AD3AA2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ROM;
struct DiscDriveCallbacks;
class Log;
struct VideoDataUnit;
struct SoundDataUnit;
struct DiscInterfaceDef;
class Trace;
struct TraceStats;
class TraceEventType;
class DiscImage;
class BeebLinkHandler;
class BeebLink;

#include <array>
#include <memory>
#include <vector>
#include "conf.h"
#include "crtc.h"
#include "ExtMem.h"
#include "1770.h"
#include "6522.h"
#include "6502.h"
#include "SN76489.h"
#include "MC146818.h"
#include "VideoULA.h"
#include "teletext.h"
#include "DiscInterface.h"
#include <time.h>
#include "keys.h"
#include "video.h"
#include "type.h"
#include "tube.h"
#include "BBCMicroParasiteType.h"
#include "adc.h"
#include "PCD8572.h"

#include <shared/enum_decl.h>
#include "BBCMicro.inl"
#include <shared/enum_end.h>

#define BBCMicroLEDFlags_AllDrives (255u * BBCMicroLEDFlag_Drive0)

constexpr uint32_t GetNormalizedBBCMicroUpdateFlags(uint32_t flags);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BBCMicro : private WD1770Handler,
                 private ADCHandler {
  public:
    struct State;
    static const uint16_t SCREEN_WRAP_ADJUSTMENTS[];

    static const uint16_t ADJI_ADDRESSES[4];

    typedef void (*UpdateROMSELPagesFn)(BBCMicro *);
    typedef void (*UpdateACCCONPagesFn)(BBCMicro *, const ACCCON *);

    // BIG_PAGE_SIZE_BYTES fits into a uint16_t.
    static constexpr size_t BIG_PAGE_SIZE_BYTES = 4096;
    static constexpr size_t BIG_PAGE_OFFSET_MASK = 4095;

    static constexpr size_t BIG_PAGE_SIZE_PAGES = BIG_PAGE_SIZE_BYTES / 256u;

#if BBCMICRO_DEBUGGER
    struct HardwareDebugState {
        R6522::IRQ system_via_irq_breakpoints = {};
        R6522::IRQ user_via_irq_breakpoints = {};
    };

    struct DebugState {
        static const uint16_t INVALID_PAGE_INDEX = 0xffff;

        struct Breakpoint {
            uint64_t id = 0;
        };

        bool is_halted = false;

        BBCMicroStepType step_type = BBCMicroStepType_None;
        const M6502 *step_cpu = nullptr;

        HardwareDebugState hw;

        // No attempt made to minimize this stuff... it doesn't go into
        // the saved states, so whatever.

        // Byte-specific breakpoint flags.
        uint8_t big_pages_byte_debug_flags[NUM_BIG_PAGES][BIG_PAGE_SIZE_BYTES] = {};

        //
        uint64_t breakpoints_changed_counter = 1;

        // List of all addresses that have address breakpoints associated with
        // them.
        std::vector<M6502Word> addr_breakpoints;

        // List of temp execute breakpoints to be reset on a halt. Each entry is
        // a pointer to one of the bytes in big_pages_debug_flags or
        // address_debug_flags.
        //
        // Entries are added to this list, but not removed - there's not really
        // much point.
        std::vector<uint8_t *> temp_execute_breakpoints;

        // Host address-specific breakpoint flags.
        uint8_t host_address_debug_flags[65536] = {};

        // Parasite address-specific breakpoint flags.
        //
        // (This buffer exists even if the parasite is disabled. 64 KB just
        // isn't enough to worry about any more.)
        uint8_t parasite_address_debug_flags[65536] = {};

        char halt_reason[1000] = {};
    };
#endif

    // This is just to keep things regular. There aren't currently any big
    // pages actually of this type.
    static const BigPageType IO_BIG_PAGE_TYPE;

    struct BigPage {
        // if non-NULL, points to BIG_PAGE_SIZE_BYTES bytes. NULL if this
        // big page isn't readable.
        const uint8_t *r = nullptr;

        // if non-NULL, points to BIG_PAGE_SIZE_BYTES bytes. NULL if this
        // big page isn't writeable.
        uint8_t *w = nullptr;

#if BBCMICRO_DEBUGGER
        // if non-NULL, points to BIG_PAGE_SIZE_BYTES values. NULL if this
        // BBCMicro has no associated DebugState.
        uint8_t *byte_debug_flags = nullptr;

        // if non-NULL, points to BIG_PAGE_SIZE_BYTES values. NULL if this
        // BBCMicro has no associated DebugState.
        //
        // The address flags are per-address, so multiple BigPage structs may
        // point to the same set of address flags.
        uint8_t *address_debug_flags = nullptr;
#endif

        // Index of this big page, from 0 (inclusive) to NUM_BIG_PAGES
        // (exclusive).
        uint8_t index = 0;

        const BigPageMetadata *metadata = nullptr;
    };

    // Same, but for reading only.
    struct ReadOnlyBigPage {
        const uint8_t *r = nullptr;

        // set if memory is actually writeable - as there's no w field to
        // indicate this.
        bool writeable = false;

#if BBCMICRO_DEBUGGER
        const uint8_t *byte_debug_flags = nullptr;
        const uint8_t *address_debug_flags = nullptr;
#endif
        uint8_t index = 0;
        const BigPageMetadata *metadata = nullptr;
    };

    // nvram_contents and rtc_time are ignored if the BBCMicro doesn't
    // support such things.
    BBCMicro(const BBCMicroType *type,
             const DiscInterfaceDef *def,
             BBCMicroParasiteType parasite_type,
             const std::vector<uint8_t> &nvram_contents,
             const tm *rtc_time,
             uint32_t init_flags,
             BeebLinkHandler *beeblink_handler,
             CycleCount initial_cycle_count);

  protected:
    BBCMicro(const BBCMicro &src);

  public:
    ~BBCMicro();

    // result is a combination of BBCMicroCloneImpediment.
    uint32_t GetCloneImpediments() const;

    std::unique_ptr<BBCMicro> Clone() const;

    //typedef std::array<uint8_t, 16384> ROMData;

#if BBCMICRO_TRACE
#include <shared/pshpack1.h>
    struct InstructionTraceEvent {
        uint8_t a, x, y, p, data, opcode, s;
        uint16_t pc, ad, ia;
    };
#include <shared/poppack.h>

    static const TraceEventType INSTRUCTION_EVENT;
#endif

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

    // w[i], r[i] and debug[i] point to the corresponding members of bp[i].
    //
    // These tables are SoA, rather than AoS like BigPage, as they get used
    // all the time and so the 8-byte stride won't hurt - though it is
    // probably marginal.
    struct MemoryBigPages {
        uint8_t *w[16] = {};
        const uint8_t *r[16] = {};
#if BBCMICRO_DEBUGGER
        uint8_t *byte_debug_flags[16] = {};
        const BigPage *bp[16] = {};
#endif
    };

    typedef uint8_t (*ReadMMIOFn)(void *, M6502Word);
    struct ReadMMIO {
        ReadMMIOFn fn;
        void *context;
    };

    typedef void (*WriteMMIOFn)(void *, M6502Word, uint8_t value);
    struct WriteMMIO {
        WriteMMIOFn fn;
        void *context;
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

    // Called after an opcode fetch and before execution.
    //
    // cpu->pc.w is PC+1; cpu->pc.dbus is the opcode fetched.
    //
    // Return true to keep the callback, or false to remove it.
    typedef bool (*InstructionFn)(const BBCMicro *m, const M6502 *cpu, void *context);

    // Called when an address is about to be written.
    typedef bool (*WriteFn)(const BBCMicro *m, const M6502 *cpu, void *context);

    const BBCMicroType *GetType() const;
    BBCMicroParasiteType GetParasiteType() const;

    // Get read-only pointer to the cycle counter. The pointer is never
    // NULL and remains valid for the lifetime of the BBCMicro.
    //
    // (This is supposed to be impossible to get wrong (like a getter) but
    // also cheap enough in a debug build (unlike a getter) that I don't
    // have to worry about that.)
    const CycleCount *GetCycleCountPtr() const;

    static uint64_t Get3MHzCycleCount(CycleCount n);

    uint8_t GetKeyState(BeebKey key);

    // Read a value from memory. The read takes place as if the PC were in
    // zero page. There are no side-effects and reads from memory-mapped
    // I/O return 0x00.
    //uint8_t ReadMemory(uint16_t address);

    // Return pointer to base of BBC RAM. Get the size of RAM from
    // GetTypeInfo(m)->ram_size.
    const uint8_t *GetRAM() const;

    // Set key state. If the key is Break, handle the reset line
    // appropriately.
    //
    // Returns true if the key state changed.
    bool SetKeyState(BeebKey key, bool new_state);

    // Get/set joystick button state.
    bool GetJoystickButtonState(uint8_t index) const;
    void SetJoystickButtonState(uint8_t index, bool state);

    bool HasNumericKeypad() const;

#if BBCMICRO_DEBUGGER
    //bool GetTeletextDebug() const;
    void SetTeletextDebug(bool teletext_debug);
#endif

    // Result is a combination of BBCMicroUpdateResultFlag values.
    uint32_t Update(VideoDataUnit *video_unit, SoundDataUnit *sound_unit) {
        return (this->*m_update_mfn)(video_unit, sound_unit);
    }

    static uint32_t GetNormalizedBBCMicroUpdateFlags(uint32_t flags);

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    // The disc drive sounds are used by all BBCMicro objects created
    // after they're set.
    //
    // Once a particular sound is set, it can't be changed.
    static void SetDiscDriveSound(DiscDriveType type, DiscDriveSound sound, std::vector<float> samples);
#endif

    uint32_t GetLEDs();

    std::vector<uint8_t> GetNVRAM() const;

    // The shared_ptr is copied.
    void SetOSROM(std::shared_ptr<const std::array<uint8_t, 16384>> data);

    // The shared_ptr is copied.
    void SetSidewaysROM(uint8_t bank, std::shared_ptr<const std::array<uint8_t, 16384>> data);

    // The ROM data is copied.
    void SetSidewaysRAM(uint8_t bank, std::shared_ptr<const std::array<uint8_t, 16384>> data);

    // The shared_ptr is copied.
    void SetParasiteOS(std::shared_ptr<const std::array<uint8_t, 2048>> data);

#if BBCMICRO_TRACE
    /* Allocates a new trace (replacing any existing one) and sets it
    * going. */
    void StartTrace(uint32_t trace_flags, size_t max_num_bytes);

    /* If there's a trace, stops it. If *old_trace_ptr, set *old_trace_ptr to
     * old Trace, if there was one.
     */
    void StopTrace(std::shared_ptr<Trace> *old_trace_ptr);

    int GetTraceStats(struct TraceStats *stats);
#endif

    // Add host CPU instruction/host CPU write callback. It's an error to add
    // the same one twice.
    //
    // To remove, have the callback return false, or use RemoveInstructionFn
    // (providing both fn and context).
    //
    // The callback mustn't affect reproducability.
    void AddHostInstructionFn(InstructionFn fn, void *context);
    void RemoveHostInstructionFn(InstructionFn fn, void *context);

    void AddHostWriteFn(WriteFn fn, void *context);

    // Set SHEILA IO functions.
    void SetSIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context);

    // Set external FRED/JIM IO functions.
    void SetXFJIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context);

    // Set internal FRED/JIM IO functions (Master 128 only).
    void SetIFJIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context);

    // The pointer is moved into the result.
    std::shared_ptr<DiscImage> TakeDiscImage(int drive);
    std::shared_ptr<const DiscImage> GetDiscImage(int drive) const;

    // Setting the disk image makes the drive non write-protected.
    void SetDiscImage(int drive,
                      std::shared_ptr<DiscImage> disc_image);

    void SetDriveWriteProtected(int drive, bool is_write_protected);
    bool IsDriveWriteProtected(int drive) const;

    // Every time there is a disc access, the disc access flag is set
    // to true. This call retrieves its current value, and sets it to
    // false.
    bool GetAndResetDiscAccessFlag();

    static const BeebKey PASTE_START_KEY;
    static const char PASTE_START_CHAR;

    bool IsPasting() const;
    void StartPaste(std::shared_ptr<const std::string> text);
    void StopPaste();

    std::shared_ptr<const BBCMicro::State> DebugGetState() const;

    const M6502 *GetM6502() const;

#if BBCMICRO_DEBUGGER
    // Given address, return the BigPage for the memory there, taking
    // the debug page overrides into account.
    //
    // mos indicates whether the access is from MOS code (true) or user code
    // (false).
    const BigPage *DebugGetBigPageForAddress(M6502Word addr, bool mos, uint32_t dso) const;
    static void DebugGetBigPageForAddress(ReadOnlyBigPage *bp,
                                          const State *state,
                                          const DebugState *debug_state,
                                          M6502Word addr,
                                          bool mos,
                                          uint32_t dso);

    static void DebugGetMemBigPageIsMOSTable(uint8_t *mem_big_page_is_mos, const State *state, uint32_t dso);

    // Get/set per-byte debug flags for one byte.
    uint8_t DebugGetByteDebugFlags(const BigPage *big_page, uint32_t offset) const;
    void DebugSetByteDebugFlags(uint8_t big_page_index, uint32_t offset, uint8_t flags);

    // Returns pointer to per-address debug flags for the entire given mem big
    // page.
    const uint8_t *DebugGetAddressDebugFlagsForMemBigPage(uint8_t mem_big_page) const;

    // Get/set per-address byte debug flags for one address.
    uint8_t DebugGetAddressDebugFlags(M6502Word addr, uint32_t dso) const;
    void DebugSetAddressDebugFlags(M6502Word addr, uint32_t dso, uint8_t flags) const;

    void DebugGetBytes(uint8_t *bytes, size_t num_bytes, M6502Word addr, uint32_t dso);
    void DebugSetBytes(M6502Word addr, uint32_t dso, const uint8_t *bytes, size_t num_bytes);

    void SetExtMemory(uint32_t addr, uint8_t value);

    void DebugHalt(const char *fmt, ...) PRINTF_LIKE(2, 3);

    inline bool DebugIsHalted() const {
        return m_debug_is_halted;
    }

    const char *DebugGetHaltReason() const;

    void DebugRun();

    void DebugStepOver(uint32_t dso);
    void DebugStepIn(uint32_t dso);

    bool HasDebugState() const;
    std::shared_ptr<DebugState> TakeDebugState();
    std::shared_ptr<const DebugState> GetDebugState() const;
    void SetDebugState(std::shared_ptr<DebugState> debug);

    HardwareDebugState GetHardwareDebugState() const;
    void SetHardwareDebugState(const HardwareDebugState &hw);

    static uint32_t DebugGetCurrentStateOverride(const State *state);

    // The breakpoints change counter is incremented any time the set of
    // breakpoints changes.
    uint64_t DebugGetBreakpointsChangeCounter() const;

    // Get copies of the debug flags.
    //
    // host_address_debug_flags data is 65536 bytes.
    //
    // parasite_address_debug_flags data is 65536 bytes.
    //
    // big_pages_debug_flags data is NUM_BIG_PAGES*BIG_PAGE_SIZE_BYTES.
    //void DebugGetDebugFlags(uint8_t *host_address_debug_flags,
    //                        uint8_t *parasite_address_debug_flags,
    //                        uint8_t *big_pages_debug_flags) const;
#endif

    void SendBeebLinkResponse(std::vector<uint8_t> data);

    uint16_t GetAnalogueChannel(uint8_t channel) const;
    void SetAnalogueChannel(uint8_t channel, uint16_t value);

    DigitalJoystickInput GetDigitalJoystickState(uint8_t index) const;
    void SetDigitalJoystickState(uint8_t index, DigitalJoystickInput state);

    static void PrintInfo(Log *log);

    // When the printer is enabled, printer output will be captured into a
    // buffer.
    void SetPrinterEnabled(bool is_printer_enabled);

    // Overly simplistic mechanism?
    void SetPrinterBuffer(std::vector<uint8_t> *buffer);

    bool HasADC() const;

  protected:
    // Hacks, not part of the public API, for use by the testing stuff so that
    // it can run even when the debugger isn't compiled in.

    // ram_buffer_index is an index into the state's ram buffer, not a proper
    // BBC Micro address.
    void TestSetByte(uint16_t ram_buffer_index, uint8_t value);

    void TestSetParasiteByte(uint16_t address, uint8_t value);

    // Use from inside an InstructionFn. Sets the data bus to 0x60, so that
    // it appears an RTS was fetched.
    //
    // CPU must be about to execute.
    void TestRTS();

  private:
    struct M6502Metadata {
        const char *name = nullptr;
#if BBCMICRO_DEBUGGER
        uint32_t dso = 0;
        uint32_t debug_step_update_flag = 0;
#endif
    };

  public: //TODO rationalize this a bit
    // All the shared_ptr<vector<uint8_t>> point to vectors that are never
    // resized once created. So in principle it's safe(ish) to call size() and
    // operator[] from multiple threads without bothering to lock.
    //
    // The ExtMem has a shared_ptr<vector<uint8_t>> inside it, and that follows
    // the same rules.
    //
    // This will get improved.
    struct State {
        // There's deliberately not much you can publicly do with one of these
        // other than default-construct it or copy it.
        explicit State() = default;
        explicit State(const State &) = default;
        State &operator=(const State &) = default;

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

      private:
        // Key states
        uint8_t key_columns[16] = {};
        uint8_t key_scan_column = 0;
        int num_keys_down = 0;
        //BeebKey auto_reset_key=BeebKey_None;

        // Disk stuff
        WD1770 fdc;
        DiscInterfaceControl disc_control = {};
        DiscDrive drives[NUM_DRIVES];

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

        explicit State(const BBCMicroType *type,
                       BBCMicroParasiteType parasite_type,
                       const std::vector<uint8_t> &nvram_contents,
                       uint32_t init_flags,
                       const tm *rtc_time,
                       CycleCount initial_cycle_count);

        friend class BBCMicro;
    };

  private:
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that's mostly copyable using copy ctor. A few things do
    // require minor fixups afterwards.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    State m_state;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that is copied, but needs special handling.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    DiscInterface *const m_disc_interface = nullptr;
    std::shared_ptr<DiscImage> m_disc_images[NUM_DRIVES];
    bool m_is_drive_write_protected[NUM_DRIVES] = {};
    //const bool m_video_nula;
    //const bool m_ext_mem;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that can be derived from m_state or the non-copyable stuff.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    BigPage m_big_pages[NUM_BIG_PAGES];
    typedef uint32_t (BBCMicro::*UpdateMFn)(VideoDataUnit *, SoundDataUnit *);
    UpdateMFn m_update_mfn = nullptr;

    // Memory
    MemoryBigPages m_mem_big_pages[2] = {};
    const MemoryBigPages *m_pc_mem_big_pages[16];

    // Each points to 768 entries, one per byte. [0x000...0x0ff] is for page FC,
    // [0x100...0x1ff] for FD and [0x200...0x2ff] for FE.
    const ReadMMIO *m_read_mmios = nullptr;
    const uint8_t *m_read_mmios_stretch = nullptr;
    const WriteMMIO *m_write_mmios = nullptr;
    const uint8_t *m_write_mmios_stretch = nullptr;

    static std::vector<ReadMMIO> BBCMicro::*ms_read_mmios_mptrs[];
    static std::vector<uint8_t> BBCMicro::*ms_read_mmios_stretch_mptrs[];
    static std::vector<WriteMMIO> BBCMicro::*ms_write_mmios_mptrs[];
    static std::vector<uint8_t> BBCMicro::*ms_write_mmios_stretch_mptrs[];

    // Tables for pages FC/FD/FE that access the hardware - B, B+, M128 when
    // ACCCON TST=0.
    std::vector<ReadMMIO> m_read_mmios_hw;
    std::vector<WriteMMIO> m_write_mmios_hw;
    std::vector<uint8_t> m_mmios_stretch_hw;

    // Tables for pages FC/FD/FE that access cartridge hardware - M128 when
    // ACCON IFJ=1.
    std::vector<ReadMMIO> m_read_mmios_hw_cartridge;
    std::vector<WriteMMIO> m_write_mmios_hw_cartridge;
    std::vector<uint8_t> m_mmios_stretch_hw_cartridge;

    // Tables for pages FC/FD/FE that access the ROM - reads on M128 when ACCCON
    // TST=1.
    std::vector<ReadMMIO> m_read_mmios_rom;
    std::vector<uint8_t> m_mmios_stretch_rom;

    //// Whether memory-mapped I/O reads currently access ROM or not.
    //bool m_rom_mmio = false;

    //// Whether IFJ mode is currently on or not.
    //bool m_ifj = false;

    uint8_t m_romsel_mask = 0;
    uint8_t m_acccon_mask = 0;

    // teletext_bases[0] is used when 6845 address bit 11 is clear;
    // teletext_bases[1] when it's set.
    uint16_t m_teletext_bases[2] = {};

    uint8_t *m_ram = nullptr;

    uint8_t *m_parasite_ram = nullptr;
    ReadMMIOFn m_parasite_read_mmio_fns[8] = {};
    WriteMMIOFn m_parasite_write_mmio_fns[8] = {};

    const std::vector<float> *m_disc_drive_sounds[DiscDriveSound_EndValue];

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that isn't copied.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    M6502Metadata m_host_cpu_metadata;
    M6502Metadata m_parasite_cpu_metadata;

    // This doesn't need to be copied. The event list records its
    // influence.
    bool m_disc_access = false;

#if VIDEO_TRACK_METADATA
    // This doesn't need to be copied. If it becomes stale, it'll be
    // refreshed within 1 cycle...
    uint16_t m_last_video_access_address = 0;
#endif

#if BBCMICRO_TRACE
    // Event trace stuff.
    //
    // m_trace holds the result of m_trace_ptr.get(), in the interests
    // of non-terrible debug build performance.
    std::shared_ptr<Trace> m_trace_ptr;
    Trace *m_trace = nullptr;
    InstructionTraceEvent *m_trace_current_instruction = nullptr;
    uint32_t m_trace_flags = 0;
    InstructionTraceEvent *m_trace_parasite_current_instruction = nullptr;
#endif

    std::vector<std::pair<InstructionFn, void *>> m_host_instruction_fns;
    std::vector<std::pair<WriteFn, void *>> m_host_write_fns;

#if BBCMICRO_DEBUGGER
    std::shared_ptr<DebugState> m_debug_ptr;

    // try to avoid appalling debug build performance...
    DebugState *m_debug = nullptr;
    bool m_debug_is_halted = false;
#else
    static const bool m_debug_is_halted = false;
#endif

    // To avoid a lot of hassle, the state can't be saved while BeebLink is
    // active - so none of this stuff participates.
    BeebLinkHandler *m_beeblink_handler = nullptr;
    std::unique_ptr<BeebLink> m_beeblink;

    std::vector<uint8_t> *m_printer_buffer = nullptr;

    void InitStuff();
#if BBCMICRO_TRACE
    void SetTrace(std::shared_ptr<Trace> trace, uint32_t trace_flags);
#endif
    void UpdatePaging();
    void InitPaging();
    static void Write1770ControlRegister(void *m_, M6502Word a, uint8_t value);
    static uint8_t Read1770ControlRegister(void *m_, M6502Word a);
#if BBCMICRO_TRACE
    void TracePortB(SystemVIAPB pb);
#endif
    static void WriteUnmappedMMIO(void *m_, M6502Word a, uint8_t value);
    static uint8_t ReadUnmappedMMIO(void *m_, M6502Word a);
    static uint8_t ReadROMMMIO(void *m_, M6502Word a);
    static uint8_t ReadROMSEL(void *m_, M6502Word a);
    static void WriteROMSEL(void *m_, M6502Word a, uint8_t value);
    static uint8_t ReadACCCON(void *m_, M6502Word a);
    static void WriteACCCON(void *m_, M6502Word a, uint8_t value);
    static uint8_t ReadADJI(void *m_, M6502Word a);
#if BBCMICRO_DEBUGGER
    void UpdateDebugBigPages(MemoryBigPages *mem_big_pages);
    void UpdateDebugState();
    void SetDebugStepType(BBCMicroStepType step_type, const M6502 *step_cpu);
    void DebugHitBreakpoint(const M6502 *cpu, uint8_t flags);
    void DebugHandleStep();
    // Public for the debugger's benefit.
#endif
    static void InitReadOnlyBigPage(ReadOnlyBigPage *bp,
                                    const State *state,
#if BBCMICRO_DEBUGGER
                                    const DebugState *debug_state,
#endif
                                    uint8_t big_page_index);

    static void CheckMemoryBigPages(const MemoryBigPages *pages, bool non_null);

    // 1770 handler stuff.
    bool IsTrack0() override;
    void StepOut() override;
    void StepIn() override;
    void SpinUp() override;
    void SpinDown() override;
    bool IsWriteProtected() override;
    bool GetByte(uint8_t *value, uint8_t sector, size_t offset) override;
    bool SetByte(uint8_t sector, size_t offset, uint8_t value) override;
    bool GetSectorDetails(uint8_t *track, uint8_t *side, size_t *size, uint8_t sector, bool double_density) override;
    DiscDrive *GetDiscDrive();
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    void InitDiscDriveSounds(DiscDriveType type);
    void StepSound(DiscDrive *dd);
    float UpdateDiscDriveSound(DiscDrive *dd);
#endif
    void UpdateCPUDataBusFn();
    void SetMMIOFnsInternal(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context, bool set_xfj, bool set_ifj);

    static void WriteHostTube0Wrapper(void *context, M6502Word a, uint8_t value);

    // ADC handler stuff.
    uint16_t ReadAnalogueChannel(uint8_t channel) const override;

    template <uint32_t UPDATE_FLAGS>
    uint32_t UpdateTemplated(VideoDataUnit *video_unit, SoundDataUnit *sound_unit);

    static const uint8_t CURSOR_PATTERNS[8];
    static const UpdateMFn ms_update_mfns[4096];
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
