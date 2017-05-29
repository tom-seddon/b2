#include <shared/system.h>
#include <shared/CommandLineParser.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <stdio.h>
#include <string>
#include <map>
#include <set>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "6502_gen.inl"
#include <shared/enum_end.h>


#include <shared/enum_def.h>
#include "6502_gen.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class LogPrinterFILE:
    public LogPrinter
{
public:
    void SetFILE(FILE *f) {
        m_f=f;
    }

    void Print(const char *str,size_t str_len) override {
        if(m_f) {
            fwrite(str,1,str_len,m_f);
        }
    }
protected:
private:
    FILE *m_f=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LogPrinterFILE g_code_file_printer;

LOG_DEFINE(V,"",&log_printer_stderr_and_debugger,false);
LOG_DEFINE(CODE_FILE,"",&g_code_file_printer,false);
LOG_DEFINE(CODE_STDOUT,"",&log_printer_stdout,false);
LOG_DEFINE(ERR,"",&log_printer_stderr_and_debugger,false);

//#define P(...) LOGF(CODE,__VA_ARGS__)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string CMOS_MNEMONIC_SUFFIX="_cmos";
static const std::string CMOS_FUNCTION_SUFFIX="_CMOS";
static const std::string BCD_CMOS_FUNCTION_SUFFIX="_BCD_CMOS";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    std::string ofname;
    bool to_stdout=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string ToUpper(std::string str) {
    for(size_t i=0;i<str.size();++i) {
        str[i]=toupper(str[i]);
    }

    return str;
}

static void PRINTF_LIKE(1,2) P(const char *fmt,...) {
    char *str;
    {
        va_list v;

        va_start(v,fmt);
        vasprintf(&str,fmt,v);
        va_end(v);
    }

    for(const char *c=str;*c!=0;++c) {
        if(*c=='}') {
            LOG(CODE_FILE).PopIndent();
            LOG(CODE_STDOUT).PopIndent();
        }

        LOG(CODE_FILE).c(*c);
        LOG(CODE_STDOUT).c(*c);

        if(*c=='{') {
            LOG(CODE_FILE).PushIndent(4);
            LOG(CODE_STDOUT).PushIndent(4);
        }
    }

    free(str);
    str=nullptr;
}

static void Sep() {
    P("//////////////////////////////////////////////////////////////////////////\n");
}

static void Sep2() {
    P("\n");
    Sep();
    Sep();
    P("\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<InstrType,std::set<std::string>> INSTR_TYPES={
    {
        InstrType_R,
        {
            "adc","and","bit","cmp","cpx","cpy","eor","lda","ldx","ldy","ora","sbc",
            "adc_cmos","sbc_cmos","bit_cmos",
            "lax","alr","arr","xaa","lxa","axs","anc","las",//illegal
        }
    },
    {
        InstrType_W,
        {
            "sta","stx","sty",
            "sax","ahx","shy","shx","tas",
            "stz",// cmos
        }
    },
    {
        InstrType_Branch,
        {
            "bcc","bcs","beq","bmi","bne","bpl","bvc","bvs",
            "bra",// cmos
        }
    },
    {
        InstrType_IMP,
        {
            "clc","cld","clv","dex","dey","inx","iny","sec","sed","sei","tax","tay","tsx","txa","txs","tya",
            "cli",
            "ill",// illegal
        }
    },
    {
        InstrType_Push,
        {
            "pha","php",
            "phx","phy",// cmos
        }
    },
    {
        InstrType_Pop,
        {
            "pla","plp",
            "plx","ply",// cmos
        }
    },
    {
        InstrType_RMW,
        {
            "asl","dec","inc","lsr","rol","ror",
            "slo","rla","sre","rra","dcp","isc",//illegal
            "trb","tsb",// cmos
        }
    },
};

class Instr {
public:
    std::string mnemonic;
    Mode mode=Mode_Imp;
    bool cmos=false;
    bool undocumented=false;

    Instr()=default;

    Instr(std::string mnemonic_,Mode mode_):
        mnemonic(mnemonic_),
        mode(mode_)
    {
    }

    InstrType GetInstrType() const {
        // Special cases.
        if(this->mnemonic=="nop") {
            if(this->mode==Mode_Imp) {
                return InstrType_IMP;
            } else {
                return InstrType_R;
            }
        }

        if(this->mode==Mode_Acc) {
            return InstrType_IMP;
        }

        for(auto &&it:INSTR_TYPES) {
            if(it.second.count(this->mnemonic)>0) {
                return it.first;
            }
        }

        return InstrType_Unknown;
    }

    std::string GetDisassemblyMnemonic() const {
        std::string m=this->mnemonic;

        if(m.size()>=CMOS_MNEMONIC_SUFFIX.size()) {
            size_t index=m.size()-CMOS_MNEMONIC_SUFFIX.size();
            if(m.substr(index)==CMOS_MNEMONIC_SUFFIX) {
                m=m.substr(0,index);
            }
        }

        return m;
    }

    Mode GetDisassemblyMode() const {
        switch(this->mode) {
        default:
            return this->mode;

        case Mode_Nop11_CMOS:
            return Mode_Imp;

        case Mode_Nop22_CMOS:
        case Mode_Nop23_CMOS:
        case Mode_Nop24_CMOS:
            return Mode_Zpg;

        case Mode_Nop34_CMOS:
        case Mode_Nop38_CMOS:
            return Mode_Abs;
        }
    }

    void GetTFunAndIFun(std::string *tfun,std::string *ifun) const {
        if(this->mnemonic=="brk") {
            if(this->cmos) {
                *tfun="T0_BRK_CMOS";
            } else {
                *tfun="T0_Interrupt";
            }
            ifun->clear();
        } else {
            InstrType type=this->GetInstrType();

            if(type==InstrType_Unknown) {
                std::string suffix;

                if(this->mnemonic=="rts"||this->mnemonic=="rti"||this->mnemonic=="jsr") {
                    suffix=ToUpper(this->mnemonic);
                } else if(this->mnemonic=="jmp") {
                    // All the JMPs are special-cased.
                    suffix=ToUpper(this->mnemonic)+"_"+ToUpper(GetModeEnumName(this->mode));

                    if(this->cmos&&this->mode==Mode_Ind) {
                        suffix+=CMOS_FUNCTION_SUFFIX;
                    }
                } else if(this->mnemonic=="hlt") {
                    suffix="HLT";
                } else {
                    ASSERT(false);
                }

                *tfun="T0_"+suffix;
                ifun->clear();
            } else {
                *ifun=ToUpper(this->mnemonic);

                if(this->mode==Mode_Imp||this->mode==Mode_Rel) {
                    *tfun=std::string("T0_")+GetInstrTypeEnumName(type);
                } else if(this->mode==Mode_Acc) {
                    *tfun="T0_IMP";
                    *ifun+="A";
                } else {
                    Mode m=this->mode;

                    if(this->cmos) {
                        // Some CMOS hacks.

                        if(m==Mode_Abx&&this->IsShiftInstruction()) {
                            // 1 cycle short when no page boundary
                            // crossed, totally unlike the other RMW
                            // instructions.
                            //
                            // No need for the _CMOS suffix for the tfun,
                            // as the enum name includes it.
                            m=Mode_Abx2_CMOS;
                        } else if(this->IsBCDInstruction()) {
                            // 1 cycle extra when in BCD mode.
                            *tfun=BCD_CMOS_FUNCTION_SUFFIX;

                            // The ifun is different too. The flags are
                            // set differently.
                            *ifun+=CMOS_FUNCTION_SUFFIX;
                        } else if(type==InstrType_RMW) {
                            // Last two cycles are RW rather than WW.
                            *tfun=CMOS_FUNCTION_SUFFIX;
                        }
                    }

                    *tfun=std::string("T0_")+GetInstrTypeEnumName(type)+"_"+ToUpper(GetModeEnumName(m))+*tfun;
                }
            }
        }
    }

    bool IsShiftInstruction() const {
        return this->mnemonic=="asl"||this->mnemonic=="lsr"||this->mnemonic=="rol"||this->mnemonic=="ror";
    }

    bool IsBCDInstruction() const {
        return this->mnemonic=="adc"||this->mnemonic=="sbc";
    }
};

#define I(OPCODE,MNEMONIC,MODE)\
BEGIN_MACRO {\
instructions[OPCODE]=Instr(MNEMONIC,Mode_##MODE);\
} END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<uint8_t,Instr> GetDefinedInstructions() {
    std::map<uint8_t,Instr> instructions;

    I(0x00,"brk",Imp);I(0x01,"ora",Inx);I(0x05,"ora",Zpg);I(0x06,"asl",Zpg);
    I(0x08,"php",Imp);I(0x09,"ora",Imm);I(0x0a,"asl",Acc);I(0x0d,"ora",Abs);
    I(0x0e,"asl",Abs);I(0x10,"bpl",Rel);I(0x11,"ora",Iny);I(0x15,"ora",Zpx);
    I(0x16,"asl",Zpx);I(0x18,"clc",Imp);I(0x19,"ora",Aby);I(0x1d,"ora",Abx);
    I(0x1e,"asl",Abx);I(0x20,"jsr",Abs);I(0x21,"and",Inx);I(0x24,"bit",Zpg);
    I(0x25,"and",Zpg);I(0x26,"rol",Zpg);I(0x28,"plp",Imp);I(0x29,"and",Imm);
    I(0x2a,"rol",Acc);I(0x2c,"bit",Abs);I(0x2d,"and",Abs);I(0x2e,"rol",Abs);
    I(0x30,"bmi",Rel);I(0x31,"and",Iny);I(0x35,"and",Zpx);I(0x36,"rol",Zpx);
    I(0x38,"sec",Imp);I(0x39,"and",Aby);I(0x3d,"and",Abx);I(0x3e,"rol",Abx);
    I(0x40,"rti",Imp);I(0x41,"eor",Inx);I(0x45,"eor",Zpg);I(0x46,"lsr",Zpg);
    I(0x48,"pha",Imp);I(0x49,"eor",Imm);I(0x4a,"lsr",Acc);I(0x4c,"jmp",Abs);
    I(0x4d,"eor",Abs);I(0x4e,"lsr",Abs);I(0x50,"bvc",Rel);I(0x51,"eor",Iny);
    I(0x55,"eor",Zpx);I(0x56,"lsr",Zpx);I(0x58,"cli",Imp);I(0x59,"eor",Aby);
    I(0x5d,"eor",Abx);I(0x5e,"lsr",Abx);I(0x60,"rts",Imp);I(0x61,"adc",Inx);
    I(0x65,"adc",Zpg);I(0x66,"ror",Zpg);I(0x68,"pla",Imp);I(0x69,"adc",Imm);
    I(0x6a,"ror",Acc);I(0x6c,"jmp",Ind);I(0x6d,"adc",Abs);I(0x6e,"ror",Abs);
    I(0x70,"bvs",Rel);I(0x71,"adc",Iny);I(0x75,"adc",Zpx);I(0x76,"ror",Zpx);
    I(0x78,"sei",Imp);I(0x79,"adc",Aby);I(0x7d,"adc",Abx);I(0x7e,"ror",Abx);
    I(0x81,"sta",Inx);I(0x84,"sty",Zpg);I(0x85,"sta",Zpg);I(0x86,"stx",Zpg);
    I(0x88,"dey",Imp);I(0x8a,"txa",Imp);I(0x8c,"sty",Abs);I(0x8d,"sta",Abs);
    I(0x8e,"stx",Abs);I(0x90,"bcc",Rel);I(0x91,"sta",Iny);I(0x94,"sty",Zpx);
    I(0x95,"sta",Zpx);I(0x96,"stx",Zpy);I(0x98,"tya",Imp);I(0x99,"sta",Aby);
    I(0x9a,"txs",Imp);I(0x9d,"sta",Abx);I(0xa0,"ldy",Imm);I(0xa1,"lda",Inx);
    I(0xa2,"ldx",Imm);I(0xa4,"ldy",Zpg);I(0xa5,"lda",Zpg);I(0xa6,"ldx",Zpg);
    I(0xa8,"tay",Imp);I(0xa9,"lda",Imm);I(0xaa,"tax",Imp);I(0xac,"ldy",Abs);
    I(0xad,"lda",Abs);I(0xae,"ldx",Abs);I(0xb0,"bcs",Rel);I(0xb1,"lda",Iny);
    I(0xb4,"ldy",Zpx);I(0xb5,"lda",Zpx);I(0xb6,"ldx",Zpy);I(0xb8,"clv",Imp);
    I(0xb9,"lda",Aby);I(0xba,"tsx",Imp);I(0xbc,"ldy",Abx);I(0xbd,"lda",Abx);
    I(0xbe,"ldx",Aby);I(0xc0,"cpy",Imm);I(0xc1,"cmp",Inx);I(0xc4,"cpy",Zpg);
    I(0xc5,"cmp",Zpg);I(0xc6,"dec",Zpg);I(0xc8,"iny",Imp);I(0xc9,"cmp",Imm);
    I(0xca,"dex",Imp);I(0xcc,"cpy",Abs);I(0xcd,"cmp",Abs);I(0xce,"dec",Abs);
    I(0xd0,"bne",Rel);I(0xd1,"cmp",Iny);I(0xd5,"cmp",Zpx);I(0xd6,"dec",Zpx);
    I(0xd8,"cld",Imp);I(0xd9,"cmp",Aby);I(0xdd,"cmp",Abx);I(0xde,"dec",Abx);
    I(0xe0,"cpx",Imm);I(0xe1,"sbc",Inx);I(0xe4,"cpx",Zpg);I(0xe5,"sbc",Zpg);
    I(0xe6,"inc",Zpg);I(0xe8,"inx",Imp);I(0xe9,"sbc",Imm);I(0xea,"nop",Imp);
    I(0xec,"cpx",Abs);I(0xed,"sbc",Abs);I(0xee,"inc",Abs);I(0xf0,"beq",Rel);
    I(0xf1,"sbc",Iny);I(0xf5,"sbc",Zpx);I(0xf6,"inc",Zpx);I(0xf8,"sed",Imp);
    I(0xf9,"sbc",Aby);I(0xfd,"sbc",Abx);I(0xfe,"inc",Abx);

    return instructions;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddBulkInstructions(std::map<uint8_t,Instr> *instructions,const char *mnemonic,Mode mode,const std::initializer_list<uint8_t> &opcodes) {
    for(uint8_t opcode:opcodes) {
        ASSERT(instructions->count(opcode)==0);
        (*instructions)[opcode]=Instr(mnemonic,mode);
    }
}

static void MarkInstructions(std::map<uint8_t,Instr> *instructions,bool Instr::*mptr) {
    for(auto &&it:*instructions) {
        it.second.*mptr=true;
    }
}

static std::map<uint8_t,Instr> GetUndefinedInstructions() {
    std::map<uint8_t,Instr> instructions;

    I(0x0c,"nop",Abs);
    I(0x07,"slo",Zpg);I(0x17,"slo",Zpx);I(0x03,"slo",Inx);I(0x13,"slo",Iny);I(0x0f,"slo",Abs);I(0x1f,"slo",Abx);I(0x1b,"slo",Aby);
    I(0x27,"rla",Zpg);I(0x37,"rla",Zpx);I(0x23,"rla",Inx);I(0x33,"rla",Iny);I(0x2f,"rla",Abs);I(0x3f,"rla",Abx);I(0x3b,"rla",Aby);
    I(0x47,"sre",Zpg);I(0x57,"sre",Zpx);I(0x43,"sre",Inx);I(0x53,"sre",Iny);I(0x4f,"sre",Abs);I(0x5f,"sre",Abx);I(0x5b,"sre",Aby);
    I(0x67,"rra",Zpg);I(0x77,"rra",Zpx);I(0x63,"rra",Inx);I(0x73,"rra",Iny);I(0x6f,"rra",Abs);I(0x7f,"rra",Abx);I(0x7b,"rra",Aby);
    I(0xc7,"dcp",Zpg);I(0xd7,"dcp",Zpx);I(0xc3,"dcp",Inx);I(0xd3,"dcp",Iny);I(0xcf,"dcp",Abs);I(0xdf,"dcp",Abx);I(0xdb,"dcp",Aby);
    I(0xe7,"isc",Zpg);I(0xf7,"isc",Zpx);I(0xe3,"isc",Inx);I(0xf3,"isc",Iny);I(0xef,"isc",Abs);I(0xff,"isc",Abx);I(0xfb,"isc",Aby);
    I(0x87,"sax",Zpg);I(0x97,"sax",Zpy);I(0x83,"sax",Inx);/****************/I(0x8f,"sax",Abs);
    I(0xa7,"lax",Zpg);I(0xb7,"lax",Zpy);I(0xa3,"lax",Inx);I(0xb3,"lax",Iny);I(0xaf,"lax",Abs);/****************/I(0xbf,"lax",Aby);
    I(0x4b,"alr",Imm);
    I(0x6b,"arr",Imm);
    I(0x8b,"xaa",Imm);
    I(0xab,"lxa",Imm);
    I(0xcb,"axs",Imm);
    I(0x93,"ahx",Iny);I(0x9f,"ahx",Aby);
    I(0x9c,"shy",Abx);
    I(0x9e,"shx",Aby);
    I(0x9b,"tas",Aby);
    I(0x0b,"anc",Imm);I(0x2b,"anc",Imm);
    I(0xbb,"las",Aby);
    I(0xeb,"sbc",Imm);

    AddBulkInstructions(&instructions,"nop",Mode_Imp,{0x1a,0x3a,0x5a,0x7a,0xda,0xfa});
    AddBulkInstructions(&instructions,"nop",Mode_Imm,{0x80,0x82,0x89,0xc2,0xe2});
    AddBulkInstructions(&instructions,"nop",Mode_Zpg,{0x04,0x44,0x64});
    AddBulkInstructions(&instructions,"nop",Mode_Zpy,{0x14,0x34,0x54,0x74,0xd4,0xf4});
    AddBulkInstructions(&instructions,"nop",Mode_Abx,{0x1c,0x3c,0x5c,0x7c,0xdc,0xfc});
    AddBulkInstructions(&instructions,"hlt",Mode_Imp,{0x02,0x12,0x22,0x32,0x42,0x52,0x62,0x72,0x92,0xB2,0xD2,0xF2});

    MarkInstructions(&instructions,&Instr::undocumented);

    return instructions;
}

static std::map<uint8_t,Instr> GetUndefinedTrapInstructions() {
    std::map<uint8_t,Instr> defined_instructions=GetDefinedInstructions();

    std::map<uint8_t,Instr> instructions;

    for(size_t i=0;i<256;++i) {
        if(defined_instructions.count((uint8_t)i)==0) {
            instructions[(uint8_t)i]=Instr("ill",Mode_Imp);
        }
    }

    MarkInstructions(&instructions,&Instr::undocumented);

    return instructions;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<uint8_t,Instr> GetCMOSInstructions() {
    std::map<uint8_t,Instr> instructions;

    // Start with the default set.
    instructions=GetDefinedInstructions();

    // Add spot fixes.
    //instructions[0x00]=Instr("brk_cmos",Mode_Imp);
    //instructions[0x6c]=Instr("jmp",Mode_Ind_CMOS);

    // Add new instructions.
    I(0xda,"phx",Imp);I(0x5a,"phy",Imp);I(0xfa,"plx",Imp);I(0x7a,"ply",Imp);
    I(0x64,"stz",Zpg);I(0x74,"stz",Zpx);I(0x9c,"stz",Abs);I(0x9e,"stz",Abx);
    I(0x14,"trb",Zpg);I(0x1c,"trb",Abs);
    I(0x04,"tsb",Zpg);I(0x0c,"tsb",Abs);
    I(0x80,"bra",Rel);

    // Add new addressing modes.
    I(0x72,"adc",Inz);
    I(0x32,"and",Inz);
    I(0xD2,"cmp",Inz);
    I(0x52,"eor",Inz);
    I(0xB2,"lda",Inz);
    I(0x12,"ora",Inz);
    I(0xF2,"sbc",Inz);
    I(0x92,"sta",Inz);
    I(0x89,"bit_cmos",Imm);I(0x34,"bit",Zpx);I(0x3c,"bit",Abx);
    I(0x3a,"dec",Acc);
    I(0x1a,"inc",Acc);
    I(0x7c,"jmp",Indx);

    // Add new NOPs.
    for(int lsn:{0x03,0x07,0x0b,0x0f}) {
        for(int msn=0;msn<16;++msn) {
            AddBulkInstructions(&instructions,"nop",Mode_Nop11_CMOS,{(uint8_t)(msn<<4|lsn)});
        }
    }

    AddBulkInstructions(&instructions,"nop",Mode_Nop22_CMOS,{0x02,0x22,0x42,0x62,0x82,0xc2,0xe2});
    AddBulkInstructions(&instructions,"nop",Mode_Nop23_CMOS,{0x44});
    AddBulkInstructions(&instructions,"nop",Mode_Nop24_CMOS,{0x54,0xd4,0xf4});
    AddBulkInstructions(&instructions,"nop",Mode_Nop38_CMOS,{0x5c});
    AddBulkInstructions(&instructions,"nop",Mode_Nop34_CMOS,{0xdc,0xfc});

    // Check every opcode is covered.
    for(int i=0;i<256;++i) {
        ASSERT(instructions.count((uint8_t)i)==1);
    }

    MarkInstructions(&instructions,&Instr::cmos);

    return instructions;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class InstrGen;

static const std::map<std::string,std::string> ADDR_EXPRS={
    {"pc","s->pc.w"},
    {"pc++","s->pc.w++"},
    {"adl","s->ad.b.l"},
    {"ad","s->ad.w"},
    {"ial","s->ia.b.l"},
    {"ial+1","(uint8_t)(s->ia.b.l+1)"},
    {"sp","s->s.w"},
    {"ia","s->ia.w"},
    {"ia+1","s->ia.w+1"},
    {"ial+x","(uint8_t)(s->ia.b.l+s->x)"},
    {"ial+x+1","(uint8_t)(s->ia.b.l+s->x+1)"},
    {"nmil","0xfffa"},
    {"nmih","0xfffb"},
    {"resl","0xfffc"},
    {"resh","0xfffd"},
    {"irql","0xfffe"},
    {"irqh","0xffff"},
};

static const std::map<std::string,std::string> WHAT_EXPRS={
    {"adl","ad.b.l"},
    {"adh","ad.b.h"},
    {"data","data"},
    {"data!","data"},
    {"ial","ia.b.l"},
    {"iah","ia.b.h"},
    {"pch","pc.b.h"},
    {"pcl","pc.b.l"},
    {"p","p.value"},
};

struct Cycle {
    bool read=true;
    std::string addr,what,action;
    //void (*phase1_fn)(const InstrGen *gen)=nullptr;

    Cycle(bool read_,std::string addr_,const char *what_,const char *action_):
        read(read_),
        addr(std::move(addr_)),
        what(what_?what_:""),
        action(action_?action_:"")
    {
    }
};

class InstrGen {
public:
    std::string stem,description;
    std::vector<Cycle> cycles;
    char index=0;

    InstrGen(std::string stem_,std::string description_,const std::initializer_list<Cycle> &cycles_,char index_=0):
        stem(std::move(stem_)),
        description(std::move(description_)),
        cycles(cycles_),
        index(index_)
    {
    }

    void GenerateC() const {
        Sep();
        Sep();
        P("//\n");
        P("// %s\n",this->description.c_str());
        P("//\n");
        Sep();
        Sep();
        P("\n");

        for(size_t i=0;i<this->cycles.size();++i) {
            //const Cycle *c=&this->cycles[i];

            P("static void T%zu_%s(M6502 *);\n",1+i,this->stem.c_str());
        }

        P("\n");

        int i=-1;
        while(i<(int)this->cycles.size()) {
            std::string fn_name="T"+std::to_string(1+i)+"_"+this->stem;

            P("static void %s(M6502 *s) {\n",fn_name.c_str());
            P("/* T%d phase 2 */\n",i+1);

            if(i<0) {
                P("/* (decode - already done) */\n");
            } else {
                this->GeneratePhase2(&this->cycles[i]);
            }

            P("\n");

            ++i;

            P("/* T%d phase 1 */\n",(i+1)%((int)this->cycles.size()+1));
            if(i==(int)this->cycles.size()) {
                P("M6502_NextInstruction(s);\n");
            } else {
                this->GeneratePhase1(&this->cycles[i]);
                P("s->tfn=&T%d_%s;\n",i+1,this->stem.c_str());

                if(i==(int)this->cycles.size()-1) {
                    this->GenerateD1x1();
                }
            }
            P("}\n\n");
        }
    }

    std::vector<std::string> GetFnNames() const {
        std::vector<std::string> fn_names;

        // +1 because T0 is implied.
        for(size_t i=0;i<this->cycles.size()+1;++i) {
            fn_names.emplace_back("T"+std::to_string(i)+"_"+this->stem);
        }

        return fn_names;
    }
protected:
private:
    void GeneratePhase1(const Cycle *c) const {
        bool set_acarry=false;

        if(c->addr=="1.ad+index") {
            set_acarry=true;

            this->GenerateIndexedLSB("ad");
        } else if(c->addr=="1.ia+index") {
            set_acarry=true;
            this->GenerateIndexedLSB("ia");
        } else if(c->addr=="2.ad+index") {
            this->GenerateIndexedMSB("ad");
        } else if(c->addr=="2.ia+index") {
            this->GenerateIndexedMSB("ia");
        } else if(c->addr=="1.ia+x_cmos") {
            // This is a guess.
            P("s->abus.w=s->ia.w+s->x;\n");
        } else if(c->addr=="2.ia+x_cmos") {
            // This is a guess.
            P("s->abus.w=s->ia.w+s->x;\n");
        } else if(c->addr=="3.ia+x_cmos") {
            // This is a guess.
            P("s->abus.w=s->ia.w+s->x+1;\n");
        } else if(c->addr=="sp++") {
            P("s->abus=s->s;\n");
            P("++s->s.b.l;\n");
        } else if(c->addr=="sp--") {
            P("s->abus=s->s;\n");
            P("--s->s.b.l;\n");
        } else if(c->addr=="adl+index") {
            P("s->abus.b.l=s->ad.b.l+s->%c;\n",this->index);
            P("s->abus.b.h=0;\n");
        } else if(c->addr=="ia+1(nocarry)") {
            P("s->abus=s->ia;\n");
            P("++s->abus.b.l; /* don't fix up the high byte */\n");
        } else {
            auto &&it=ADDR_EXPRS.find(c->addr);
            ASSERT(it!=ADDR_EXPRS.end());
            P("s->abus.w=%s;\n",it->second.c_str());
        }

        if(!c->read) {
            auto &&it=WHAT_EXPRS.find(c->what);
            ASSERT(it!=WHAT_EXPRS.end());
            P("s->dbus=s->%s;\n",it->second.c_str());
        }

        P("s->read=%d;\n",c->read);
        // P("    SET_DBUS(%s);\n"%what)

        if(c->action=="maybe_call") {
            ASSERT(set_acarry);
            this->GenerateConditionalD1x1("!s->acarry");
        } else if(c->action=="maybe_call_bcd_cmos") {
            ASSERT(set_acarry);
            this->GenerateConditionalD1x1("!s->p.bits.d&&!s->acarry");
        } else if(c->action=="call_bcd_cmos") {
            this->GenerateConditionalD1x1("!s->p.bits.d");
        }
    }

    void GeneratePhase2(const Cycle *c) const {
        if(c->read) {
            if(c->what.empty()) {
                P("/* ignore dummy read */\n");
            } else {
                auto &&it=WHAT_EXPRS.find(c->what);
                ASSERT(it!=WHAT_EXPRS.end());
                P("s->%s=s->dbus;\n",it->second.c_str());
            }
        }

        if(c->action.empty()) {
            // ignore...
        } else if(c->action=="maybe_call") {
            P("if(!s->acarry) {\n");
            P("/* No carry - done. */\n");
            this->GenerateCallIFn();
            P("\n");
            P("/* T0 phase 1 */\n");
            P("M6502_NextInstruction(s);\n");
            P("return;\n");
            P("}\n");
        } else if(c->action=="call") {
            this->GenerateCallIFn();
        } else if(c->action=="maybe_call_bcd_cmos") {
            P("    if(!s->p.bits.d) {\n");
            P("if(!s->acarry) {\n");
            P("/* No carry, no decimal - done. */\n");
            this->GenerateCallIFn();
            P("\n");
            P("/* T0 phase 1 */\n");
            P("M6502_NextInstruction(s);\n");
            P("return;\n");
            P("}\n");
            P("}\n");
        } else if(c->action=="call_bcd_cmos") {
            this->GenerateCallIFn();
            P("if(!s->p.bits.d) {\n");
            P("/* No decimal - done. */\n");
            P("\n");
            P("/* T0 phase 1 */\n");
            P("M6502_NextInstruction(s);\n");
            P("return;\n");
            P("}\n");
        } else if(c->action=="jmp") {
            P("s->pc=s->ad;\n");
        } else if(c->action=="rti") {
            P("if(s->rti_fn) {\n");
            P("(*s->rti_fn)(s);\n");
            P("}\n");
        } else {
            ASSERT(false);
        }
    }

    void GenerateCallIFn() const {
        P("(*s->ifn)(s);\n");
        P("#ifdef _DEBUG\n");
        P("s->ifn=NULL;\n");
        P("#endif\n");
    }

    void GenerateConditionalD1x1(const char *cond) const {
        P("if(%s) {\n",cond);
        this->GenerateD1x1();
        P("}\n");
    }

    void GenerateD1x1() const {
        P("CheckForInterrupts(s);\n");
    }

    void GenerateIndexedLSB(const char *addr) const {
        ASSERT(this->index!=0);

        P("s->abus.w=s->%s.b.l+s->%c;\n",addr,this->index);
        P("s->acarry=s->abus.b.h;\n");
        P("s->abus.b.h=s->%s.b.h;\n",addr);
    }

    void GenerateIndexedMSB(const char *addr) const {
        ASSERT(this->index!=0);

        P("s->abus.w=s->%s.w+s->%c;\n",addr,this->index);
    }
};

static std::vector<InstrGen> GetAll() {
    std::vector<InstrGen> gs;

#define R(...) Cycle(true,__VA_ARGS__)
#define W(...) Cycle(false,__VA_ARGS__)
#define G(...) gs.push_back(InstrGen(__VA_ARGS__))

    // 1-byte instructions.
    {
        G("IMP","1-byte instructions",{
            R("pc","data!","call")
        });
    }

    // Read instructions.
    {
        G("R_IMM","Read/Immediate",{
            R("pc++","data","call")
        });

        G("R_ZPG","Read/Zero page",{
            R("pc++","adl",nullptr),
            R("adl","data","call")
        });

        G("R_ABS","Read/Absolute",{
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("ad","data","call")
        });

        G("R_INX","Read/Indirect,X",{
            R("pc++","ial",nullptr),
            R("ial","data!",nullptr),
            R("ial+x","adl",nullptr),
            R("ial+x+1","adh",nullptr),
            R("ad","data","call")
        });

        std::initializer_list<Cycle> r_abs_indexed={
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("1.ad+index","data!","maybe_call"),
            R("2.ad+index","data","call"),
        };

        G("R_ABX","Read/Absolute,X",r_abs_indexed,'x');
        G("R_ABY","Read/Absolute,Y",r_abs_indexed,'y');

        std::initializer_list<Cycle> r_zpg_indexed={
            R("pc++","adl",nullptr),
            R("adl","data!",nullptr),
            R("adl+index","data","call")
        };

        G("R_ZPX","Read/Zero page,X",r_zpg_indexed,'x');
        G("R_ZPY","Read/Zero page,Y",r_zpg_indexed,'y');

        G("R_INY","Read/Indirect,Y",{
            R("pc++","ial",nullptr),
            R("ial","adl",nullptr),
            R("ial+1","adh",nullptr),
            R("1.ad+index","data!","maybe_call"),
            R("2.ad+index","data","call")
        },'y');

        G("R_INZ","Read/Indirect Zero Page",{
            R("pc++","ial",nullptr),
            R("ial","adl",nullptr),
            R("ial+1","adh",nullptr),
            R("ad","data","call")
        });
    }

    // Read instructions with BCD (CMOS).
    {
        G("R_IMM_BCD_CMOS","Read/Immediate",{
            R("pc++","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        });

        G("R_ZPG_BCD_CMOS","Read/Zero page",{
            R("pc++","adl",nullptr),
            R("adl","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        });

        G("R_ABS_BCD_CMOS","Read/Absolute",{
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("ad","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        });

        G("R_INX_BCD_CMOS","Read/Indirect,X",{
            R("pc++","ial",nullptr),
            R("ial","data!",nullptr),
            R("ial+x","adl",nullptr),
            R("ial+x+1","adh",nullptr),
            R("ad","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        });

        std::initializer_list<Cycle> r_abs_indexed_bcd_cmos={
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("1.ad+index","data!","maybe_call_bcd_cmos"),
            R("2.ad+index","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        };

        G("R_ABX_BCD_CMOS","Read/Absolute,X",r_abs_indexed_bcd_cmos,'x');
        G("R_ABY_BCD_CMOS","Read/Absolute,Y",r_abs_indexed_bcd_cmos,'y');

        std::initializer_list<Cycle> r_zpg_indexed_bcd_cmos={
            R("pc++","adl",nullptr),
            R("adl","data!",nullptr),
            R("adl+index","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        };

        G("R_ZPX_BCD_CMOS","Read/Zero page,X",r_zpg_indexed_bcd_cmos,'x');
        G("R_ZPY_BCD_CMOS","Read/Zero page,Y",r_zpg_indexed_bcd_cmos,'y');

        G("R_INY_BCD_CMOS","Read/Indirect,Y",{
            R("pc++","ial",nullptr),
            R("ial","adl",nullptr),
            R("ial+1","adh",nullptr),
            R("1.ad+index","data!","maybe_call_bcd_cmos"),
            R("2.ad+index","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        },'y');

        G("R_INZ_BCD_CMOS","Read/Indirect Zero Page",{
            R("pc++","ial",nullptr),
            R("ial","adl",nullptr),
            R("ial+1","adh",nullptr),
            R("ad","data","call_bcd_cmos"),
            R("pc",nullptr,nullptr)
        });
    }

    // Write instructions.
    {
        G("W_ZPG","Write/Zero page",{
            R("pc++","adl","call"),
            W("adl","data",nullptr)
        });

        G("W_ABS","Write/Absolute",{
            R("pc++","adl",nullptr),
            R("pc++","adh","call"),
            W("ad","data",nullptr)
        });

        G("W_INX","Write/Indirect,X",{
            R("pc++","ial",nullptr),
            R("ial","data!",nullptr),
            R("ial+x","adl",nullptr),
            R("ial+x+1","adh","call"),
            W("ad","data",nullptr)
        });

        std::initializer_list<Cycle> w_abs_indexed={
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("1.ad+index","data!","call"),
            W("2.ad+index","data",nullptr)
        };

        G("W_ABX","Write/Absolute,X",w_abs_indexed,'x');
        G("W_ABY","Write/Absolute,Y",w_abs_indexed,'y');

        std::initializer_list<Cycle> w_zpg_indexed={
            R("pc++","adl",nullptr),
            R("adl","data","call"),
            W("adl+index","data",nullptr)
        };

        G("W_ZPX","Write/Zero page,X",w_zpg_indexed,'x');
        G("W_ZPY","Write/Zero page,Y",w_zpg_indexed,'y');

        G("W_INY","Write/Indirect,Y",{
            R("pc++","ial",nullptr),
            R("ial","adl",nullptr),
            R("ial+1","adh",nullptr),
            R("1.ad+index","data","call"),
            W("2.ad+index","data",nullptr)
        },'y');

        G("W_INZ","Write/Indirect Zero Page",{
            R("pc++","ial",nullptr),
            R("ial","adl",nullptr),
            R("ial+1","adh","call"),
            W("ad","data",nullptr)
        });
    }

    // Read-Modify-Write instructions.
    size_t rmw_idx0=gs.size();
    {
        G("RMW_ZPG","Read-modify-write/Zero page",{
            R("pc++","adl",nullptr),
            R("adl","data!",nullptr),
            W("adl","data!","call"),
            W("adl","data",nullptr)
        });

        G("RMW_ABS","Read-modify-write/Absolute",{
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("ad","data!",nullptr),
            W("ad","data!","call"),
            W("ad","data",nullptr)
        });

        G("RMW_ZPX","Read-modify-write/Zero page,X",{
            R("pc++","adl",nullptr),
            R("adl","data",nullptr),
            R("adl+index","data!",nullptr),
            W("adl+index","data!","call"),
            W("adl+index","data",nullptr)
        },'x');

        G("RMW_ABX","Read-modify-write/Absolute,X",{
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("1.ad+index","data!",nullptr),
            R("2.ad+index","data!",nullptr),
            W("2.ad+index","data!","call"),
            W("2.ad+index","data",nullptr)
        },'x');

        G("RMW_ABY","Read-modify-write/Absolute,Y",{
            R("pc++","adl",nullptr),
            R("pc++","adh",nullptr),
            R("1.ad+index","data!",nullptr),
            R("2.ad+index","data!",nullptr),
            W("2.ad+index","data!","call"),
            W("2.ad+index","data",nullptr)
        },'y');

        G("RMW_INX","Read-modify-write/Indirect,X",{
            R("pc++","ial",nullptr),//T2
            R("ial","data!",nullptr),//T3
            R("ial+x","adl",nullptr),//T4
            R("ial+x+1","adh",nullptr),//T5
            R("ad","data",nullptr),//??
            W("ad","data","call"),//??
            W("ad","data",nullptr)//T0
        },'x');

        G("RMW_INY","Read-modify-write/Indirect,Y",{
            R("pc++","ial",nullptr),//T2
            R("ial","adl",nullptr),//T3
            R("ial+1","adh",nullptr),//T4
            R("1.ad+index","data",nullptr),//T5
            R("2.ad+index","data",nullptr),//??
            W("2.ad+index","data","call"),//??
            W("2.ad+index","data",nullptr)//T0
        },'y');
    }

    // Read-Modify-Write (CMOS).
    {
        size_t n=gs.size();
        for(size_t i=rmw_idx0;i<n;++i) {
            InstrGen g_cmos=gs[i];

            g_cmos.stem+=CMOS_FUNCTION_SUFFIX;
            g_cmos.description+=" (CMOS)";
            ASSERT(g_cmos.cycles.size()>=2);
            ASSERT(!g_cmos.cycles[g_cmos.cycles.size()-2].read);
            g_cmos.cycles[g_cmos.cycles.size()-2].read=true;

            gs.push_back(g_cmos);
        }
    }

    // Push instructions.
    {
        G("Push","Push instructions",{
            R("pc","data!","call"),
            W("sp--","data",nullptr)
        });
    }

    // Pop instructions.
    {
        G("Pop","Pop instructions",{
            R("pc","data!",nullptr),
            R("sp++","data!",nullptr),
            R("sp","data","call")
        });
    }

    // Special cases.
    {
        G("JSR","JSR",{
            R("pc++","adl",nullptr),
            R("sp","data!",nullptr),
            W("sp--","pch",nullptr),
            W("sp--","pcl",nullptr),
            R("pc++","adh","jmp")
        });

        G("Reset","Interrupts",{
            R("pc","data!",nullptr),
            R("sp--","pch",nullptr),
            R("sp--","pcl",nullptr),
            R("sp--","data",nullptr),
            R("resl","pcl",nullptr),
            R("resh","pch",nullptr)
        });

        G("RTI","RTI",{
            R("pc","data!",nullptr),
            R("sp++","data",nullptr),
            R("sp++","p",nullptr),
            R("sp++","pcl",nullptr),
            R("sp","pch","rti")
        });

        G("JMP_ABS","JMP absolute",{
            R("pc++","adl",nullptr),
            R("pc++","adh","jmp")
        });

        G("JMP_IND","JMP indirect",{
            R("pc++","ial",nullptr),
            R("pc++","iah",nullptr),
            R("ia","pcl",nullptr),
            R("ia+1(nocarry)","pch",nullptr)
        });

        // always seems to take 6 cycles.
        G("JMP_IND_CMOS","JMP indirect",{
            R("pc++","ial",nullptr),
            R("pc++","iah",nullptr),
            R("ia","pcl",nullptr),
            R("ia+1","pch",nullptr),
            R("ia+1","pch",nullptr)
        });

        // always seems to take 6 cycles
        G("JMP_INDX","JMP (indirect,X)",{
            R("pc++","ial",nullptr),
            R("pc++","iah",nullptr),
            R("1.ia+x_cmos","data!",nullptr),
            R("2.ia+x_cmos","pcl",nullptr),
            R("3.ia+x_cmos","pch",nullptr)
        },'x');

        G("RTS","RTS",{
            R("pc","data!",nullptr),
            R("sp++","data",nullptr),
            R("sp++","pcl",nullptr),
            R("sp","pch",nullptr),
            R("pc++","data",nullptr)
        });

        // CMOS NOPs
        G("R_NOP11_CMOS","CMOS NOP (1 byte, 1 cycle)",{
        });

        G("R_NOP22_CMOS","CMOS NOP (2 bytes, 2 cycles)",{
            R("pc++","data",nullptr)
        });

        G("R_NOP23_CMOS","CMOS NOP (2 bytes, 3 cycles)",{
            R("pc","data",nullptr),
            R("pc++","data",nullptr)
        });

        G("R_NOP24_CMOS","CMOS NOP (2 bytes, 4 cycles)",{
            R("pc","data",nullptr),
            R("pc","data",nullptr),
            R("pc++","data",nullptr)
        });

        G("R_NOP34_CMOS","CMOS NOP (3 bytes, 4 cycles)",{
            R("pc","data",nullptr),
            R("pc++","data",nullptr),
            R("pc++","data",nullptr)
        });

        G("R_NOP38_CMOS","CMOS NOP (3 bytes, 8 cycles)",{
            R("pc","data",nullptr),
            R("pc","data",nullptr),
            R("pc","data",nullptr),
            R("pc","data",nullptr),
            R("pc","data",nullptr),
            R("pc++","data",nullptr),
            R("pc++","data",nullptr)
        });
    }

#undef R
#undef W
#undef G

    return gs;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GenerateConfig(const std::string &stem,const std::map<uint8_t,Instr> &instrs0,const std::map<uint8_t,Instr> &instrs1) {
    const Instr *instrs[256]={};

    for(auto &&it:instrs0) {
        instrs[it.first]=&it.second;
    }

    for(auto &&it:instrs1) {
        ASSERT(!instrs[it.first]);
        instrs[it.first]=&it.second;
    }

    for(size_t i=0;i<256;++i) {
        ASSERT(instrs[i]);
    }

    std::string fns_name="g_"+stem+"_fns";
    std::string di_name="g_"+stem+"_disassembly_info";
    std::string config_name="M6502_"+stem+"_config";
    //std::string get_fn_name_name="GetFnName_"+stem;

    Sep();
    Sep();
    P("\n");

    P("static const M6502Fns %s[256]={\n",fns_name.c_str());
    for(size_t i=0;i<256;++i) {
        std::string tfun,ifun;
        instrs[i]->GetTFunAndIFun(&tfun,&ifun);
        P("[0x%02zx]={&%s,",i,tfun.c_str());
        if(ifun.empty()) {
            P("NULL");
        } else {
            P("&%s",ifun.c_str());
        }
        P(",},\n");
    }
    P("};\n");
    P("\n");

    P("static const M6502DisassemblyInfo %s[256]={\n",di_name.c_str());
    for(size_t i=0;i<256;++i) {
        const Instr *instr=instrs[i];

        std::string mode="M6502AddrMode_"+ToUpper(GetModeEnumName(instr->GetDisassemblyMode()));

        std::string mnemonic=instr->GetDisassemblyMnemonic();

        P("[0x%02zx]={%zu,\"%s\",%s,%d},\n",
            i,
            mnemonic.size(),
            mnemonic.c_str(),
            mode.c_str(),
            instr->undocumented);
    }
    P("};\n");
    P("\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GenerateFnNameStuff(const std::vector<InstrGen> &gens) {
    std::set<std::string> names;

    for(const InstrGen &gen:gens) {
        for(const std::string &fn_name:gen.GetFnNames()) {
            ASSERT(names.count(fn_name)==0);
            names.insert(fn_name);
        }
    }

    P("static const Fn g_fns_by_name[]={\n");
    {
        for(auto &&it:names) {
            P("{\"%s\",&%s},\n",it.c_str(),it.c_str());
        }
    }
    P("{NULL,NULL},\n");
    P("};\n");
    P("\n");

    //P("static const char *GetGeneratedFnName(M6502Fn tfn) {\n");
    //for(auto &&it:names) {
    //    P("if(tfn==&%s) {\n",it.first.c_str());
    //    P("return g_fn_names[%zu];\n",it.second);
    //    P("}\n");
    //    P("\n");
    //}
    //P("return NULL;\n");
    //P("}\n");
    //P("\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool DoCommandLine(Options *options,int argc,char *argv[]) {
    CommandLineParser p("6502 code generator");

    bool verbose=false;

    p.AddOption('v',"verbose").SetIfPresent(&verbose).Help("be more verbose (on stderr)");
    p.AddOption('o').Arg(&options->ofname).Meta("FILE").Help("write code to FILE");
    p.AddOption("stdout").SetIfPresent(&options->to_stdout).Help("write code to stdout");
    p.AddHelpOption();

    if(!p.Parse(argc,argv)) {
        return false;
    }

    if(verbose) {
        LOG(V).Enable();
    }

    return true;
}

int main(int argc,char *argv[]) {
    Options options;
    if(!DoCommandLine(&options,argc,argv)) {
        return EXIT_FAILURE;
    }

    if(options.to_stdout) {
        LOG(CODE_STDOUT).Enable();
    }

    FILE *of=nullptr;
    if(!options.ofname.empty()) {
        of=fopen(options.ofname.c_str(),"wt");
        if(!of) {
            LOGF(ERR,"FATAL: failed to open output file: %s\n",options.ofname.c_str());
            return EXIT_FAILURE;
        }

        g_code_file_printer.SetFILE(of);
        LOG(CODE_FILE).Enable();
    }

    std::vector<InstrGen> gens=GetAll();

    P("// Automatically generated by 6502_gen.cpp\n");
    P("\n");

    for(const InstrGen &gen:gens) {
        gen.GenerateC();
    }

    GenerateFnNameStuff(gens);

    GenerateConfig("defined",GetDefinedInstructions(),GetUndefinedTrapInstructions());
    GenerateConfig("nmos6502",GetDefinedInstructions(),GetUndefinedInstructions());
    GenerateConfig("cmos6502",GetCMOSInstructions(),{});

    g_code_file_printer.SetFILE(nullptr);

    if(of) {
        fclose(of);
        of=nullptr;
    }
}
