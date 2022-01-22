#include <shared/system.h>
#include <shared/CommandLineParser.h>
#include <shared/testing.h>
#include <shared/path.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <6502/6502.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

/* Test driver for Wolfgang Lorenz's 6502 test suite. */

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    std::string log_fname;
    std::string start_file;
    std::string end_file;
    bool test_disassembler=false;
    bool running_disassembly=false;
};
typedef struct Options Options;

static uint8_t g_mem[65536];
static std::string g_last_file;
static M6502Fns g_test_opcodes[256];
static bool g_done,g_bork;
static Options g_options;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class LogPrinter6502Log:
    public LogPrinter
{
public:
    FILE *f=nullptr;

    void Print(const char *str,size_t str_len) override {
        if(this->f) {
            fwrite(str,str_len,1,this->f);
            fflush(this->f);
        }

        if(g_options.running_disassembly) {
            fwrite(str,str_len,1,stdout);
            fflush(stdout);
        }
    }
protected:
private:
};

static LogPrinter6502Log g_log_printer_6502;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(6502,"6502",&g_log_printer_6502,false);
LOG_DEFINE(TEST,"TEST",&log_printer_stdout);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t g_irq[]={
    0x48,                       // PHA
    0x8A,                       // TXA
    0x48,                       // PHA
    0x98,                       // TYA
    0x48,                       // PHA
    0xBA,                       // TSX
    0xBD,0x04,0x01,             // LDA    $0104,X
    0x29,0x10,                  // AND    #$10
    0xF0,0x03,                  // BEQ    $FF58
    0x6C,0x16,0x03,             // JMP    ($0316)
    0x6C,0x14,0x03,             // JMP    ($0314)
};

#define HACK_OPCODE (2)

static void LoadFileAndReset(const std::string &fname,M6502 *s) {
    memset(g_mem,0,sizeof g_mem);

    if(!g_options.end_file.empty()) {
        if(PathCompare(fname,g_options.end_file)==0) {
            g_done=1;
            return;
        }
    }

    /* In any event, always stop at trap1, because that's the end of
     * the non-C64 tests. */
    if(PathCompare(fname,"trap1")==0) {
        LOGF(TEST,"\n");
        g_done=1;
        return;
    }

    if(fname.empty()) {
        // Reload the last one...
    } else {
        g_last_file=PathJoined(LORENZ_FOLDER_NAME,fname);
    }

    FILE *f=fopen(g_last_file.c_str(),"rb");
    TEST_NON_NULL(f);

    int l=fgetc(f);
    TEST_TRUE(l!=-1);

    int h=fgetc(f);
    TEST_TRUE(h!=-1);

    M6502Word addr;
    addr.b.h=(uint8_t)h;
    addr.b.l=(uint8_t)l;

    int c;
    while((c=fgetc(f))!=EOF) {
        g_mem[addr.w++]=(uint8_t)c;
    }

    fclose(f);
    f=NULL;

    //LOGF(TEST,"Loaded file: %s\n",fname);

    g_mem[0x0002]=0x00;
    g_mem[0xA002]=0x00;
    g_mem[0xA003]=0x80;
    g_mem[0xFFFE]=0x48;
    g_mem[0xFFFF]=0xFF;
    g_mem[0x01FE]=0xFF;
    g_mem[0x01FF]=0x7F;

    memcpy(&g_mem[0xff48],g_irq,sizeof g_irq);

    g_mem[0xffd2]=HACK_OPCODE;
    g_mem[0xffd3]=0x60;

    g_mem[0xe16f]=HACK_OPCODE;
    g_mem[0xe170]=0x60;

    g_mem[0xffe4]=HACK_OPCODE;
    g_mem[0xffe5]=0x60;

    g_mem[0x8000]=HACK_OPCODE;
    g_mem[0xa474]=HACK_OPCODE;

    s->s.b.l=0xfd;
    s->p.bits.i=1;
    s->pc.w=0x816;
    //M6502_NextInstruction(s);
    s->tfn=&M6502_NextInstruction;

    //if(strcmp(fname,"ldab")==0) {
    //    LOGF_ENABLE(6502);
    //}
}

static void HackOpcode(M6502 *s) {
    uint16_t pc=s->pc.w-1;
    switch(pc) {
    default:
        ASSERT(0);
        break;

    case 0x8000:
    case 0xa474:
        g_bork=1;
        break;

    case 0xffd2:
        /* Print character */
        {
            g_mem[0x30c]=0;

            //++s->s.b.l;
            //s->pc.b.l=g_mem[s->s.w];

            //++s->s.b.l;
            //s->pc.b.h=g_mem[s->s.w];

            LOGF(TEST,"%c",s->a=='\r'?'\n':s->a);
            LOG(TEST).Flush();

            M6502_NextInstruction(s);
        }
        break;

    case 0xe16f:
        /* Load */
        {
            M6502Word addr;
            addr.b.l=g_mem[0xbb];
            addr.b.h=g_mem[0xbc];

            std::string fname((const char *)&g_mem[addr.w],g_mem[0xb7]);

            LoadFileAndReset(fname,s);
        }
        break;

    case 0xffe4:
        /* Scan keyboard */
        {
            s->a=3;

            //++s->s.b.l;
            //s->pc.b.l=g_mem[s->s.w];

            //++s->s.b.l;
            //s->pc.b.h=g_mem[s->s.w];

            M6502_NextInstruction(s);
        }
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* static void DisassembleByte(uint16_t pc,const char *prefix,const char *suffix) { */
/*     LOGF(6502," %s0x%02x%s",prefix,g_mem[(uint16_t)(pc+1)],suffix); */
/* } */

/* static void DisassembleWord(uint16_t pc,const char *prefix,const char *suffix) { */
/*     LOGF(6502," %s0x%02x%02x%s",prefix,g_mem[(uint16_t)(pc+2)],g_mem[(uint16_t)(pc+1)],suffix); */
/* } */

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool DoCommandLine(int argc,char *argv[]) {
    CommandLineParser p("Run Lorenz 6502 tests","[OPTIONS]");

    p.AddOption('d',"test-disassembler").SetIfPresent(&g_options.test_disassembler).Help("test disassembler");
    p.AddOption('l',"log").Arg(&g_options.log_fname).Meta("FILE").Help("write 6502 instruction log to FILE");
    p.AddOption('s',"start-file").Arg(&g_options.start_file).Meta("FILE").Help("stem of name of test suite file to start with");
    p.AddOption('e',"end-file").Arg(&g_options.end_file).Meta("FILE").Help("stem of name of test suite file to end on - when this file is loaded, tests will stop");
    p.AddOption('r').SetIfPresent(&g_options.running_disassembly).Help("continuous disassembly to stdout");
    p.AddHelpOption();

    std::vector<std::string> other_args;
    if(!p.Parse(argc,argv,&other_args)) {
        return false;
    }

    if(!other_args.empty()) {
        fprintf(stderr,"FATAL: additional arguments supplied\n");
        return false;
    }

    return true;
}

// C:\tom\b2\etc\testsuite-2.15\bin

static void HandleIllegalOpcode(M6502 *s,void *context) {
    (void)s,(void)context;

    fprintf(stderr,"Bork - encountered illegal opcode 0x%02x at ~0x%04x or so\n",s->opcode,s->pc.w);

    g_bork=1;
}

struct AddrModeDisassemblyInfo {
    size_t num_operand_bytes;
    const char *prefix,*suffix;
};
typedef struct AddrModeDisassemblyInfo AddrModeDisassemblyInfo;

static AddrModeDisassemblyInfo GetDisassemblyInfoForAddrMode(M6502AddrMode mode) {
    switch(mode) {
    case M6502AddrMode_IMP:
        return{0,"",""};

    case M6502AddrMode_IMM:
        return{1,"#",""};

    case M6502AddrMode_ZPG:
        return{1,"",""};

    case M6502AddrMode_ZPX:
        return{1,"",",x"};

    case M6502AddrMode_ZPY:
        return{1,"",",y"};

    case M6502AddrMode_INX:
        return{1,"(",",x)"};

    case M6502AddrMode_INY:
        return{1,"(","),y"};

    case M6502AddrMode_ABS:
        return{2,"",""};

    case M6502AddrMode_ABX:
        return{2,"",",x"};

    case M6502AddrMode_ABY:
        return{2,"",",y"};

    case M6502AddrMode_IND:
        return{2,"(",")"};

    case M6502AddrMode_ACC:
        return{0," A",""};

    default:
        return{};
    }
}

int main(int argc,char *argv[]) {
    if(!DoCommandLine(argc,argv)) {
        return 1;
    }

    if(g_options.running_disassembly) {
        LOG(6502).Enable();
    }

    M6502 s;
    M6502_Init(&s,&M6502_nmos6502_config);

    memcpy(g_test_opcodes,s.fns,sizeof g_test_opcodes);

    /* TEST_NULL(g_test_opcodes[2].t1fn); */
    /* TEST_NULL(g_test_opcodes[2].ifn); */

    g_test_opcodes[HACK_OPCODE].t0fn=&HackOpcode;

    s.fns=g_test_opcodes;

    s.ill_fn=&HandleIllegalOpcode;

    LoadFileAndReset(g_options.start_file.empty()?"start":g_options.start_file,&s);

    uint64_t num_cycles=0;
    uint64_t start_ticks=GetCurrentTickCount();

    char last_instr[100]="";
    char next_instr[100]="";
    int last_pc=-1;

    while(!g_done) {
        (*s.tfn)(&s);
        ++num_cycles;

        if(LOG(6502).enabled||g_options.test_disassembler) {
            if(M6502_IsAboutToExecute(&s)) {
                if(last_pc>=0) {
                    //printf("%" PRIu64 "\n",num_cycles);
                    M6502_DisassembleLastInstruction(&s,last_instr,sizeof last_instr,NULL,NULL);

                    LOGF(6502,"%04X %-25s",last_pc,last_instr);

                    //printf("opcode=%02X pc=%04X ad=%04X\n",s.opcode,s.pc.w,s.ad.w);
                    //printf("last_instr: %04X: %s\n",s.abus.w,last_instr);

                    TEST_EQ_SS(last_instr,next_instr);
                }

                last_pc=s.abus.w;

                uint16_t pc=s.abus.w;
                uint8_t opcode=g_mem[pc];
                const M6502DisassemblyInfo *di=&s.config->disassembly_info[opcode];

                // Print registers FIRST...
                M6502P p=M6502_GetP(&s);
                LOGF(6502," - A=$%02X X=$%02X Y=$%02X PC=$%04X S=$%04X P=%c%c%c%c%c%c%c%c - ",
                    s.a,s.x,s.y,s.abus.w,s.s.w,
                    p.bits.n?'N':'_',p.bits.v?'V':'_',p.bits._?'_':'_',p.bits.b?'B':'_',
                    p.bits.d?'D':'_',p.bits.i?'I':'_',p.bits.z?'Z':'_',p.bits.c?'C':'_');

                if(di->mode==M6502AddrMode_REL) {
                    snprintf(next_instr,sizeof next_instr,"%s $%04x",
                        di->mnemonic,
                        pc+2+(int8_t)g_mem[(uint16_t)(pc+1)]);
                } else {
                    AddrModeDisassemblyInfo amdi=GetDisassemblyInfoForAddrMode((M6502AddrMode)di->mode);
                    ASSERT(amdi.prefix);
                    ASSERT(amdi.suffix);

                    if(amdi.num_operand_bytes==0) {
                        snprintf(next_instr,sizeof next_instr,"%s%s%s",
                            di->mnemonic,amdi.prefix,amdi.suffix);
                    } else {
                        unsigned operand=0;

                        for(size_t i=0;i<amdi.num_operand_bytes;++i) {
                            operand|=(unsigned)g_mem[(uint16_t)(pc+1u+i)]<<(i*8);
                        }

                        snprintf(next_instr,sizeof next_instr,"%s %s$%0*x%s",
                            di->mnemonic,
                            amdi.prefix,
                            (int)(amdi.num_operand_bytes*2),
                            operand,
                            amdi.suffix);
                    }
                }

                //printf("next_instr: %04X: %s\n",pc,next_instr);
                LOGF(6502,"\n");
            }
        }

        if(g_bork) {
            if(g_options.log_fname.empty()||g_log_printer_6502.f) {
            bork:;
                fprintf(stderr,"Bork.\n");
                break;
            }

            fprintf(stderr,"Bork - but will retry.\n");

            g_log_printer_6502.f=fopen(g_options.log_fname.c_str(),"wt");
            if(!g_log_printer_6502.f) {
                fprintf(stderr,"WARNING: failed to open \"%s\": %s\n",g_options.log_fname.c_str(),strerror(errno));
                goto bork;
            }

            fprintf((FILE *)g_log_printer_6502.f,"hello...\n");

            LOG(6502).Enable();

            LoadFileAndReset(NULL,&s);
            g_bork=0;//for now, at least...
            continue;//what a horrid loop
        }

        if(s.read) {
            s.dbus=g_mem[s.abus.w];
            //LOGF(6502,"Read 0x%04X: %s=0x%02X",s.abus.w,M6502_GetDBusTargetName(&s),*dbus);
        } else {
            //LOGF(6502,"Write 0x%04X: %s=0x%02X\n",s.abus.w,M6502_GetDBusTargetName(&s),*dbus);
            g_mem[s.abus.w]=s.dbus;
        }
    }

    uint64_t end_ticks=GetCurrentTickCount();

    double num_seconds=GetSecondsFromTicks(end_ticks-start_ticks);
    (void)num_seconds;
    LOGF(TEST,"~%.1fMHz\n",num_cycles/num_seconds/1000000.);
    LOGF(TEST,"%" PRIthou PRIu64 " cycles\n",num_cycles);

    if(g_log_printer_6502.f) {
        fclose(g_log_printer_6502.f);
        g_log_printer_6502.f=NULL;
    }

    if(g_bork&&IsDebuggerAttached()) {
        fprintf(stderr,"press enter to exit...\n");
        getchar();
    }
}
