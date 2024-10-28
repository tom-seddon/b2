#ifndef HEADER_5515E213300440068BB90FD401AD3AA2
#define HEADER_5515E213300440068BB90FD401AD3AA2

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ROM;
struct DiscDriveCallbacks;
class Log;
struct VideoDataUnit;
struct SoundDataUnit;
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
#include "keys.h"
#include "BBCMicroParasiteType.h"
#include "BBCMicroState.h"

#include <shared/enum_decl.h>
#include "BBCMicro.inl"
#include <shared/enum_end.h>

//
#define BBCMICRO_NUM_UPDATE_GROUPS (4)

#define BBCMicroLEDFlags_AllDrives (255u * BBCMicroLEDFlag_Drive0)

constexpr uint32_t GetNormalizedBBCMicroUpdateFlags(uint32_t flags);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

constexpr ROMType GetBBCMicroUpdateFlagsROMType(uint32_t update_flags) {
    return (ROMType)(update_flags >> BBCMicroUpdateFlag_ROMTypeShift & BBCMicroUpdateFlag_ROMTypeMask);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BBCMicro : private WD1770Handler {
  public:
    static const uint16_t SCREEN_WRAP_ADJUSTMENTS[];

    static const uint16_t ADJI_ADDRESSES[4];

    typedef void (*UpdateROMSELPagesFn)(BBCMicro *);
    typedef void (*UpdateACCCONPagesFn)(BBCMicro *, const ACCCON *);
    typedef uint32_t (BBCMicro::*UpdateMFn)(VideoDataUnit *, SoundDataUnit *);

    static constexpr size_t NUM_UPDATE_MFNS = 65536;

    static std::string GetUpdateFlagExpr(const uint32_t flags_);

#if BBCMICRO_DEBUGGER

    struct UpdateMFnData {
        // number of cycles spent in each state
        CycleCount update_mfn_cycle_count[NUM_UPDATE_MFNS] = {};

        // Number of times the update mfn has changed
        uint64_t num_update_mfn_changes = 0;

        // Number of UpdatePaging calls
        uint64_t num_UpdatePaging_calls = 0;
    };

    // TODO: need to do a pass on the thread safety of this stuff?

    struct HardwareDebugState {
        R6522::IRQ system_via_irq_breakpoints = {};
        R6522::IRQ user_via_irq_breakpoints = {};
    };

    struct DebugState {
        static const uint16_t INVALID_PAGE_INDEX = 0xffff;

        struct RelativeCycleCountBase {
            // Cycle count of most recent reset, or invalid if no such.
            CycleCount recent = {};

            // Cycle count of previous reset, if any. Used to provide a useful
            // time-since-last-breakpoint indicator if a breakpoint was hit this
            // cycle (thus overwriting hit_recent).
            CycleCount prev = {};

            // Whether to reset when a breokpoint is hit.
            bool reset_on_breakpoint = true;
        };

        struct Breakpoint {
            uint64_t id = 0;
        };

        bool is_halted = false;

        BBCMicroStepType step_type = BBCMicroStepType_None;
        const M6502 *step_cpu = nullptr;

        HardwareDebugState hw;

        RelativeCycleCountBase host_relative_base;
        RelativeCycleCountBase parasite_relative_base;

        // No attempt made to minimize this stuff... it doesn't go into
        // the saved states, so whatever.

        // Byte-specific breakpoint flags.
        uint8_t big_pages_byte_debug_flags[NUM_BIG_PAGES][BIG_PAGE_SIZE_BYTES] = {};

        // Increases every time the breakpoint state changes.
        uint64_t breakpoints_changed_counter = 1;

        // Total number of bytes and/or addresses that currently have any
        // breakpoints set on them. When num_breakpoint_bytes==0, no breakpoints
        // are set.
        uint64_t num_breakpoint_bytes = 0;

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
        BigPageIndex index = {0};

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

        // Multiple big pages can occupy the same address. The address flags
        // often overlap.
        const uint8_t *address_debug_flags = nullptr;
#endif
        BigPageIndex index = {0};
        const BigPageMetadata *metadata = nullptr;
    };

    // nvram_contents and rtc_time are ignored if the BBCMicro doesn't
    // support such things.
    BBCMicro(std::shared_ptr<const BBCMicroType> type,
             const DiscInterface *disc_interface,
             BBCMicroParasiteType parasite_type,
             const std::vector<uint8_t> &nvram_contents,
             const tm *rtc_time,
             uint32_t init_flags,
             BeebLinkHandler *beeblink_handler,
             CycleCount initial_cycle_count);

    explicit BBCMicro(const BBCMicroUniqueState &state);
    BBCMicro(const BBCMicro &) = delete;

  public:
    ~BBCMicro();

    // result is a combination of BBCMicroCloneImpediment.
    uint32_t GetCloneImpediments() const;

    const BBCMicroUniqueState *GetUniqueState() const;

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

    // Called after an opcode fetch and before execution.
    //
    // cpu->pc.w is PC+1; cpu->pc.dbus is the opcode fetched.
    //
    // Return true to keep the callback, or false to remove it.
    typedef bool (*InstructionFn)(const BBCMicro *m, const M6502 *cpu, void *context);

    // Called when an address is about to be written.
    typedef bool (*WriteFn)(const BBCMicro *m, const M6502 *cpu, void *context);

    BBCMicroTypeID GetTypeID() const;
    BBCMicroParasiteType GetParasiteType() const;

    // Get read-only pointer to the cycle counter. The pointer is never
    // NULL and remains valid for the lifetime of the BBCMicro.
    //
    // (This is supposed to be impossible to get wrong (like a getter) but
    // also cheap enough in a debug build (unlike a getter) that I don't
    // have to worry about that.)
    const CycleCount *GetCycleCountPtr() const;

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

    // Update stats and debug stuff that need updating only at low frequency
    // (100 Hz, say...) - if even at all.
    void OptionalLowFrequencyUpdate();

    static uint32_t GetNormalizedBBCMicroUpdateFlags(uint32_t flags);

    // The disc drive sounds are used by all BBCMicro objects created
    // after they're set.
    //
    // Once a particular sound is set, it can't be changed.
    static void SetDiscDriveSound(DiscDriveType type, DiscDriveSound sound, std::vector<float> samples);

    uint32_t GetLEDs();

    std::vector<uint8_t> GetNVRAM() const;

    // The shared_ptr is copied.
    void SetOSROM(std::shared_ptr<const std::array<uint8_t, 16384>> data);

    // The shared_ptr is copied.
    void SetSidewaysROM(uint8_t bank, std::shared_ptr<const std::vector<uint8_t>> data, ROMType type);

    // The first 16 KB of the ROM data is copied.
    void SetSidewaysRAM(uint8_t bank, std::shared_ptr<const std::vector<uint8_t>> data);

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

    std::shared_ptr<const BBCMicroReadOnlyState> DebugGetState() const;

    const M6502 *GetM6502() const;

#if BBCMICRO_DEBUGGER
    // Given address, return the BigPage for the memory there, taking
    // the debug page overrides into account.
    //
    // mos indicates whether the access is from MOS code (true) or user code
    // (false).
    const BigPage *DebugGetBigPageForAddress(M6502Word addr, bool mos, uint32_t dso) const;
    static void DebugGetBigPageForAddress(ReadOnlyBigPage *bp,
                                          const BBCMicroState *state,
                                          const DebugState *debug_state,
                                          M6502Word addr,
                                          bool mos,
                                          uint32_t dso);

    static void DebugGetMemBigPageIsMOSTable(uint8_t *mem_big_page_is_mos, const BBCMicroState *state, uint32_t dso);

    // Get/set per-byte debug flags for one byte.
    uint8_t DebugGetByteDebugFlags(const BigPage *big_page, uint32_t offset) const;
    void DebugSetByteDebugFlags(BigPageIndex big_page_index, uint32_t offset, uint8_t flags);

    // Get/set per-address byte debug flags for one address.
    uint8_t DebugGetAddressDebugFlags(M6502Word addr, uint32_t dso) const;
    void DebugSetAddressDebugFlags(M6502Word addr, uint32_t dso, uint8_t flags);

    void DebugGetBytes(uint8_t *bytes, size_t num_bytes, M6502Word addr, uint32_t dso, bool mos);
    void DebugSetBytes(M6502Word addr, uint32_t dso, bool mos, const uint8_t *bytes, size_t num_bytes);

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

    static uint32_t DebugGetCurrentStateOverride(const BBCMicroState *state);

    // The breakpoints change counter is incremented any time the set of
    // breakpoints changes.
    uint64_t DebugGetBreakpointsChangeCounter() const;

    void DebugResetRelativeCycleBase(uint32_t dso);
    void DebugToggleResetRelativeCycleBaseOnBreakpoint(uint32_t dso);

    // Bit ugly to actually use, but it at least centralises some logic.
    static DebugState::RelativeCycleCountBase DebugState::*DebugGetRelativeCycleCountBaseMPtr(const BBCMicroState &state, uint32_t dso);

#endif

    void SendBeebLinkResponse(std::vector<uint8_t> data);

    uint16_t GetAnalogueChannel(uint8_t channel) const;
    void SetAnalogueChannel(uint8_t channel, uint16_t value);

    BBCMicroState::DigitalJoystickInput GetDigitalJoystickState(uint8_t index) const;
    void SetDigitalJoystickState(uint8_t index, BBCMicroState::DigitalJoystickInput state);

    static void PrintInfo(Log *log);

    // When the printer is enabled, printer output will be captured into a
    // buffer.
    void SetPrinterEnabled(bool is_printer_enabled);

    // Overly simplistic mechanism?
    void SetPrinterBuffer(std::vector<uint8_t> *buffer);

    bool HasADC() const;

    uint32_t GetUpdateFlags() const;
#if BBCMICRO_DEBUGGER
    std::shared_ptr<const UpdateMFnData> GetUpdateMFnData() const;
#endif

    void AddMouseMotion(int dx, int dy);
    void SetMouseButtons(uint8_t mask, uint8_t value);

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

  private:
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that's mostly copyable using copy ctor. A few things do
    // require minor fixups afterwards.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    BBCMicroUniqueState m_state;

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //
    // stuff that can be derived from m_state or the non-copyable stuff.
    //
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    BigPage m_big_pages[NUM_BIG_PAGES];
    UpdateMFn m_update_mfn = nullptr;
    uint32_t m_update_flags = 0;

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

#if BBCMICRO_DEBUGGER
    std::shared_ptr<UpdateMFnData> m_update_mfn_data_ptr;
    UpdateMFnData *m_update_mfn_data = nullptr;
    CycleCount m_last_mfn_change_cycle_count = {};
#endif

    void InitStuff();
#if BBCMICRO_TRACE
    void SetTrace(std::shared_ptr<Trace> trace, uint32_t trace_flags);
#endif
    void UpdatePaging();
    void InitPaging();
    static void Write1770ControlRegister(void *m_, M6502Word a, uint8_t value);
    static uint8_t Read1770ControlRegister(void *m_, M6502Word a);
#if BBCMICRO_TRACE
    void TracePortB(BBCMicroState::SystemVIAPB pb);
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
    void DebugHitBreakpoint(const M6502 *cpu, BBCMicro::DebugState::RelativeCycleCountBase *base, uint8_t flags);
    void DebugHandleStep();
    // Public for the debugger's benefit.
#endif
    static void InitReadOnlyBigPage(ReadOnlyBigPage *bp,
                                    const BBCMicroState *state,
#if BBCMICRO_DEBUGGER
                                    const DebugState *debug_state,
#endif
                                    BigPageIndex big_page_index);

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
    BBCMicroState::DiscDrive *GetDiscDrive();
    void InitDiscDriveSounds(DiscDriveType type);
    void StepSound(BBCMicroState::DiscDrive *dd);
    float UpdateDiscDriveSound(BBCMicroState::DiscDrive *dd);
    void UpdateCPUDataBusFn();
    void SetMMIOFnsInternal(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context, bool set_xfj, bool set_ifj);

    static void WriteHostTube0Wrapper(void *context, M6502Word a, uint8_t value);

    // ADC handler stuff.
    static uint16_t ReadAnalogueChannel(uint8_t channel, void *context);

    template <uint32_t UPDATE_FLAGS>
    uint32_t UpdateTemplated(VideoDataUnit *video_unit, SoundDataUnit *sound_unit);

#if BBCMICRO_DEBUGGER
    void UpdateUpdateMFnData();
#endif

    void UpdateMapperRegion(uint8_t region);

    static const uint8_t CURSOR_PATTERNS[8];

#if !(BBCMICRO_NUM_UPDATE_GROUPS == 1 || BBCMICRO_NUM_UPDATE_GROUPS == 2 || BBCMICRO_NUM_UPDATE_GROUPS == 4)
#error unsupported BBCMICRO_NUM_UPDATE_GROUPS value
#endif

    static_assert(NUM_UPDATE_MFNS % BBCMICRO_NUM_UPDATE_GROUPS == 0);

    static void EnsureUpdateMFnsTableIsReady();

#if BBCMICRO_NUM_UPDATE_GROUPS == 1
    static const UpdateMFn ms_update_mfns[NUM_UPDATE_MFNS];
#endif
#if BBCMICRO_NUM_UPDATE_GROUPS >= 2
    // A bit wasteful, but it reduces the impact of changing
    // BBCMICRO_NUM_UPDATE_GROUPS on the rest of the code.
    static UpdateMFn ms_update_mfns[NUM_UPDATE_MFNS];

    static const UpdateMFn ms_update_mfns0[NUM_UPDATE_MFNS / BBCMICRO_NUM_UPDATE_GROUPS];
    static const UpdateMFn ms_update_mfns1[NUM_UPDATE_MFNS / BBCMICRO_NUM_UPDATE_GROUPS];
#endif
#if BBCMICRO_NUM_UPDATE_GROUPS == 4
    static const UpdateMFn ms_update_mfns2[NUM_UPDATE_MFNS / BBCMICRO_NUM_UPDATE_GROUPS];
    static const UpdateMFn ms_update_mfns3[NUM_UPDATE_MFNS / BBCMICRO_NUM_UPDATE_GROUPS];
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
