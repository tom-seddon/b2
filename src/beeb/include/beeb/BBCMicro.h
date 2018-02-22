#ifndef HEADER_5515E213300440068BB90FD401AD3AA2
#define HEADER_5515E213300440068BB90FD401AD3AA2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ROM;
struct DiscDriveCallbacks;
class Log;
union VideoDataUnit;
struct SoundDataUnit;
struct DiscInterfaceDef;
class Trace;
struct TraceStats;
class TraceEventType;
class DiscImage;

#include <array>
#include <memory>
#include <vector>
#include "conf.h"
#include "crtc.h"
#include "1770.h"
#include "6522.h"
#include <6502/6502.h>
#include "SN76489.h"
#include "MC146818.h"
#include "VideoULA.h"
#include "teletext.h"
#include "DiscInterface.h"
#include <shared/mutex.h>
#include <time.h>
#include "keys.h"
#include "video.h"

#include <shared/enum_decl.h>
#include "BBCMicro.inl"
#include <shared/enum_end.h>

#define BBCMicroLEDFlags_AllDrives (255u*BBCMicroLEDFlag_Drive0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BBCMicro:
    private WD1770Handler
{
public:
    static const uint16_t SCREEN_WRAP_ADJUSTMENTS[];

    union ACCCON;
    typedef void (*UpdateROMSELPagesFn)(BBCMicro *);
    typedef void (*UpdateACCCONPagesFn)(BBCMicro *,const ACCCON *);

#if BBCMICRO_DEBUGGER
    struct HardwareDebugState {
        R6522::IRQ system_via_irq_breakpoints={};
        R6522::IRQ user_via_irq_breakpoints={};
    };

    struct DebugState {
        static const uint16_t INVALID_PAGE_INDEX=0xffff;
        
        struct Breakpoint {
            uint64_t id=0;
        };

        struct ByteDebugFlagBits {
            uint8_t break_execute:1;
            uint8_t temp_execute:1;
            uint8_t break_read:1;
            uint8_t break_write:1;
        };

        union ByteDebugFlags {
            uint8_t value;
            ByteDebugFlagBits bits;
        };

        bool is_halted=false;
        BBCMicroStepType step_type=BBCMicroStepType_None;

        HardwareDebugState hw;

        static_assert(sizeof(ByteDebugFlags)==1,"");

        static const uint16_t NUM_DEBUG_FLAGS_PAGES=256+16*64+64;

        // No attempt made to minimize this stuff... it doesn't go into
        // the saved states, so whatever.
        ByteDebugFlags pages[NUM_DEBUG_FLAGS_PAGES][256]={};
        //Breakpoint *indexes[NUM_DEBUG_FLAGS_PAGES][256]={};

        std::vector<ByteDebugFlags *> temp_execute_breakpoints;
        //std::vector<Breakpoint *> breakpoints;

        char halt_reason[1000];
    };
#endif

    // nvram_contents and rtc_time are ignored if the BBCMicro doesn't
    // support such things.
    BBCMicro(BBCMicroType type,const DiscInterfaceDef *def,const std::vector<uint8_t> &nvram_contents,const tm *rtc_time,bool video_nula);
protected:
    BBCMicro(const BBCMicro &src);
public:
    ~BBCMicro();

    // The clone has no disc drive callbacks, and no disc access
    // mutex.
    std::unique_ptr<BBCMicro> Clone() const;

    typedef std::array<uint8_t,16384> ROMData;

#if BBCMICRO_TRACE
#include <shared/pshpack1.h>
    struct InstructionTraceEvent {
        uint8_t a,x,y,p,data,opcode,s;
        uint16_t pc,ad,ia;
    };
#include <shared/poppack.h>

    static const TraceEventType INSTRUCTION_EVENT;
#endif

#if BBCMICRO_TRACE
#include <shared/pshpack1.h>
    struct InitialTraceEvent {
        const struct M6502Config *config;
    };
#include <shared/poppack.h>

    static const TraceEventType INITIAL_EVENT;
#endif

#include <shared/pushwarn_bitfields.h>
    struct SystemVIAPBBits {
        uint8_t latch_index:3,latch_value:1,not_joystick0_fire:1,not_joystick1_fire:1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct BSystemVIAPBBits {
        uint8_t _:6,speech_ready:1,speech_interrupt:1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct Master128SystemVIAPBBits {
        uint8_t _:6,rtc_chip_select:1,rtc_address_strobe:1;
    };
#include <shared/popwarn.h>

    union SystemVIAPB {
        SystemVIAPBBits bits;
        BSystemVIAPBBits b_bits;
        Master128SystemVIAPBBits m128_bits;
        uint8_t value;
    };

#include <shared/pushwarn_bitfields.h>
    struct BAddressableLatchBits {
        uint8_t _:1,speech_read:1,speech_write:1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct AddressableLatchBits {
        uint8_t not_sound_write:1,_:2,not_kb_write:1,screen_base:2,caps_lock_led:1,shift_lock_led:1;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct Master128AddressableLatchBits {
        uint8_t _:1,rtc_read:1,rtc_data_strobe:1;
    };
#include <shared/popwarn.h>

    union AddressableLatch {
        AddressableLatchBits bits;
        BAddressableLatchBits b_bits;
        Master128AddressableLatchBits m128_bits;
        uint8_t value;
    };

    struct BROMSELBits {
        uint8_t pr:4,_:4;
    };

    struct BPlusROMSELBits {
        uint8_t pr:4,_:3,ram:1;
    };

    struct Master128ROMSELBits {
        uint8_t pm:4,_:3,ram:1;
    };

    union ROMSEL {
        uint8_t value;
        BROMSELBits b_bits;
        BPlusROMSELBits bplus_bits;
        Master128ROMSELBits m128_bits;
    };

    struct BPlusACCCONBits {
        uint8_t _:7,shadow:1;
    };

    struct Master128ACCCONBits {
        uint8_t d:1,e:1,x:1,y:1,itu:1,ifj:1,tst:1,irr:1;
    };

    union ACCCON {
        uint8_t value;
        BPlusACCCONBits bplus_bits;
        Master128ACCCONBits m128_bits;
    };

    struct MemoryPages {
        uint8_t *w[256]={};
        const uint8_t *r[256]={};
#if BBCMICRO_DEBUGGER
        DebugState::ByteDebugFlags *debug[256]={};
        uint16_t debug_page_index[256];
#endif

        MemoryPages();
    };

    typedef uint8_t (*ReadMMIOFn)(void *,union M6502Word);
    //struct ReadMMIO {
    //    ReadMMIOFn fn;
    //    void *obj;
    //};

    typedef void (*WriteMMIOFn)(void *,union M6502Word,uint8_t value);
    //struct WriteMMIO {
    //    WriteMMIOFn fn;
    //    void *obj;
    //};

    struct DiscDrive {
        bool motor=false;
        uint8_t track=0;
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        int step_sound_index=-1;

        DiscDriveSound seek_sound=DiscDriveSound_EndValue;
        size_t seek_sound_index=0;

        DiscDriveSound spin_sound=DiscDriveSound_EndValue;
        size_t spin_sound_index=0;

        float noise=0.f;
#endif
    };

    // Called after an opcode fetch and before execution.
    //
    // cpu->pc.w is PC+1; cpu->pc.dbus is the opcode fetched.
    //
    // Return true to keep the callback, or false to remove it.
    typedef bool (*InstructionFn)(const BBCMicro *m,const M6502 *cpu,void *context);

    BBCMicroType GetType() const;

    // Get read-only pointer to the cycle counter. The pointer is never
    // NULL and remains valid for the lifetime of the BBCMicro.
    //
    // (This is supposed to be impossible to get wrong (like a getter) but
    // also cheap enough in a debug build (unlike a getter) that I don't
    // have to worry about that.)
    const uint64_t *GetNum2MHzCycles() const;

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
    bool SetKeyState(BeebKey key,bool new_state);

    bool HasNumericKeypad() const;

#if BBCMICRO_DEBUGGER
    bool GetTeletextDebug() const;
    void SetTeletextDebug(bool teletext_debug);
#endif

    bool Update(VideoDataUnit *video_unit,SoundDataUnit *sound_unit);
    //void UpdateCycle0(VideoDataUnit *video_unit);
    //bool UpdateCycle1(VideoDataUnit *video_unit,SoundDataUnit *sound_unit);

    // *UNIT0 and *UNIT1 are always filled in. Returns true if
    // *SOUND_UNIT was filled in.
    //bool Update(VideoDataUnit *unit0,VideoDataUnit *unit1,SoundDataUnit *sound_unit);

#if BBCMICRO_TURBO_DISC
    bool GetTurboDisc();
    void SetTurboDisc(int turbo);
#endif

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    // The disc drive sounds are used by all BBCMicro objects created
    // after they're set.
    //
    // Once a particular sound is set, it can't be changed.
    static void SetDiscDriveSound(DiscDriveType type,DiscDriveSound sound,std::vector<float> samples);
#endif

    uint32_t GetLEDs();

    // Get the buffer size from GetNVRAMSize.
    const uint8_t *GetNVRAM() const;
    size_t GetNVRAMSize() const;

    // The shared_ptr is copied.
    void SetOSROM(std::shared_ptr<const ROMData> data);

    // The shared_ptr is copied.
    void SetSidewaysROM(uint8_t bank,std::shared_ptr<const ROMData> data);

    // The ROM data is copied.
    void SetSidewaysRAM(uint8_t bank,std::shared_ptr<const ROMData> data);

#if BBCMICRO_TRACE
    /* Allocates a new trace (replacing any existing one) and sets it
    * going. */
    void StartTrace(uint32_t trace_flags);

    /* If there's a trace, stops it, and returns a shared_ptr to it.
     * Otherwise, returns null. */
    std::shared_ptr<Trace> StopTrace();

    int GetTraceStats(struct TraceStats *stats);
#endif

    // Add instruction callback. It's an error to add the same one
    // twice.
    //
    // To remove, have the callback return false.
    //
    // The InstructionFn mustn't affect reproducability.
    void AddInstructionFn(InstructionFn fn,void *context);

    void SetMMIOFns(uint16_t addr,ReadMMIOFn read_fn,WriteMMIOFn write_fn,void *context);
    void SetMMIOCycleStretch(uint16_t addr,bool stretch);

    std::shared_ptr<DiscImage> GetMutableDiscImage(int drive);
    std::shared_ptr<const DiscImage> GetDiscImage(int drive) const;

    void SetDiscImage(int drive,std::shared_ptr<DiscImage> disc_image);

    // Every time there is a disc access, the disc access flag is set
    // to true. This call retrieves its current value, and sets it to
    // false.
    //
    // The disc mutex will be locked if there is one.
    bool GetAndResetDiscAccessFlag();

    static const char PASTE_START_CHAR;

    bool IsPasting() const;
    void StartPaste(std::shared_ptr<std::string> text);
    void StopPaste();

    const M6502 *GetM6502() const;

#if BBCMICRO_DEBUGGER
    uint16_t DebugGetBeebAddressFromCRTCAddress(uint8_t h,uint8_t l) const;

    // Hiding these behind accessors really doesn't buy much. Just
    // trying to avoid making the debug stuff a friend of this
    // class... though maybe it wouldn't matter?
    const CRTC *DebugGetCRTC() const;
    const VideoULA *DebugGetVideoULA() const;
    const AddressableLatch DebugGetAddressableLatch() const;
    const R6522 *DebugGetSystemVIA() const;
    const R6522 *DebugGetUserVIA() const;

    uint16_t DebugGetFlatPage(uint8_t page) const;

    void DebugCopyMemory(void *bytes_dest,DebugState::ByteDebugFlags *debug_dest,M6502Word addr_,uint16_t num_bytes) const;

    // Uses addressing scheme described in
    // http://mdfs.net/Docs/Comp/BBC/MemAddrs. Returns <0 if the
    // address isn't accessible.
    void DebugSetByte(uint32_t addr,uint8_t value);
    int DebugGetByte(uint32_t addr) const;

    void SetMemory(M6502Word addr,uint8_t value);

    void DebugHalt(const char *fmt,...) PRINTF_LIKE(2,3);

    bool DebugIsHalted() const;
    const char *DebugGetHaltReason() const;

    void DebugRun();

    DebugState::ByteDebugFlags DebugGetByteFlags(M6502Word addr) const;

    void DebugSetByteFlags(M6502Word addr,DebugState::ByteDebugFlags flags);

    // Temp breakpoints are automatically removed when the BBCMicro is
    // halted.
    void DebugAddTempBreakpoint(M6502Word addr);

    void DebugStepIn();

    static char DebugGetFlatPageCode(uint16_t flat_page);

    bool HasDebugState() const;
    std::unique_ptr<DebugState> TakeDebugState();
    void SetDebugState(std::unique_ptr<DebugState> debug);

    HardwareDebugState GetHardwareDebugState() const;
    void SetHardwareDebugState(const HardwareDebugState &hw);

    static const uint16_t INVALID_ASYNC_CALL_ADDRESS=0xffff;
    typedef void (*DebugAsyncCallFn)(bool called,void *context);

    // Set future call. The call will be made on exit from the next
    // IRQ routine (this being about as safe a place to put it as
    // any).
    //
    // The callback will be called with called=true when the call is
    // made, or with called=false if it doesn't happen in a timely
    // fashion.
    void DebugSetAsyncCall(uint16_t address,uint8_t a,uint8_t x,uint8_t y,bool c,DebugAsyncCallFn fn,void *context);
#endif
protected:
private:
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that's mostly copyable using copy ctor. A few things do
    // require minor fixups afterwards.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    struct State {
        // 6845
        CRTC crtc;
        CRTC::Output crtc_last_output={};

        // Video output
        VideoULA video_ula;
        SAA5050 saa5050;
        uint8_t saa5050_byte=0;

        // 0x8000 to display shadow RAM; 0x0000 to display normal RAM.
        uint16_t shadow_select_mask=0x0000;
        uint8_t cursor_pattern=0;

        SN76489 sn76489;

        // Number of emulated 2MHz cycles elapsed. Used to regulate sound
        // output and measure (for informational purposes) time between
        // vsyncs.
        uint64_t num_2MHz_cycles=0;

        // Addressable latch.
        AddressableLatch addressable_latch={};

        // Previous values, for detecting edge transitions.
        AddressableLatch old_addressable_latch={};

        M6502 cpu={};
        uint8_t stretched_cycles_left=0;
        bool resetting=false;

        R6522 system_via;
        R6522 user_via;

        ROMSEL romsel={};
        ACCCON acccon={};

        // Key states
        uint8_t key_columns[16]={};
        uint8_t key_scan_column=0;
        int num_keys_down=0;
        //BeebKey auto_reset_key=BeebKey_None;

        // Disk stuff
        WD1770 fdc;
        DiscInterfaceControl disc_control={};
        DiscDrive drives[NUM_DRIVES];

        // RTC
        MC146818 rtc;

        uint64_t last_vsync_2MHz_cycles=0;
        uint64_t last_frame_2MHz_cycles=0;

        std::vector<uint8_t> ram_buffer;

        std::shared_ptr<const ROMData> os_buffer;
        std::shared_ptr<const ROMData> sideways_rom_buffers[16];
        // Each element is either 16K (i.e., copy of ROMData
        // contents), or empty (nothing). Ideally this would be
        // something like unique_ptr<ROMData>, but that isn't
        // copyable.
        std::vector<uint8_t> sideways_ram_buffers[16];

        // Combination of BBCMicroHackFlag.
        uint32_t hack_flags=0;

        // Current paste data, if any.
        //
        // (This has to be part of the BBCMicro state - suppose a
        // state is saved mid-paste. The initiating event is in the
        // past, and won't be included in the replay data, but when
        // starting a replay from that state then the rest of the
        // paste needs to be performed.)
        BBCMicroPasteState paste_state=BBCMicroPasteState_None;
        std::shared_ptr<std::string> paste_text;
        size_t paste_index=0;
        uint64_t paste_wait_end=0;

        // Upcoming async call address.
        //
        // (This has to be part of the BBCMicro state - same argument
        // as for pasting.)
        M6502Word async_call_address={INVALID_ASYNC_CALL_ADDRESS};
        uint8_t async_call_a=0;
        uint8_t async_call_x=0;
        uint8_t async_call_y=0;
        bool async_call_c=0;
        uint8_t async_call_thunk_buf[32]={};
        int async_call_timeout=0;

        explicit State(BBCMicroType type,const std::vector<uint8_t> &nvram_contents,const tm *rtc_time);
    };

    State m_state;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that is copied, but needs special handling.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    const BBCMicroType m_type;
    DiscInterface *const m_disc_interface=nullptr;
    std::shared_ptr<DiscImage> m_disc_images[NUM_DRIVES];
    const bool m_video_nula;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that can be derived from m_state or the non-copyable stuff.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    bool m_has_rtc=false;
    void (*m_handle_cpu_data_bus_fn)(BBCMicro *)=nullptr;
    UpdateROMSELPagesFn m_update_romsel_pages_fn=nullptr;
    UpdateACCCONPagesFn m_update_acccon_pages_fn=nullptr;

    // Memory
    MemoryPages m_pages={};

    // B+ and later only - read/write pages that include the shadow
    // screen RAM.
    MemoryPages *m_shadow_pages=nullptr;

    // B+ and later only - points to the MemoryPages to use. Index
    // using the MSB of the address of the last opcode fetch.
    const MemoryPages **m_pc_pages=nullptr;//[256]

    // [0] is for page FC, [1] for FD and [2] for FE.
    //
    // Each points to 256 entries, one per byte.
    //
    // These tables are used for reads. Writes always go to the
    // hardware.
    const ReadMMIOFn *m_rmmio_fns[3]={};
    void *const *m_mmio_fn_contexts[3]={};
    const uint8_t *m_mmio_stretch[3]={};

    // Tables for pages FC/FD/FE that access the hardware - B, B+,
    // M128 when ACCCON TST=0.
    std::vector<ReadMMIOFn> m_hw_rmmio_fns[3];
    std::vector<WriteMMIOFn> m_hw_wmmio_fns[3];
    std::vector<void *> m_hw_mmio_fn_contexts[3];
    std::vector<uint8_t> m_hw_mmio_stretch[3];

    // Tables for pages FC/FD/FE that access the ROM - reads on M128
    // when ACCCON TST=1. All 3 pages behave the same in this case, so
    // there's just one set of tables.
    std::vector<ReadMMIOFn> m_rom_rmmio_fns;
    std::vector<void *> m_rom_mmio_fn_contexts;
    std::vector<uint8_t> m_rom_mmio_stretch;

    uint8_t m_romsel_mask=0;
    uint8_t m_acccon_mask=0;

    // teletext_bases[0] is used when 6845 address bit 11 is clear;
    // teletext_bases[1] when it's set.
    uint16_t m_teletext_bases[2]={};

    uint8_t *m_ram=nullptr;

    std::vector<float> m_disc_drive_sounds[DiscDriveSound_EndValue];

    void (*m_default_handle_cpu_data_bus_fn)(BBCMicro *)=nullptr;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that isn't copied.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    //mutable std::mutex m_disc_image_mutexes[NUM_DRIVES];

    //DiscDriveCallbacks m_disc_drive_callbacks={};

    // This doesn't need to be copied. The event list records its
    // influence.
    //
    // Controlled by the disc mutex, if there is one.
    bool m_disc_access=false;

#if VIDEO_TRACK_METADATA
    // This doesn't need to be copied. If it becomes stale, it'll be
    // refreshed within 1 cycle...
    uint16_t m_last_video_access_address=0;
#endif

#if BBCMICRO_TRACE
    // Event trace stuff.
    //
    // m_trace holds the result of m_trace_ptr.get(), in the interests
    // of non-terrible debug build performance.
    std::shared_ptr<Trace> m_trace_ptr;
    Trace *m_trace=nullptr;
    InstructionTraceEvent *m_trace_current_instruction=nullptr;
    uint32_t m_trace_flags=0;
#endif

    std::vector<std::pair<InstructionFn,void *>> m_instruction_fns;

#if BBCMICRO_DEBUGGER
    std::unique_ptr<DebugState> m_debug_ptr;

    // try to avoid appalling debug build performance...
    DebugState *m_debug=nullptr;

    DebugAsyncCallFn m_async_call_fn=nullptr;
    void *m_async_call_context=nullptr;
#endif

    void InitStuff();
    void SetOSPages(uint8_t dest_page,uint8_t src_page,uint8_t num_pages);
    void SetROMPages(uint8_t bank,uint8_t page,size_t src_page,size_t num_pages);
#if BBCMICRO_TRACE
    void SetTrace(std::shared_ptr<Trace> trace,uint32_t trace_flags);
#endif
    void SetPages(uint8_t page_,size_t num_pages,
                  const uint8_t *read_data,size_t read_page_stride,
                  uint8_t *write_data,size_t write_page_stride
#if BBCMICRO_DEBUGGER/////////////////////////////////////////////////////////<--note
                  ,uint16_t debug_page////////////////////////////////////////<--note
#endif////////////////////////////////////////////////////////////////////////<--note
    );////////////////////////////////////////////////////////////////////////<--note
    static void UpdateBROMSELPages(BBCMicro *m);
    static void UpdateBPlusACCCONPages(BBCMicro *m,const ACCCON *old);
    static void UpdateBACCCONPages(BBCMicro *m,const ACCCON *old);
    static void UpdateBPlusROMSELPages(BBCMicro *m);
    static void UpdateMaster128ROMSELPages(BBCMicro *m);
    static bool DoesMOSUseShadow(ACCCON acccon);
    static void UpdateMaster128ACCCONPages(BBCMicro *m,const ACCCON *old_);
    void InitROMPages();
    static void Write1770ControlRegister(void *m_,M6502Word a,uint8_t value);
    static uint8_t Read1770ControlRegister(void *m_,M6502Word a);
#if BBCMICRO_TRACE
    void TracePortB(SystemVIAPB pb);
#endif
    static void HandleSystemVIAB(R6522 *via,uint8_t value,uint8_t old_value,void *m_);
    static void WriteUnmappedMMIO(void *m_,M6502Word a,uint8_t value);
    static uint8_t ReadUnmappedMMIO(void *m_,M6502Word a);
    static uint8_t ReadROMMMIO(void *m_,M6502Word a);
    static uint8_t ReadROMSEL(void *m_,M6502Word a);
    static void WriteROMSEL(void *m_,M6502Word a,uint8_t value);
    static uint8_t ReadACCCON(void *m_,M6502Word a);
    static void WriteACCCON(void *m_,M6502Word a,uint8_t value);
#if BBCMICRO_DEBUGGER
    static uint8_t ReadAsyncCallThunk(void *m_,M6502Word a);
    void HandleReadByteDebugFlags(uint8_t read,DebugState::ByteDebugFlags *flags);
    void HandleInterruptBreakpoints();
#endif
    static void HandleCPUDataBusMainRAMOnly(BBCMicro *m);
    static void HandleCPUDataBusWithShadowRAM(BBCMicro *m);
#if BBCMICRO_DEBUGGER
    static void HandleCPUDataBusMainRAMOnlyDebug(BBCMicro *m);
    static void HandleCPUDataBusWithShadowRAMDebug(BBCMicro *m);
    void UpdateDebugPages(MemoryPages *pages);
    void UpdateDebugState();
    void SetDebugStepType(BBCMicroStepType step_type);
    void DebugGetBytePointers(uint8_t **write_ptr,const uint8_t **read_ptr,uint16_t *debug_page,uint32_t full_addr);
    void FinishAsyncCall(bool called);
#endif
    static void HandleCPUDataBusWithHacks(BBCMicro *m);
    static void HandleTurboRTI(M6502 *cpu);

    // 1770 handler stuff.
    bool IsTrack0() override;
    void StepOut() override;
    void StepIn() override;
    void SpinUp() override;
    void SpinDown() override;
    bool IsWriteProtected() override;
    bool GetByte(uint8_t *value,uint8_t track,uint8_t sector,size_t offset) override;
    bool SetByte(uint8_t track,uint8_t sector,size_t offset,uint8_t value) override;
    bool GetSectorDetails(uint8_t *side,size_t *size,uint8_t track,uint8_t sector,bool double_density) override;
    DiscDrive *GetDiscDrive();
#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    void InitDiscDriveSounds(DiscDriveType type);
    void StepSound(DiscDrive *dd);
    float UpdateDiscDriveSound(DiscDrive *dd);
#endif
    void UpdateCPUDataBusFn();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
