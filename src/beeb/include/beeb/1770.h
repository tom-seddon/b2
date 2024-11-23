#ifndef HEADER_B2263C0454BF4C5997821F7E62057922
#define HEADER_B2263C0454BF4C5997821F7E62057922

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"
#include "6502.h"

class DiscInterface;
class Trace;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Basic 1770 simulator.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "1770.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Handle communication between 1770 and the disc drive.
class WD1770Handler {
  public:
    WD1770Handler();
    virtual ~WD1770Handler() = 0;

    WD1770Handler(const WD1770Handler &) = delete;
    WD1770Handler &operator=(const WD1770Handler &) = delete;
    WD1770Handler(WD1770Handler &&) = delete;
    WD1770Handler &operator=(WD1770Handler &&) = delete;

    // Return true if the drive is at track 0.
    virtual bool IsTrack0() = 0;

    // Step out (towards track 0).
    virtual void StepOut(int step_rate_ms) = 0;

    // Step in (towards track N).
    virtual void StepIn(int step_rate_ms) = 0;

    // Spin the drive up. The motor is assumed to switch on.
    virtual void SpinUp() = 0;

    // Spin the drive down. The motor is assumed to switch off.
    virtual void SpinDown() = 0;

    virtual bool IsWriteProtected() = 0;

    // These always operate on the current track.
    virtual bool GetByte(uint8_t *value, uint8_t sector, size_t offset) = 0;
    virtual bool SetByte(uint8_t track, size_t offset, uint8_t value) = 0;
    virtual bool GetSectorDetails(uint8_t *track, uint8_t *side, size_t *size, uint8_t sector, bool double_density) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if WD1770_DUMP_WRITTEN_SECTOR || WD1770_DUMP_READ_SECTOR || BBCMICRO_TRACE
#define WD1770_SAVE_SECTOR_DATA 1
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct WD1770 {
  public:
#include <shared/pushwarn_bitfields.h>
    struct StatusBits {
        uint8_t busy : 1;
        uint8_t drq_or_idx : 1;
        uint8_t lost_or_track0 : 1;
        uint8_t crc_error : 1;
        uint8_t rnf : 1;
        uint8_t deleted_or_spinup : 1;
        uint8_t write_protect : 1;
        uint8_t motor_on : 1;
    };
#include <shared/popwarn.h>

    union Status {
        StatusBits bits;
        uint8_t value;
    };

#include <shared/pushwarn_bitfields.h>
    struct CommandTypeIBits {
        uint8_t r : 2, v : 1, h : 1, cmd : 4;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct CommandStepBits {
        uint8_t r : 2, v : 1, h : 1, u : 1, cmd : 3;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct CommandTypeIIBits {
        uint8_t a0 : 1, p : 1, e : 1, h : 1, m : 1, cmd : 3;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct CommandTypeIIIBits {
        uint8_t _ : 1, p : 1, e : 1, h : 1, cmd : 4;
    };
#include <shared/popwarn.h>

#include <shared/pushwarn_bitfields.h>
    struct CommandTypeIVBits {
        uint8_t _ : 2, index : 1, immediate : 1, cmd : 4;
    };
#include <shared/popwarn.h>

    union Command {
        CommandTypeIBits bits_i;
        CommandStepBits bits_step;
        CommandTypeIIBits bits_ii;
        CommandTypeIIIBits bits_iii;
        CommandTypeIVBits bits_iv;
        uint8_t value;
    };

#include <shared/pushwarn_bitfields.h>
    struct PinsBits {
        bool drq : 1, intrq : 1;
    };
#include <shared/popwarn.h>

    union Pins {
        PinsBits bits;
        uint8_t value;
    };

    explicit WD1770();

    void Reset();
    void SetHandler(WD1770Handler *handler);

    static void Write0(void *fdc, M6502Word addr, uint8_t value);
    static void Write1(void *fdc, M6502Word addr, uint8_t value);
    static void Write2(void *fdc, M6502Word addr, uint8_t value);
    static void Write3(void *fdc, M6502Word addr, uint8_t value);

    static uint8_t Read0(void *fdc, M6502Word addr);
    static uint8_t Read1(void *fdc, M6502Word addr);
    static uint8_t Read2(void *fdc, M6502Word addr);
    static uint8_t Read3(void *fdc, M6502Word addr);

    void SetDDEN(bool dden);

    void SetTrace(Trace *trace);

    Pins Update();

    void Set1772(bool is1772);
    void SetNoINTRQ(bool no_intrq);

  protected:
  private:
    WD1770Handler *m_handler = nullptr;

    //
    bool m_no_intrq = false;
    bool m_is1772 = false;

    // Registers.
    Status m_status = {};   //read
    Command m_command = {}; //write
    uint8_t m_track = 0;    //read/write
    uint8_t m_sector = 0;   //read/write
    uint8_t m_data = 0;     //read/write

    bool m_dden = false;

    Pins m_pins = {};
    uint8_t m_direction = 0; //step IN (todo)
    uint8_t m_restore_count = 0;
    size_t m_offset = 0;
    size_t m_sector_size = 0;
    int m_wait_us = 0;
    uint8_t m_address[6] = {};

    WD1770State m_state = WD1770State_BeginIdle;
    WD1770State m_next_state = WD1770State_BeginIdle;
    int m_state_time = 0;

#if WD1770_SAVE_SECTOR_DATA
    uint8_t m_sector_data[1024] = {};
#endif

#if BBCMICRO_TRACE
    Trace *m_trace = nullptr;
#endif

    void ResetStatusRegister();
    //bool IsTrack0();
    //void StepOut();
    //void StepIn();
    int IsMotorOn();
    void SpinUp();
    void SpinDown();
    int GetStepRateMS(uint8_t index) const;
    void SetState(WD1770State state);
    void Print1770Registers(Log *log);
    void SetDRQ(bool value);
    void SetINTRQ(bool value);
    void DoSpinUp(int h, WD1770State state);
    void DoTypeI(WD1770State state);
    void DoTypeII(WD1770State state);
    void DoTypeIII(WD1770State state);
    void DoTypeIV();
    void Wait(int us, WD1770State next_state);
    void DoTypeIIOrTypeIIIDelay(WD1770State next_state);
    int DoTypeIIFindSector();
    void DoTypeIINextByte(WD1770State next_byte_state, WD1770State next_sector_state);
    void UpdateTrack0Status();

    static const int STEP_RATES_MS_1770[];
    static const int STEP_RATES_MS_1772[];

#ifdef BBCMICRO_DEBUGGER
    friend class WD1770DebugWindow;
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CHECK_SIZEOF(WD1770::Status, 1);
CHECK_SIZEOF(WD1770::Command, 1);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
