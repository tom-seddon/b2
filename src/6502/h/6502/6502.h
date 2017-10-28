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

/* This struct never aliases anything in 6502 memory - it needs to be
 * the right way round for the host system. (It should probably be
 * called M6502HostWord, or something...) */
struct M6502WordBytes {
    uint8_t l,h;
};

union M6502Word {
    uint16_t w;
    struct M6502WordBytes b;
};
typedef union M6502Word M6502Word;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct M6502PBitsInternal {
    uint8_t c:1;
    uint8_t z:1;
    uint8_t i:1;
    uint8_t d:1;
    uint8_t _4:1;//driven by d1x1
    uint8_t _5:1;//always reads as 1
    uint8_t v:1;
    uint8_t n:1;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

union M6502PInternal {
    struct M6502PBitsInternal bits;
    uint8_t value;
};
typedef union M6502PInternal M6502PInternal;

extern const char M6502_check_size[sizeof(M6502PInternal)==1?1:-1];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct M6502PBits {
    uint8_t c:1,z:1,i:1,d:1;
    uint8_t b:1,_:1,v:1,n:1;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

union M6502P {
    struct M6502PBits bits;
    uint8_t value;
};
typedef union M6502P M6502P;

extern const char M6502_check_size[sizeof(M6502P)==1?1:-1];

// Writes 9 bytes: 8 chars and a '\x0'. Returns dest.
char *M6502P_GetString(char *dest,M6502P value);

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
};
typedef enum M6502AddrMode M6502AddrMode;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef void (*M6502Fn)(struct M6502 *);

typedef void (*M6502Callback)(struct M6502 *,void *);

struct M6502Fns {
    M6502Fn t0fn,ifn;
};
typedef struct M6502Fns M6502Fns;

struct M6502DisassemblyInfo {
    // 3-char mnemonic.
    char mnemonic[4];

    // M6502AddrMode.
    uint8_t mode;

    // Count includes the instruction.
    uint8_t num_bytes;

    // Set if undocumented.
    uint8_t undocumented:1;

    // Set if this is a JSR.
    uint8_t jsr:1;

    // Set if this is an RTS.
    uint8_t rts:1;

    // Set if this is RTS or RTI.
    uint8_t return_:1;
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

/* Config for a 6502 that mimics the NMOS part exactly... hopefully.
 *
 * (The M6502 object's ill_fn will never be called; as on a real 6502,
 * every instruction does something.)
 */
extern const M6502Config M6502_defined_config;

/* Config for a 6502 that only executes the defined instructions. If
 * an undefined instruction is encountered, the M6502 object's ill_fn
 * will be called.
 */
extern const M6502Config M6502_nmos6502_config;

/* Config for a 65C02. This does the standard CMOS bits, but not the
 * Rockwell or WDC bits.
 */
extern const M6502Config M6502_cmos6502_config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Appropriate type for holding device IRQ flags. There's 1 bit per
 * device that might cause an IRQ.
 */
typedef uint8_t M6502_DeviceIRQFlags;
typedef uint8_t M6502_DeviceNMIFlags;

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

    /* Read/write pin. (true=read, false=write) */
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
    M6502Word pc;               /* program counter */
    M6502Word s;                /* stack pointer (H byte always 1) */
    M6502Word ad;               /* (internal) data address */
    M6502Word ia;               /* (internal) indirect address */
    M6502Word opcode_pc;        /* address of last opcode fetched */

    // (50)

    /* 8-bit registers */
    uint8_t a;                  /* accumulator */
    uint8_t x;                  /* X */
    uint8_t y;                  /* Y */
    M6502PInternal p;           /* status register */
    uint8_t opcode;             /* (internal) last opcode fetched */
    uint8_t data;               /* (internal) misc - usually operand
                                 * for current instruction */
    uint8_t acarry:1;           /* (internal) address calculation
                                 * carry flag */ /*  */
    uint8_t d1x1:1;             /* (internal) D1x1, active low */

    // (57-58)

    M6502Fn rti_fn;

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

void M6502_Init(M6502 *s,const M6502Config *config);
void M6502_Destroy(M6502 *s);

void M6502_Reset(M6502 *s);

/* Immediately puts the 6502 in a state as if the HLT instruction were
 * just executed.
 */
void M6502_Halt(M6502 *s);

/* If the 6502 is about to execute an instruction, return true. The
 * address of the instruction can be seen on the address bus, and
 * s->read is 1.
 *
 * s->opcode is the last opcode executed, and a/x/y/etc. have
 * appropriate values.
 *
 * When (and only when) M6502_IsAboutToExecute returns true, the 6502
 * state can be (somewhat) safely fiddled with.
 */
int M6502_IsAboutToExecute(M6502 *s);

/* If M6502_IsAboutToExecute, disassemble the last instruction. (Any
 * other time, you just get junk...)
 *
 * The instruction comes from OPCODE; the operand(s) are inferred from
 * the 6502 state. If it wouldn't be obvious from the disassembly, *ia
 * is set to the indirect address used, else -1; *ad is the equivalent
 * for the address used, likewise. (Either may be NULL when the caller
 * doesn't care.)
 */
void M6502_DisassembleLastInstruction(M6502 *s,char *buf,size_t buf_size,int *ia,int *ad);

/* Disassemble the given instruction.
 *
 * PC is the address of the instruction, and its bytes (max 3) are A,
 * B and C.
 *
 * The return value is the number of bytes consumed - always >=1.
 */
//uint16_t M6502Config_DisassembleInstruction(const M6502Config *config,char *buf,size_t buf_size,uint16_t pc,uint8_t a,uint8_t b,uint8_t c);

/* After the end state of an instruction, point the PC at the right
 * place and call this to set things up for the next one. */
void M6502_NextInstruction(M6502 *s);

/* Sets the given device(s)'s IRQ flags as appropriate.
 */
void M6502_SetDeviceIRQ(M6502 *s,M6502_DeviceIRQFlags mask,int wants_irq);
void M6502_SetDeviceNMI(M6502 *s,M6502_DeviceNMIFlags mask,int wants_nmi);

/* Result points into static buffer. */
const char *M6502_GetStateName(M6502 *s);

/* Getter and setter for the P register. Don't access it directly. */
void M6502_SetP(M6502 *s,uint8_t p);
M6502P M6502_GetP(const M6502 *s);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif
