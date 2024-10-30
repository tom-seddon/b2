#ifndef HEADER_68B8925BC8D24ED7A572AB6FAAB58E41
#define HEADER_68B8925BC8D24ED7A572AB6FAAB58E41

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct M6502;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !M6502WORD_DEFINED

/* This struct never aliases anything in 6502 memory - it needs to be
 * the right way round for the host system. (It should probably be
 * called M6502HostWord, or something...) */
struct M6502WordBytes {
    uint8_t l, h;
};

union M6502Word {
    uint16_t w;
    struct M6502WordBytes b;
};
typedef union M6502Word M6502Word;

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct M6502PBitsInternal {
    uint8_t c : 1;
    uint8_t z : 1;
    uint8_t i : 1;
    uint8_t d : 1;
    uint8_t _4 : 1; //driven by d1x1
    uint8_t _5 : 1; //always reads as 1
    uint8_t v : 1;
    uint8_t n : 1;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

union M6502PInternal {
    uint8_t value;
    struct M6502PBitsInternal bits;
};
typedef union M6502PInternal M6502PInternal;

extern const char M6502_check_size[sizeof(M6502PInternal) == 1 ? 1 : -1];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct M6502PBits {
    uint8_t c : 1, z : 1, i : 1, d : 1;
    uint8_t b : 1, _ : 1, v : 1, n : 1;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

union M6502P {
    uint8_t value;
    struct M6502PBits bits;
};
typedef union M6502P M6502P;

extern const char M6502_check_size[sizeof(M6502P) == 1 ? 1 : -1];

// Writes 9 bytes: 8 chars and a '\x0'. Returns dest.
char *M6502P_GetString(char *dest, M6502P value);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum M6502AddrMode {
    M6502AddrMode_IMP,
    M6502AddrMode_IMM,
    M6502AddrMode_REL,
    M6502AddrMode_ZPG,
    M6502AddrMode_ZPX,
    M6502AddrMode_ZPY,
    M6502AddrMode_INX,
    M6502AddrMode_INY,
    M6502AddrMode_ABS,
    M6502AddrMode_ABX,
    M6502AddrMode_ABY,
    M6502AddrMode_IND,
    M6502AddrMode_ACC,
    M6502AddrMode_INZ,
    M6502AddrMode_INDX,
    M6502AddrMode_ZPG_REL_ROCKWELL,
};
typedef enum M6502AddrMode M6502AddrMode;

const char *M6502AddrMode_GetName(uint8_t mode);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum M6502Condition {
    M6502Condition_None = 0,
    M6502Condition_Always,
    M6502Condition_CC,
    M6502Condition_CS,
    M6502Condition_VC,
    M6502Condition_VS,
    M6502Condition_NE,
    M6502Condition_EQ,
    M6502Condition_PL,
    M6502Condition_MI,

    // The BRx values are contiguous
    M6502Condition_BR0,
    M6502Condition_BR1,
    M6502Condition_BR2,
    M6502Condition_BR3,
    M6502Condition_BR4,
    M6502Condition_BR5,
    M6502Condition_BR6,
    M6502Condition_BR7,

    // The BSx values are contiguous
    M6502Condition_BS0,
    M6502Condition_BS1,
    M6502Condition_BS2,
    M6502Condition_BS3,
    M6502Condition_BS4,
    M6502Condition_BS5,
    M6502Condition_BS6,
    M6502Condition_BS7,
};
typedef enum M6502Condition M6502Condition;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef void (*M6502Fn)(struct M6502 *);

typedef void (*M6502Callback)(struct M6502 *, void *);

struct M6502Fns {
    M6502Fn t0fn, ifn;
};
typedef struct M6502Fns M6502Fns;

struct M6502DisassemblyInfo {
    // M6502AddrMode.
    uint8_t mode;

    // Count includes the instruction.
    uint8_t num_bytes;

    // Set if undocumented.
    uint8_t undocumented : 1;

    // Set if the debugger should actually do a step in when stepping
    // over this instruction.
    uint8_t always_step_in : 1;

    // Condition for branch - 0=not a branch, otherwise one of the
    // M6502Condition values.
    uint8_t branch_condition : 5;

    // Mnemonic.
    char mnemonic[5];
};
typedef struct M6502DisassemblyInfo M6502DisassemblyInfo;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct M6502Config {
    /* Name of this type of 6502. */
    const char *name;

    /* Function to call to start interrupt processing. */
    M6502Fn interrupt_tfn;

    /* Value to use for the XAA and LXA instructions, for CPUs that
     * support them.
     */
    uint8_t xaa_magic;

    /* The 256 opcode function pairs. */
    const M6502Fns *fns;

    /* A DisassemblyInfo for each opcode. */
    const M6502DisassemblyInfo *disassembly_info;
};
typedef struct M6502Config M6502Config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// * NMOS: standard defined set of instructions
// * Undocumented: undocumented NMOS-specific instructions
// * CMOS: additional 65c02 instructions and addressing modes
// * Rockwell: BBRn, BBSn, RMBn, SMBn (replacing some CMOS NOPs)
// * WDC: WAI, STP (replacing some CMOS NOPs)

/* NMOS, Undocumented
 */
extern const M6502Config M6502_nmos6502_config;

/* NMOS
 *
 * If an undefined instruction is encountered, the M6502 object's ill_fn will be
 * called.
 */
extern const M6502Config M6502_defined_config;

/* NMOS, CMOS
 */
extern const M6502Config M6502_cmos6502_config;

/* NMOS, CMOS, Rockwell
 */
extern const M6502Config M6502_rockwell65c02_config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Appropriate type for holding device IRQ flags. There's 1 bit per
 * device that might cause an IRQ.
 */
typedef uint8_t M6502_DeviceIRQFlags;
typedef uint8_t M6502_DeviceNMIFlags;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum M6502ReadType {
    // Read data from memory.
    M6502ReadType_Data = 1,

    // Fetch non-opcode instruction byte.
    M6502ReadType_Instruction = 2,

    // Fetch indirect address.
    M6502ReadType_Address = 3,

    // Almost certainly not interesting.
    M6502ReadType_Uninteresting = 4,

    // Fetch opcode byte. This only occurs at one point: the first
    // cycle of an instruction.
    M6502ReadType_Opcode = 5,

    // Dummy fetch when an interrupt/NMI is due. This only occurs at
    // one point: the first cycle of an instruction, when that
    // instruction was interrupted. The next 7 cycles will be the
    // usual interrupt setup stuff, and then there'll be a Opcode read
    // for the first instruction of the interrupt handler.
    M6502ReadType_Interrupt = 6,

    M6502ReadType_Count,

    M6502ReadType_LastInterestingDataRead = M6502ReadType_Address,
    M6502ReadType_FirstBeginInstruction = M6502ReadType_Opcode,
};

const char *M6502ReadType_GetName(uint8_t read_type);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct M6502 {
    /* Function that steps the 6502 one cycle. After each step,
     * examine ABUS (the address to access) and READ, and take
     * appropriate action. When READ is true, set *DBUS to the value
     * read; when READ is false, put *DBUS in the appropriate
     * place. */
    M6502Fn tfn;

    /* (internal) Function that does whatever for the current
     * instruction. */
    M6502Fn ifn;

    /* Pointer to 256 function pairs for opcode dispatch (copied from
     * the config object). */
    const M6502Fns *fns;

    /* Pointer to the IRQ function. */
    M6502Fn interrupt_tfn;

    // (32)

    /* Address pins. */
    M6502Word abus;

    /* Read/write pin. true = read - one of the M6502_READ_xxx values; false = write. */
    uint8_t read;

    /* Data bus. */
    uint8_t dbus;

    /* IRQ/NMI flags. Set if an IRQ/NMI was received since the last
     * check, and wants servicing.
     *
     * (The NMI flags are also used to handle the positive edge
     * thing.)
     */
    M6502_DeviceIRQFlags irq_flags;
    M6502_DeviceNMIFlags nmi_flags;

    /* Current IRQ/NMI flags. There are N available, 1 per device that
     * might cause an interrupt, so they can be set or unset
     * independently.
     */
    M6502_DeviceIRQFlags device_irq_flags;
    M6502_DeviceNMIFlags device_nmi_flags;

    // (40)

    /* 16-bit registers */
    M6502Word pc;        /* program counter */
    M6502Word s;         /* stack pointer (H byte always 1) */
    M6502Word ad;        /* (internal) data address */
    M6502Word ia;        /* (internal) indirect address */
    M6502Word opcode_pc; /* address of last opcode fetched */

    // (50)

    /* 8-bit registers */
    uint8_t a;          /* accumulator */
    uint8_t x;          /* X */
    uint8_t y;          /* Y */
    M6502PInternal p;   /* status register */
    uint8_t opcode;     /* (internal) last opcode fetched */
    uint8_t data;       /* (internal) misc - usually operand
                                 * for current instruction */
    uint8_t acarry : 1; /* (internal) address calculation
                                 * carry flag */
                        /*  */
    uint8_t d1x1 : 1;   /* (internal) D1x1, active low */

    // (57-58)

    /* Pointer to the config object this 6502 was initialised with. */
    const M6502Config *config;

    /* Callback called when an illegal instruction is encountered.
     * Consult OPCODE for about as much detail as you're going to
     * get... the program counter will have moved on by the time this
     * is called. */
    M6502Callback ill_fn;
    void *ill_context;

    /* Opaque context pointer. */
    void *context;
};
typedef struct M6502 M6502;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void M6502_Init(M6502 *s, const M6502Config *config);
void M6502_Destroy(M6502 *s);

void M6502_Reset(M6502 *s);

/* Immediately puts the 6502 in a state as if the HLT instruction were
 * just executed.
 */
void M6502_Halt(M6502 *s);

/* If the 6502 is about to execute an instruction, return true. The
 * address of the instruction can be seen on the address bus, and
 * s->read is M6502ReadType_Opcode.
 *
 * s->opcode is the last opcode executed, and a/x/y/etc. have
 * appropriate values.
 *
 * When (and only when) M6502_IsAboutToExecute returns true, the 6502
 * state can be (somewhat) safely fiddled with.
 */
#define M6502_IsAboutToExecute(S) ((S)->read == M6502ReadType_Opcode)

/* If M6502_IsAboutToExecute, disassemble the last instruction. (Any
 * other time, you just get junk...)
 *
 * The instruction comes from OPCODE; the operand(s) are inferred from
 * the 6502 state. If it wouldn't be obvious from the disassembly, *ia
 * is set to the indirect address used, else -1; *ad is the equivalent
 * for the address used, likewise. (Either may be NULL when the caller
 * doesn't care.)
 */
void M6502_DisassembleLastInstruction(M6502 *s, char *buf, size_t buf_size, int *ia, int *ad);

/* Get the current opcode, assuming the data bus is up to date.
 *
 * If M6502_IsAboutToExecute, the current opcode is what's on the data
 * bus; otherwise, it's the opcode register.
 */
uint8_t M6502_GetOpcode(const M6502 *s);

/* Guess whether the current interrupt is an IRQ rather than an NMI.
 * Call when the bus access type is M6502ReadType_Interrupt - the
 * result isn't meaningful otherwise.
 *
 * The result is accurate for CMOS parts. NMOS parts on the other hand
 * don't actually check for NMI vs IRQ until 4 cycles later, so the
 * result could be wrong.
 */
#define M6502_IsProbablyIRQ(S) ((S)->irq_flags != 0)

/* After the end state of an instruction, point the PC at the right
 * place and call this to set things up for the next one. */
void M6502_NextInstruction(M6502 *s);

/* Sets the given device(s)'s IRQ flags as appropriate.
 */
void M6502_SetDeviceIRQ(M6502 *s, M6502_DeviceIRQFlags mask, int wants_irq);
void M6502_SetDeviceNMI(M6502 *s, M6502_DeviceNMIFlags mask, int wants_nmi);

/* Result points into static buffer. */
const char *M6502_GetStateName(M6502 *s, uint8_t is_dbus_valid);

/* Getter and setter for the P register. Don't access it directly. */
void M6502_SetP(M6502 *s, uint8_t p);
M6502P M6502_GetP(const M6502 *s);

typedef void (*M6502_ForEachFnFn)(const char *name, M6502Fn fn, void *context);
void M6502_ForEachFn(M6502_ForEachFnFn fn, void *context);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif
