#!/usr/bin/python
import os,os.path,sys,argparse,uuid

##########################################################################
##########################################################################
#
# 6502 generator
# --------------
#
##########################################################################
##########################################################################
#
# Cycles list
# -----------

# Each cycle can be an R (read) or W (write). 3 arguments:
#
# addr - the address to read from (put on the abus in phase 1)
#
# what - the data. For write, the value to put on the dbus in phase 1;
# for read, where to put the value copied from the dbus in phase 2.
#
# action - the action to take once the data has been read/written
# 
# addr
# ----
#
# Various addresses are available - the full list can be seet in
# gen_phase1. Some are fiddly, and there's some code that gets
# generated to support them - each has a case in the if statement in
# G.gen_phase1, that generates some code to set s->abus.w.
#
# Others map to simple C expressions - they have an entry in the
# addr_exprs dict. The generated code is like "s->abus.w=EXPR".
#
# The numbered cases are there to handle the CPU taking extra cycle(s)
# over indexed 16-bit addressing.
#
# Some addrs refer to "index", allowing them to refer to X or Y -
# supply the index register to use as one of the kwargs for the G
# constructor.
#
# what
# ----
#
# Each what maps to a member of the M6502 struct; the mapping is
# defined in the what_exprs dict.
#
# action
# ------
#
# None - do nothing
#
# call, call_bcd_cmos - call the instruction's ifn (the _bcd_cmos
# version is for CMOS ADC/SBC, which take an extra cycle in BCD mode)
#
# maybe_call, maybe_call_bcd_cmos - call the instruction's ifn, or
# not, based on whether there was a carry in the address calculations.
#
# (python callable) - call the callable, which should call the p
# function to write code to the output.

##########################################################################
##########################################################################

# Opcodes list
# ------------

# A map of int (opcode number) to an I object: mnemonic, and
# addressing mode.

##########################################################################
##########################################################################

CMOS_SUFFIX="_cMoS"

##########################################################################
##########################################################################

g_verbose=False

def v(msg):
    if not g_verbose: return
    sys.stderr.write(msg)
    sys.stderr.flush()

##########################################################################
##########################################################################

g_output=None

def p(msg):
    if g_output is None: return
    g_output.write(msg)
    g_output.flush()

##########################################################################
##########################################################################

def sep(): p("//////////////////////////////////////////////////////////////////////////\n")
def sep2():
    p("\n")
    sep()
    sep()
    p("\n")

##########################################################################
##########################################################################

# T0 is always Read("pc++","opcode","decode") - the decode action
# straight away jumps to phase 1 of the first generated cycle.

##########################################################################
##########################################################################

def gen_indexed_lsb(addr,index):
    p("    s->abus.w=s->%s.b.l+s->%s;\n"%(addr,index))
    p("    s->acarry=s->abus.b.h;\n")
    p("    s->abus.b.h=s->%s.b.h;\n"%addr)

##########################################################################
##########################################################################

def gen_indexed_msb(addr,index):
    p("    s->abus.w=s->%s.w+s->%s;\n"%(addr,index))

##########################################################################
##########################################################################

def gen_jmp(**kwargs):
    p("    s->pc=s->ad;\n")
    
def gen_rti_calllback():
    p("    if(s->rti_fn) {\n")
    p("        (*s->rti_fn)(s);\n")
    p("    }\n")

##########################################################################
##########################################################################

# def gen_jmp_ind(**kwargs):
#     p("    ++s->ia.b.l; // mustn't fix up the high byte...\n")
    
def gen_call_ifn(indent):
    p("%s(*s->ifn)(s);\n"%indent)
    p("#ifdef _DEBUG\n")
    p("%ss->ifn=NULL;\n"%indent)
    p("#endif\n")

    
##########################################################################
##########################################################################

class AddrExpr:
    def __init__(self,c_expr,c_fun):
        self.c_expr=c_expr
        self.c_fun=c_fun

addr_exprs={
    "pc":AddrExpr("s->pc.w","Fetch"),
    "pc++":AddrExpr("s->pc.w++","Fetch"),
    "adl":AddrExpr("s->ad.b.l","ZP"),
    "ad":AddrExpr("s->ad.w",""),
    "ial":AddrExpr("s->ia.b.l","ZP"),
    "ial+1":AddrExpr("(uint8_t)(s->ia.b.l+1)","ZP"),
    "sp":AddrExpr("s->s.w","Stack"),
    "ia":AddrExpr("s->ia.w",""),
    "ia+1":AddrExpr("s->ia.w+1",""),

    "ial+x":AddrExpr("(uint8_t)(s->ia.b.l+s->x)","ZP"),
    "ial+x+1":AddrExpr("(uint8_t)(s->ia.b.l+s->x+1)","ZP"),

    "nmil":AddrExpr("0xfffa",""),
    "nmih":AddrExpr("0xfffb",""),
    "resl":AddrExpr("0xfffc",""),
    "resh":AddrExpr("0xfffd",""),
    "irql":AddrExpr("0xfffe",""),
    "irqh":AddrExpr("0xffff",""),
}

what_exprs={
    "adl":"ad.b.l",
    "adh":"ad.b.h",
    "data":"data",
    "data!":"data",
    "ial":"ia.b.l",
    "iah":"ia.b.h",
    "pch":"pc.b.h",
    "pcl":"pc.b.l",
    "p":"p.value",
}

##########################################################################
##########################################################################

class G:
    def __init__(self,stem,description,cycles,**kwargs):
        self._stem=stem
        self._description=description
        self._cycles=cycles
        self._kwargs=kwargs
        # self._fn_names=set()

    def clone(self):
        return G(self._stem,
                 self._description,
                 [x.clone() for x in self._cycles],
                 **self._kwargs)

    @property
    def stem(self): return self._stem

    @property
    def cycles(self): return self._cycles

    @property
    def index(self): return self._kwargs["index"]

    def get_fn_names(self): return ["T%d_%s"%(i,self._stem) for i in range(len(self._cycles)+1)]

    # def gen_simplified(self,ifn):
    #     # cycles=[x for x in self._cycles if not (x.addr in ["1.ad+index","1.ia+index"] or
    #     #                                         x.what=="data!" or
    #     #                                         x.action=="maybe_call")]

    #     for cycle in cycles:
    #         if x.what=="data!":
    #             assert x.read
    #             # just ignore this...
    #             pass
    #         elif x.addr in ["1.ad+index","1.ia+index"]:
    #             # pointless
    #             pass
    #         else:
    #             addr_expr=addr_exprs[cycle.addr]
    #             what=cycl
    #             pass
    #             #p("
    #     p("\n")

    def gen_phase1(self,cycle,**kwargs):
        if callable(cycle.addr): cycle.addr(**kwargs)
        else:
            set_acarry=False
            if cycle.addr=="1.ad+index":
                set_acarry=True
                gen_indexed_lsb("ad",kwargs["index"])
            elif cycle.addr=="1.ia+index":
                set_acarry=True
                gen_indexed_lsb("ia",kwargs["index"])
            elif cycle.addr=="2.ad+index": gen_indexed_msb("ad",kwargs["index"])
            elif cycle.addr=="2.ia+index": gen_indexed_msb("ia",kwargs["index"])
            elif cycle.addr=="1.ia+x_cmos":
                # This is a guess.
                p("    s->abus.w=s->ia.w+s->x;\n")
            elif cycle.addr=="2.ia+x_cmos":
                # This is a guess.
                p("    s->abus.w=s->ia.w+s->x;\n")
            elif cycle.addr=="3.ia+x_cmos":
                # This is a guess.
                p("    s->abus.w=s->ia.w+s->x+1;\n")
            elif cycle.addr=="sp++":
                p("    s->abus=s->s;\n")
                p("    ++s->s.b.l;\n")
            elif cycle.addr=="sp--":
                p("    s->abus=s->s;\n")
                p("    --s->s.b.l;\n")
            elif cycle.addr=="adl+index":
                p("    s->abus.b.l=s->ad.b.l+s->%(index)s;\n"%kwargs)
                p("    s->abus.b.h=0;\n")
            elif cycle.addr=="ia+1(nocarry)":
                p("    s->abus=s->ia;\n")
                p("    ++s->abus.b.l; /* don't fix up the high byte */\n")
            else:
                addr=addr_exprs[cycle.addr]
                p("    s->abus.w=%s;\n"%addr.c_expr)

            if not cycle.read: p("    s->dbus=s->%s;\n"%what_exprs[cycle.what])

            p("    s->read=%d;\n"%cycle.read)
            # p("    SET_DBUS(%s);\n"%what)

            if cycle.action=="maybe_call":
                assert set_acarry
                p("    if(!s->acarry) {\n")
                self._gen_d1x1("        ")
                p("    }\n")
            elif cycle.action=="maybe_call_bcd_cmos":
                assert set_acarry
                p("    if(!s->p.bits.d&&!s->acarry) {\n")
                self._gen_d1x1("        ")
                p("    }\n")
            elif cycle.action=="call_bcd_cmos":
                p("    if(!s->p.bits.d) {\n")
                self._gen_d1x1("        ")
                p("    }\n")

    def gen_phase2(self,cycle,**kwargs):
        if cycle.read:
            # should really ignore bogus reads - but it looks like I
            # marked some of them up wrongly, and didn't notice,
            # because I ended up putting off the simplified emulator
            # until later :(
            #
            # Needs revisiting, but there's not too much overhead to
            # this, so it's OK for now.
            if cycle.what is None:
                p("    /* ignore dummy read */\n")
            else:
                assert cycle.what in what_exprs,(cycle)
                p("    s->%s=s->dbus;\n"%what_exprs[cycle.what])

        if cycle.action is None: pass
        elif cycle.action=="maybe_call":
            p("    if(!s->acarry) {\n")
            p("        /* No carry - done. */\n")
            gen_call_ifn("        ")
            p("\n")
            p("        /* T0 phase 1 */\n")
            p("        M6502_NextInstruction(s);\n")
            p("        return;\n")
            p("    }\n")
        elif cycle.action=="call":
            gen_call_ifn("    ")
        elif cycle.action=="maybe_call_bcd_cmos":
            p("    if(!s->p.bits.d) {\n")
            p("        if(!s->acarry) {\n")
            p("            /* No carry, no decimal - done. */\n")
            gen_call_ifn("            ")
            p("\n")
            p("            /* T0 phase 1 */\n")
            p("            M6502_NextInstruction(s);\n")
            p("            return;\n")
            p("        }\n")
            p("    }\n")
        elif cycle.action=="call_bcd_cmos":
            gen_call_ifn("    ")
            p("    if(!s->p.bits.d) {\n")
            p("        /* No decimal - done. */\n")
            p("\n")
            p("        /* T0 phase 1 */\n")
            p("        M6502_NextInstruction(s);\n")
            p("        return;\n")
            p("    }\n")
        elif callable(cycle.action):
            cycle.action(**kwargs)
        else: assert False,cycle.action

    def _gen_d1x1(self,indent):
        p("%sCheckForInterrupts(s);\n"%indent)
        
    def gen_c(self):
        sep()
        sep()
        p("//\n")
        p("// %s\n"%self._description)
        p("//\n")
        sep()
        sep()
        p("\n")

        for i in range(len(self._cycles)):
            p("static void T%d_%s(M6502 *);\n"%(i+1,self._stem))

        p("\n")
        
        i=-1
        while i<len(self._cycles):
            # if i>=0: p("static ")
            fn_name="T%d_%s"%(i+1,self._stem)
            
            p("static void %s(M6502 *s) {\n"%fn_name)
            p("    /* T%d phase 2 */\n"%(i+1))

            if i<0:
                p("    /* (decode - already done) */\n")
            else:
                self.gen_phase2(self._cycles[i],**self._kwargs)

            p("\n")

            i+=1

            p("    /* T%d phase 1 */\n"%((i+1)%(len(self._cycles)+1)))
            if i==len(self._cycles): p("    M6502_NextInstruction(s);\n")
            else:
                self.gen_phase1(self._cycles[i],**self._kwargs)
                p("    s->tfn=&T%d_%s;\n"%(i+1,self._stem))

                if i==len(self._cycles)-1: self._gen_d1x1("    ")
            p("}\n")
            p("\n")
        
##########################################################################
##########################################################################

class Cycle:
    def __init__(self,read,addr,what,action):
        self.addr=addr
        self.what=what
        self.action=action
        self.read=read

    def clone(self): return Cycle(self.read,self.addr,self.what,self.action)

    def __repr__(self):
        return "Cycle(%s,%s,%s,%s)"%(self.read,self.addr,self.what,self.action)

def R(addr,what,action): return Cycle(True,addr,what,action)
def W(addr,what,action): return  Cycle(False,addr,what,action)

def get_all():
    all=[]

    # Some instructions re-read from PC+1 into OPCODE according to the
    # docs. The simulator reads into DATA, so that OPCODE can be
    # queried.

    # When WHAT is "data!", the read/write is a dummy one and the
    # simplified simulator doesn't need to do it.

    # D1x1 is checked on the last cycle, and on a maybe_call cycle. The
    # check is made before any ifn call.

    # 1-byte instructions
    all.append(G("IMP","1-byte instructions",[R("pc","data!","call")])) # opcode
    #all.append(G("IMPMightClearI","1-byte instructions (that might clear I)",[R("pc","data!","call")],might_clear_i=True))

    ##########################################################################
    # Read instructions
    ##########################################################################
    
    r_idx0=len(all)
    all.append(G("R_IMM","Read/Immediate",[R("pc++","data","call")])) # opcode
    all.append(G("R_ZPG","Read/Zero page",[R("pc++","adl",None),
                                           R("adl","data","call")]))
    all.append(G("R_ABS","Read/Absolute",[R("pc++","adl",None),
                                          R("pc++","adh",None),
                                          R("ad","data","call")]))
    all.append(G("R_INX","Read/Indirect,X",[R("pc++","ial",None),
                                            R("ial","data!",None),
                                            R("ial+x","adl",None),
                                            R("ial+x+1","adh",None),
                                            R("ad","data","call")]))

    r_abs_indexed=[R("pc++","adl",None),
                   R("pc++","adh",None),
                   R("1.ad+index","data!","maybe_call"),
                   R("2.ad+index","data","call")]
    
    all.append(G("R_ABX","Read/Absolute,X",r_abs_indexed,index="x"))
    all.append(G("R_ABY","Read/Absolute,Y",r_abs_indexed,index="y"))

    r_zpg_indexed=[R("pc++","adl",None),
                   R("adl","data!",None),
                   R("adl+index","data","call")]
    
    all.append(G("R_ZPX","Read/Zero page,X",r_zpg_indexed,index="x"))
    all.append(G("R_ZPY","Read/Zero page,Y",r_zpg_indexed,index="y"))
    
    all.append(G("R_INY","Read/Indirect,Y",[R("pc++","ial",None),
                                            R("ial","adl",None),
                                            R("ial+1","adh",None),
                                            R("1.ad+index","data!","maybe_call"),
                                            R("2.ad+index","data","call")],
                 index="y"))

    all.append(G("R_INZ","Read/Indirect Zero Page",[R("pc++","ial",None),
                                                    R("ial","adl",None),
                                                    R("ial+1","adh",None),
                                                    R("ad","data","call")]))

    ##########################################################################
    # Read instructions with BCD for CMOS
    ##########################################################################
    
    all.append(G("R_IMM_BCD_CMOS","Read/Immediate",[R("pc++","data","call_bcd_cmos"),
                                                    R("pc",None,None)])) # opcode
    
    all.append(G("R_ZPG_BCD_CMOS","Read/Zero page",[R("pc++","adl",None),
                                                    R("adl","data","call_bcd_cmos"),
                                                    R("pc",None,None)]))
    
    all.append(G("R_ABS_BCD_CMOS","Read/Absolute",[R("pc++","adl",None),
                                                   R("pc++","adh",None),
                                                   R("ad","data","call_bcd_cmos"),
                                                   R("pc",None,None)]))
    
    all.append(G("R_INX_BCD_CMOS","Read/Indirect,X",[R("pc++","ial",None),
                                                     R("ial","data!",None),
                                                     R("ial+x","adl",None),
                                                     R("ial+x+1","adh",None),
                                                     R("ad","data","call_bcd_cmos"),
                                                     R("pc",None,None)]))

    r_abs_indexed_bcd_cmos=[R("pc++","adl",None),
                            R("pc++","adh",None),
                            R("1.ad+index","data!","maybe_call_bcd_cmos"),
                            R("2.ad+index","data","call_bcd_cmos"),
                            R("pc",None,None)]
    
    all.append(G("R_ABX_BCD_CMOS","Read/Absolute,X",r_abs_indexed_bcd_cmos,index="x"))
    all.append(G("R_ABY_BCD_CMOS","Read/Absolute,Y",r_abs_indexed_bcd_cmos,index="y"))

    r_zpg_indexed_bcd_cmos=[R("pc++","adl",None),
                            R("adl","data!",None),
                            R("adl+index","data","call_bcd_cmos"),
                            R("pc",None,None)]
    
    all.append(G("R_ZPX_BCD_CMOS","Read/Zero page,X",r_zpg_indexed_bcd_cmos,index="x"))
    all.append(G("R_ZPY_BCD_CMOS","Read/Zero page,Y",r_zpg_indexed_bcd_cmos,index="y"))
    
    all.append(G("R_INY_BCD_CMOS","Read/Indirect,Y",[R("pc++","ial",None),
                                                     R("ial","adl",None),
                                                     R("ial+1","adh",None),
                                                     R("1.ad+index","data!","maybe_call_bcd_cmos"),
                                                     R("2.ad+index","data","call_bcd_cmos"),
                                                     R("pc",None,None)],
                 index="y"))

    all.append(G("R_INZ_BCD_CMOS","Read/Indirect Zero Page",[R("pc++","ial",None),
                                                             R("ial","adl",None),
                                                             R("ial+1","adh",None),
                                                             R("ad","data","call_bcd_cmos"),
                                                             R("pc",None,None)]))

    ##########################################################################
    # Write instructions
    ##########################################################################
    
    all.append(G("W_ZPG","Write/Zero page",[R("pc++","adl","call"),
                                            W("adl","data",None)]))

    all.append(G("W_ABS","Write/Absolute",[R("pc++","adl",None),
                                           R("pc++","adh","call"),
                                           W("ad","data",None)]))

    all.append(G("W_INX","Write/Indirect,X",[R("pc++","ial",None),
                                             R("ial","data!",None),
                                             R("ial+x","adl",None),
                                             R("ial+x+1","adh","call"),
                                             W("ad","data",None)]))

    w_abs_indexed=[R("pc++","adl",None),
                   R("pc++","adh",None),
                   R("1.ad+index","data!","call"),
                   W("2.ad+index","data",None)]

    all.append(G("W_ABX","Write/Absolute,X",w_abs_indexed,index="x"))
    all.append(G("W_ABY","Write/Absolute,Y",w_abs_indexed,index="y"))

    w_zpg_indexed=[R("pc++","adl",None),
                   R("adl","data","call"),
                   W("adl+index","data",None)]

    all.append(G("W_ZPX","Write/Zero page,X",w_zpg_indexed,index="x"))
    all.append(G("W_ZPY","Write/Zero page,Y",w_zpg_indexed,index="y"))

    all.append(G("W_INY","Write/Indirect,Y",[R("pc++","ial",None),
                                             R("ial","adl",None),
                                             R("ial+1","adh",None),
                                             R("1.ad+index","data","call"),
                                             W("2.ad+index","data",None)],
                 index="y"))

    all.append(G("W_INZ","Write/Indirect Zero Page",[R("pc++","ial",None),
                                                     R("ial","adl",None),
                                                     R("ial+1","adh","call"),
                                                     W("ad","data",None)]))

    ##########################################################################
    # Read-modify-write instructions
    ##########################################################################
    
    rmw_idx0=len(all)
    
    all.append(G("RMW_ZPG","Read-modify-write/Zero page",[R("pc++","adl",None),
                                                          R("adl","data!",None),
                                                          W("adl","data!","call"),
                                                          W("adl","data",None)]))

    all.append(G("RMW_ABS","Read-modify-write/Absolute",[R("pc++","adl",None),
                                                         R("pc++","adh",None),
                                                         R("ad","data!",None),
                                                         W("ad","data!","call"),
                                                         W("ad","data",None)]))

    all.append(G("RMW_ZPX","Read-modify-write/Zero page,X",[R("pc++","adl",None),
                                                            R("adl","data",None),
                                                            R("adl+index","data!",None),
                                                            W("adl+index","data!","call"),
                                                            W("adl+index","data",None)],
                 index="x"))

    all.append(G("RMW_ABX","Read-modify-write/Absolute,X",[R("pc++","adl",None),
                                                           R("pc++","adh",None),
                                                           R("1.ad+index","data!",None),
                                                           R("2.ad+index","data!",None),
                                                           W("2.ad+index","data!","call"),
                                                           W("2.ad+index","data",None)],
                 index="x"))

    all.append(G("RMW_ABY","Read-modify-write/Absolute,Y",[R("pc++","adl",None),
                                                           R("pc++","adh",None),
                                                           R("1.ad+index","data!",None),
                                                           R("2.ad+index","data!",None),
                                                           W("2.ad+index","data!","call"),
                                                           W("2.ad+index","data",None)],
                 index="y"))
    
    all.append(G("RMW_INX",
                 "Read-modify-write/Indirect,X",[R("pc++","ial",None),#T2
                                                 R("ial","data!",None),#T3
                                                 R("ial+x","adl",None),#T4
                                                 R("ial+x+1","adh",None),#T5
                                                 R("ad","data",None),#??
                                                 W("ad","data","call"),#??
                                                 W("ad","data",None)],#T0
                 index="x"))

    all.append(G("RMW_INY",
                 "Read-modify-write/Indirect,Y",[R("pc++","ial",None),#T2
                                                 R("ial","adl",None),#T3
                                                 R("ial+1","adh",None),#T4
                                                 R("1.ad+index","data",None),#T5
                                                 R("2.ad+index","data",None),#??
                                                 W("2.ad+index","data","call"),#??
                                                 W("2.ad+index","data",None)],#T0
                 index="y"))

    ##########################################################################
    # RMW - CMOS versions.
    ##########################################################################
    
    for rmw_idx in range(rmw_idx0,len(all)):
        rmw_cmos=all[rmw_idx].clone()

        rmw_cmos._stem+=CMOS_SUFFIX.upper()
        rmw_cmos._description+=" (CMOS)"
        assert not rmw_cmos.cycles[-2].read
        rmw_cmos.cycles[-2].read=True

        assert not all[rmw_idx].cycles[-2].read#check I didn't mess up the clone...

        all.append(rmw_cmos)


    # In the end I hand-wrote this case.
    
    # all.append(G("RMW_ABX2_CMOS",
    #              "Read-modify-write shift/Absolute,X (CMOS)",
    #              [R("pc++","adl",None),
    #               R("pc++","adh",None),
    #               R("1.ad+index","data!",None),
    #               R("2.ad+index","data!","call"),
    #               R("2.ad+index","data","call"),
    #               W("2.ad+index","data",None)],
    #              index="x"))
    
    ##########################################################################
    # Push instructions
    ##########################################################################
    
    all.append(G("Push","Push instructions",[R("pc","data!","call"),
                                             W("sp--","data",None)]))

    ##########################################################################
    # Pop instructions
    ##########################################################################
    
    all.append(G("Pop","Pop instructions",
                 [R("pc","data!",None),
                  R("sp++","data!",None),
                  R("sp","data","call")]))

    all.append(G("JSR","JSR",[R("pc++","adl",None),
                              R("sp","data!",None),
                              W("sp--","pch",None),
                              W("sp--","pcl",None),
                              R("pc++","adh",gen_jmp)]))

    all.append(G("Reset","Interrupts",[R("pc","data!",None),
                                       R("sp--","pch",None),
                                       R("sp--","pcl",None),
                                       R("sp--","data",None),
                                       R("resl","pcl",None),
                                       R("resh","pch",None)]))

    ##########################################################################
    # Special cases
    ##########################################################################

    # # On a reset, the reads clobber PC and DATA, but no problem,
    # # because it's a reset...
    # all.append(G("Interrupt","Interrupts",[R("pc","data",None),
    #                                        IW("sp--","pch",None),
    #                                        IW("sp--","pcl",gen_prepare_irq_p),
    #                                        IW("sp--","data",None),
    #                                        R("ia","adl",None),
    #                                        R("ia+1","adh",gen_irq)]))

    all.append(G("RTI","RTI",[R("pc","data!",None),
                              R("sp++","data",None),
                              R("sp++","p",None),
                              R("sp++","pcl",None),
                              R("sp","pch",gen_rti_calllback)]))

    all.append(G("JMP_ABS","JMP absolute",[R("pc++","adl",None),
                                           R("pc++","adh",gen_jmp)]))

    all.append(G("JMP_IND","JMP indirect",[R("pc++","ial",None),
                                           R("pc++","iah",None),
                                           R("ia","pcl",None),
                                           R("ia+1(nocarry)","pch",None)]))

    # always seems to take 6 cycles.
    all.append(G("JMP_IND_CMOS","JMP indirect",[R("pc++","ial",None),
                                                R("pc++","iah",None),
                                                R("ia","pcl",None),
                                                R("ia+1","pch",None),
                                                R("ia+1","pch",None)]))

    # always seems to take 6 cycles
    all.append(G("JMP_INDX","JMP (indirect,X)",[R("pc++","ial",None),
                                                R("pc++","iah",None),
                                                R("1.ia+x_cmos","data!",None),
                                                R("2.ia+x_cmos","pcl",None),
                                                R("3.ia+x_cmos","pch",None)],
                 index="x"))

    all.append(G("RTS","RTS",[R("pc","data!",None),
                              R("sp++","data",None),
                              R("sp++","pcl",None),
                              R("sp","pch",None),
                              R("pc++","data",None)]))

    # CMOS NOPs
    all.append(G("R_NOP11_CMOS","CMOS NOP (1 byte, 1 cycle)",[]))
    all.append(G("R_NOP22_CMOS","CMOS NOP (2 bytes, 2 cycles)",[R("pc++","data",None)]))
    all.append(G("R_NOP23_CMOS","CMOS NOP (2 bytes, 3 cycles)",[R("pc","data",None),
                                                                R("pc++","data",None)]))
    all.append(G("R_NOP24_CMOS","CMOS NOP (2 bytes, 4 cycles)",[R("pc","data",None),
                                                                R("pc","data",None),
                                                                R("pc++","data",None)]))
    all.append(G("R_NOP34_CMOS","CMOS NOP (3 bytes, 4 cycles)",[R("pc","data",None),
                                                                R("pc++","data",None),
                                                                R("pc++","data",None)]))
    all.append(G("R_NOP38_CMOS","CMOS NOP (3 bytes, 8 cycles)",[R("pc","data",None),
                                                                R("pc","data",None),
                                                                R("pc","data",None),
                                                                R("pc","data",None),
                                                                R("pc","data",None),
                                                                R("pc++","data",None),
                                                                R("pc++","data",None)]))

    return all

##########################################################################
##########################################################################

def output_to(fname):
    global g_output
    global g_close_output
    
    if fname is None or g_output is not None:
        if g_output is not None:
            if g_close_output: g_output.close()
            g_output=None

    if fname is not None:
        if fname=="-":
            g_output=sys.stdout
            g_close_output=False
        else:
            g_output=open(fname,"wt")
            g_close_output=True

##########################################################################
##########################################################################

# (for future use - probably?)

# Modification codes:
#
# a,x,y,s,p = acc/x/y/stack/status
# P = pc
# d = W data/RMW data/acc if accumulator mode

# mnemonic_codes={ "adc":"ap", "ahx":"d", "alr":"ap", "anc":"ap",
#                  "and":"ap", "arr":"ap", "asl":"dp", "axs":"xp", "bit":"p",
#                  "brk":"", "clc":"p", "cld":"p", "cli":"p", "clv":"p", "cmp":"p",
#                  "cpx":"p", "cpy":"p", "dcp":"dap", "dec":"dp", "dex":"px",
#                  "dey":"py", "eor":"pa", "hlt":"", "inc":"dp", "inx":"px",
#                  "iny":"py", "isc":"dap", "jmp":"P", "jsr":"Ps", "las":"axp",
#                  "lax":"axp", "lda":"pa", "ldx":"px", "ldy":"py", "lsr":"dp!",
#                  "lxa":"axp", "nop":"", "ora":"pa", "pha":"s", "php":"s",
#                  "pla":"pas", "plp":"ps", "rla":"dap", "rol":"dp", "ror":"dp",
#                  "rra":"dap", "rti":"ps", "rts":"Ps", "sax":"d", "sbc":"pa",
#                  "sec":"p", "sed":"p", "sei":"p", "shx":"d", "shy":"d",
#                  "slo":"dap", "sre":"dap", "sta":"d", "stx":"d", "sty":"d",
#                  "tas":"sd", "tax":"px", "tay":"py", "tsx":"px", "txa":"pa",
#                  "txs":"s", "tya":"py", "xaa":"ap", }

types={
    "R":[
        "adc","and","bit","cmp","cpx","cpy","eor","lda","ldx","ldy","ora","sbc",
        "adc_cmos","sbc_cmos","bit_cmos",
        # it makes the illegal versions easier to handle if NOP is a
        # read instruction. Though this does mean the documented one
        # is a special case, which is a bit weird.
        "nop",
        "lax","alr","arr","xaa","lxa","axs","anc","las",
    ],
    "W":[
        "sta","stx","sty",
        "sax","ahx","shy","shx","tas",
        "stz",                  # cmos
    ],
    "Branch":[
        "bcc","bcs","beq","bmi","bne","bpl","bvc","bvs",
        "bra",                  # cmos
    ],
    "IMP":[
        "clc","cld","clv","dex","dey","inx","iny","sec","sed","sei","tax","tay","tsx","txa","txs","tya",
        "cli",
        "ill",               # illegal
    ],
    "Push":[
        "pha","php",
        "phx","phy",            # cmos
    ],
    "Pop":[
        "pla","plp",
        "plx","ply",            # cmos
    ],
    "RMW":[
        "asl","dec","inc","lsr","rol","ror",
        "slo","rla","sre","rra","dcp","isc",
        "trb","tsb",            # cmos
    ],
}

def get_instr_type(instr):
    for k,v in types.iteritems():
        if instr in v: return k
    return None

class I:
    def __init__(self,mnemonic,mode):
        self.mnemonic=mnemonic
        self.mode=mode
        self.undocumented=False

    def __repr__(self):
        return "I(%s,%s)"%(repr(self.mnemonic),repr(self.mode))

    def get_mnemonic(self):
        m=self.mnemonic
        if m.lower().endswith(CMOS_SUFFIX.lower()): m=m[:-len(CMOS_SUFFIX)]
        return m

defined_opcodes={
    0x00:I("brk","imp"),0x01:I("ora","inx"),0x05:I("ora","zpg"),0x06:I("asl","zpg"),
    0x08:I("php","imp"),0x09:I("ora","imm"),0x0a:I("asl","acc"),0x0d:I("ora","abs"),
    0x0e:I("asl","abs"),0x10:I("bpl","rel"),0x11:I("ora","iny"),0x15:I("ora","zpx"),
    0x16:I("asl","zpx"),0x18:I("clc","imp"),0x19:I("ora","aby"),0x1d:I("ora","abx"),
    0x1e:I("asl","abx"),0x20:I("jsr","abs"),0x21:I("and","inx"),0x24:I("bit","zpg"),
    0x25:I("and","zpg"),0x26:I("rol","zpg"),0x28:I("plp","imp"),0x29:I("and","imm"),
    0x2a:I("rol","acc"),0x2c:I("bit","abs"),0x2d:I("and","abs"),0x2e:I("rol","abs"),
    0x30:I("bmi","rel"),0x31:I("and","iny"),0x35:I("and","zpx"),0x36:I("rol","zpx"),
    0x38:I("sec","imp"),0x39:I("and","aby"),0x3d:I("and","abx"),0x3e:I("rol","abx"),
    0x40:I("rti","imp"),0x41:I("eor","inx"),0x45:I("eor","zpg"),0x46:I("lsr","zpg"),
    0x48:I("pha","imp"),0x49:I("eor","imm"),0x4a:I("lsr","acc"),0x4c:I("jmp","abs"),
    0x4d:I("eor","abs"),0x4e:I("lsr","abs"),0x50:I("bvc","rel"),0x51:I("eor","iny"),
    0x55:I("eor","zpx"),0x56:I("lsr","zpx"),0x58:I("cli","imp"),0x59:I("eor","aby"),
    0x5d:I("eor","abx"),0x5e:I("lsr","abx"),0x60:I("rts","imp"),0x61:I("adc","inx"),
    0x65:I("adc","zpg"),0x66:I("ror","zpg"),0x68:I("pla","imp"),0x69:I("adc","imm"),
    0x6a:I("ror","acc"),0x6c:I("jmp","ind"),0x6d:I("adc","abs"),0x6e:I("ror","abs"),
    0x70:I("bvs","rel"),0x71:I("adc","iny"),0x75:I("adc","zpx"),0x76:I("ror","zpx"),
    0x78:I("sei","imp"),0x79:I("adc","aby"),0x7d:I("adc","abx"),0x7e:I("ror","abx"),
    0x81:I("sta","inx"),0x84:I("sty","zpg"),0x85:I("sta","zpg"),0x86:I("stx","zpg"),
    0x88:I("dey","imp"),0x8a:I("txa","imp"),0x8c:I("sty","abs"),0x8d:I("sta","abs"),
    0x8e:I("stx","abs"),0x90:I("bcc","rel"),0x91:I("sta","iny"),0x94:I("sty","zpx"),
    0x95:I("sta","zpx"),0x96:I("stx","zpy"),0x98:I("tya","imp"),0x99:I("sta","aby"),
    0x9a:I("txs","imp"),0x9d:I("sta","abx"),0xa0:I("ldy","imm"),0xa1:I("lda","inx"),
    0xa2:I("ldx","imm"),0xa4:I("ldy","zpg"),0xa5:I("lda","zpg"),0xa6:I("ldx","zpg"),
    0xa8:I("tay","imp"),0xa9:I("lda","imm"),0xaa:I("tax","imp"),0xac:I("ldy","abs"),
    0xad:I("lda","abs"),0xae:I("ldx","abs"),0xb0:I("bcs","rel"),0xb1:I("lda","iny"),
    0xb4:I("ldy","zpx"),0xb5:I("lda","zpx"),0xb6:I("ldx","zpy"),0xb8:I("clv","imp"),
    0xb9:I("lda","aby"),0xba:I("tsx","imp"),0xbc:I("ldy","abx"),0xbd:I("lda","abx"),
    0xbe:I("ldx","aby"),0xc0:I("cpy","imm"),0xc1:I("cmp","inx"),0xc4:I("cpy","zpg"),
    0xc5:I("cmp","zpg"),0xc6:I("dec","zpg"),0xc8:I("iny","imp"),0xc9:I("cmp","imm"),
    0xca:I("dex","imp"),0xcc:I("cpy","abs"),0xcd:I("cmp","abs"),0xce:I("dec","abs"),
    0xd0:I("bne","rel"),0xd1:I("cmp","iny"),0xd5:I("cmp","zpx"),0xd6:I("dec","zpx"),
    0xd8:I("cld","imp"),0xd9:I("cmp","aby"),0xdd:I("cmp","abx"),0xde:I("dec","abx"),
    0xe0:I("cpx","imm"),0xe1:I("sbc","inx"),0xe4:I("cpx","zpg"),0xe5:I("sbc","zpg"),
    0xe6:I("inc","zpg"),0xe8:I("inx","imp"),0xe9:I("sbc","imm"),0xea:I("nop","imp"),
    0xec:I("cpx","abs"),0xed:I("sbc","abs"),0xee:I("inc","abs"),0xf0:I("beq","rel"),
    0xf1:I("sbc","iny"),0xf5:I("sbc","zpx"),0xf6:I("inc","zpx"),0xf8:I("sed","imp"),
    0xf9:I("sbc","aby"),0xfd:I("sbc","abx"),0xfe:I("inc","abx"),
}

undefined_opcodes={
    0x0c:I("nop","abs"),
    0x07:I("slo","zpg"),0x17:I("slo","zpx"),0x03:I("slo","inx"),0x13:I("slo","iny"),0x0f:I("slo","abs"),0x1f:I("slo","abx"),0x1b:I("slo","aby"),
    0x27:I("rla","zpg"),0x37:I("rla","zpx"),0x23:I("rla","inx"),0x33:I("rla","iny"),0x2f:I("rla","abs"),0x3f:I("rla","abx"),0x3b:I("rla","aby"),
    0x47:I("sre","zpg"),0x57:I("sre","zpx"),0x43:I("sre","inx"),0x53:I("sre","iny"),0x4f:I("sre","abs"),0x5f:I("sre","abx"),0x5b:I("sre","aby"),
    0x67:I("rra","zpg"),0x77:I("rra","zpx"),0x63:I("rra","inx"),0x73:I("rra","iny"),0x6f:I("rra","abs"),0x7f:I("rra","abx"),0x7b:I("rra","aby"),
    0xc7:I("dcp","zpg"),0xd7:I("dcp","zpx"),0xc3:I("dcp","inx"),0xd3:I("dcp","iny"),0xcf:I("dcp","abs"),0xdf:I("dcp","abx"),0xdb:I("dcp","aby"),
    0xe7:I("isc","zpg"),0xf7:I("isc","zpx"),0xe3:I("isc","inx"),0xf3:I("isc","iny"),0xef:I("isc","abs"),0xff:I("isc","abx"),0xfb:I("isc","aby"),
    0x87:I("sax","zpg"),0x97:I("sax","zpy"),0x83:I("sax","inx"),                    0x8f:I("sax","abs"),
    0xa7:I("lax","zpg"),0xb7:I("lax","zpy"),0xa3:I("lax","inx"),0xb3:I("lax","iny"),0xaf:I("lax","abs"),                   0xbf:I("lax","aby"),
    0x4b:I("alr","imm"),
    0x6b:I("arr","imm"),
    0x8b:I("xaa","imm"),
    0xab:I("lxa","imm"),
    0xcb:I("axs","imm"),
    0x93:I("ahx","iny"),0x9f:I("ahx","aby"),
    0x9c:I("shy","abx"),
    0x9e:I("shx","aby"),
    0x9b:I("tas","aby"),
    0x0b:I("anc","imm"),0x2b:I("anc","imm"),
    0xbb:I("las","aby"),
    0xeb:I("sbc","imm"),
}

def add_cmos_nops(nops,code,*opcodes):
    for opcode in opcodes:
        assert opcode not in nops
        nops[opcode]=I("nop","nop"+code+CMOS_SUFFIX.lower())

def make_cmos_opcodes():
    cmos_opcodes={}
    for k,v in defined_opcodes.iteritems(): cmos_opcodes[k]=v

    # Spot fixes.
    cmos_opcodes[0]=I("brk_cmos","imp")
    cmos_opcodes[0x6c]=I("jmp","ind_cmos")

    new_opcodes={
        0xda:I("phx","imp"),0x5a:I("phy","imp"),0xfa:I("plx","imp"),0x7a:I("ply","imp"),
        0x64:I("stz","zpg"),0x74:I("stz","zpx"),0x9c:I("stz","abs"),0x9e:I("stz","abx"),
        0x14:I("trb","zpg"),0x1c:I("trb","abs"),
        0x04:I("tsb","zpg"),0x0c:I("tsb","abs"),
        0x72:I("adc","inz"),
        0x32:I("and","inz"),
        0xD2:I("cmp","inz"),
        0x52:I("eor","inz"),
        0xB2:I("lda","inz"),
        0x12:I("ora","inz"),
        0xF2:I("sbc","inz"),
        0x92:I("sta","inz"),
        0x89:I("bit_cmos","imm"),0x34:I("bit","zpx"),0x3c:I("bit","abx"),
        0x3a:I("dec","acc"),
        0x1a:I("inc","acc"),
        0x7c:I("jmp","indx"),
        0x80:I("bra","rel"),
    }

    for k,v in new_opcodes.iteritems():
        assert k not in cmos_opcodes
        cmos_opcodes[k]=v

    for lsn in [0x03,0x07,0x0b,0x0f]:
        for msn in range(16):
            add_cmos_nops(cmos_opcodes,"11",msn*16+lsn)
        
    add_cmos_nops(cmos_opcodes,"22",0x02,0x22,0x42,0x62,0x82,0xc2,0xe2)
    add_cmos_nops(cmos_opcodes,"23",0x44)
    add_cmos_nops(cmos_opcodes,"24",0x54,0xd4,0xf4)
    add_cmos_nops(cmos_opcodes,"38",0x5c)
    add_cmos_nops(cmos_opcodes,"34",0xdc,0xfc)

    for i in range(256): assert i in cmos_opcodes,hex(i)

    # More hacks.
    for k,v in cmos_opcodes.iteritems():
        if v.mnemonic in ["adc","sbc"]:
            # Extra cycle in BCD mode
            mode=v.mode
            if mode.upper().endswith(CMOS_SUFFIX.upper()): mode=mode[:-len(CMOS_SUFFIX)]
            cmos_opcodes[k]=I(v.mnemonic+CMOS_SUFFIX.lower(),mode+"_bcd_cmos")
        elif v.mnemonic in ["asl","lsr","rol","ror"] and v.mode=="abx":
            # 1 cycle short when no page boundary crossed
            cmos_opcodes[k]=I(v.mnemonic,v.mode+"2_cmos")
        elif get_instr_type(v.mnemonic)=="RMW" and v.mode!="acc":
            # Last 2 cycles are RW rather than WW
            cmos_opcodes[k]=I(v.mnemonic,v.mode+CMOS_SUFFIX.lower())

    return cmos_opcodes

cmos_opcodes=make_cmos_opcodes()

def add_repeated_opcode(map,mnemonic,mode,*opcodes):
    for opcode in opcodes:
        assert opcode not in map
        map[opcode]=I(mnemonic,mode)

add_repeated_opcode(undefined_opcodes,"nop","imp",0x1a,0x3a,0x5a,0x7a,0xda,0xfa)
add_repeated_opcode(undefined_opcodes,"nop","imm",0x80,0x82,0x89,0xc2,0xe2)
add_repeated_opcode(undefined_opcodes,"nop","zpg",0x04,0x44,0x64)
add_repeated_opcode(undefined_opcodes,"nop","zpy",0x14,0x34,0x54,0x74,0xd4,0xf4)
add_repeated_opcode(undefined_opcodes,"nop","abx",0x1c,0x3c,0x5c,0x7c,0xdc,0xfc)
add_repeated_opcode(undefined_opcodes,"hlt","imp",0x02,0x12,0x22,0x32,0x42,0x52,0x62,0x72,0x92,0xB2,0xD2,0xF2)

for i in range(256):
    assert i in defined_opcodes or i in undefined_opcodes
    assert (i in defined_opcodes)!=(i in undefined_opcodes),hex(i)
    if i in undefined_opcodes:
        undefined_opcodes[i].undocumented=True

##########################################################################
##########################################################################
        
def get_tfun_and_ifun(instr):
    if instr.mnemonic=="brk":
        tfun="T0_Interrupt"
        ifun=None
    elif instr.mnemonic=="brk_cmos":
        tfun="T0_BRK_CMOS"
        ifun=None
    else:
        # oops... some stupid naming conventions here :(
        type=get_instr_type(instr.mnemonic)
        if type is not None:
            ifun=instr.mnemonic.upper()

            if instr.mnemonic=="nop" and instr.mode=="imp": tfun="T0_IMP"
            elif instr.mode in ["imp","rel"]: tfun="T0_%s"%type
            elif instr.mode=="acc":
                tfun="T0_IMP"
                ifun+="A"
            else: tfun="T0_%s_%s"%(type,instr.mode.upper())
        else:
            if instr.mnemonic in ["rts","rti","jsr","brk"]: type=instr.mnemonic.upper()
            elif instr.mnemonic=="jmp":
                # all the JMPs are special-cased.
                type=instr.mnemonic.upper()+"_"+instr.mode.upper()
            elif instr.mnemonic=="hlt": type="HLT"
            else: assert False,(instr)

            tfun="T0_%s"%type
            ifun=None

    return tfun,ifun

def get_all_ops(*opcodes_maps):
    ops=256*[None]

    for opcodes_map in opcodes_maps:
        for i in range(256):
            if i in opcodes_map:
                assert ops[i] is None,i
                ops[i]=opcodes_map[i]

    return ops

def gen_config(stem,*opcodes_maps):
    ops=get_all_ops(*opcodes_maps)
    
    for i in range(256):
        if ops[i] is None:
            ops[i]=I("ill","imp")
            ops[i].undocumented=True
    
    # ops=[opcodes_map.get(i,) for i in range(256)]

    fns_name="g_%s_fns"%stem
    di_name="g_%s_disassembly_info"%stem
    config_name="M6502_%s_config"%stem
    get_fn_name_name="GetFnName_%s"%stem

    sep()
    sep()
    p("\n")
    
    p("static const M6502Fns %s[256]={\n"%fns_name)

    for i in range(256):
        tfun,ifun=get_tfun_and_ifun(ops[i])
        p("    [0x%02x]={&%s,%s,},\n"%(i,
                                       tfun,
                                       "&%s"%ifun if ifun is not None else "NULL"))
    
    p("};\n")
    p("\n")

    p("static const M6502DisassemblyInfo %s[256]={\n"%di_name)

    for i in range(256):
        mnemonic=ops[i].get_mnemonic()
        p("    [0x%02x]={%d,\"%s\",M6502AddrMode_%s,%d},\n"%(i,
                                                             len(mnemonic),
                                                             mnemonic,
                                                             ops[i].mode.upper(),
                                                             ops[i].undocumented))
    
    p("};\n")
    p("\n")

##########################################################################
##########################################################################

def gen_stack_simplified(pr,cycle,oper):
    if cycle.read: p(pr+"%s=SimplifiedReadStack(s,s->s.w);\n"%cycle.what)
    else: p(pr+"SimplifiedWriteStack(s,s->s.w,%s);\n"%cycle.what)
    p(pr+"%ss->s.b.l;\n"%oper)

def gen_indexed_simplified(pr,cycle,reg,index):
    addr_expr="(M6502Word){.w=s->%s+s->%s}"%(reg,index)
    if cycle.read: p(pr+"%s=SimplifiedRead(s,%s);\n"%(cycle.what,addr_expr))
    else: p(pr+"SimplifiedWrite(s,%s,%s);\n"%(addr_expr,cycle.what))
    
def gen_simplified(stem,all,*opcodes_maps):
    p("void CONCAT2(M6502SingleStep%s_,M6502_SUFFIX)(M6502 *s) {\n"%stem)
    p("    s->opcode=SimplifiedReadFetch(s,s->pc.w);\n")
    p("    ++s->pc.w;\n")
    p("\n")
    p("    switch(s->opcode) {\n")

    ops=get_all_ops(*opcodes_maps)

    for i in range(256):
        if ops[i] is None: continue

        tfun,ifun=get_tfun_and_ifun(ops[i])
        
        assert tfun.startswith("T0_")
        stem=tfun[3:]

        # find the gen object for this
        gen=None
        for g in all:
            if g.stem==stem:
                gen=g
                break

        p("    case 0x%02x:\n"%i)
        p("        {\n")

        pr="            "
        
        if stem=="Branch":
            assert gen is None
            p(pr+"s->data=SimplifiedReadFetch(s,s->pc.w);\n")
            p(pr+"++s->pc.w;\n")
            p(pr+"%s(s);\n"%ifun)
            p(pr+"if(s->data) {\n")
            p(pr+"    s->pc.w+=(int8_t)s->data;\n")
            p(pr+"}\n")
        elif stem=="HLT":
            assert gen is None
            p(pr+"--s->pc.w;\n")
        elif stem=="Interrupt":
            # special case.
            pass
        else:
            assert gen is not None,(stem,tfun,ifun)

            for cycle in gen.cycles:
                if cycle.what=="data!":
                    # skip this.
                    pass
                elif cycle.addr in ["1.ad+index","1.ia+index","1.ia+x_cmos"]:
                    # skip this.
                    pass
                elif cycle.what is not None:
                    what=what_exprs[cycle.what]
                    if cycle.addr=="2.ad+index": gen_indexed_simplified(pr,cycle,"ad",gen.index)
                    elif cycle.addr=="2.ia+index": gen_indexed_simplified(pr,cycle,"ia",gen.index)
                    elif cycle.addr=="2.ia+x_cmos": gen_indexed_simplified(pr,cycle,"ia","x")
                    elif cycle.addr=="3.ia+x_cmos": gen_indexed_simplified(pr,cycle,"ia","x+1")
                    elif cycle.addr=="sp++": gen_stack_simplified(pr,cycle,"++")
                    elif cycle.addr=="sp--": gen_stack_simplified(pr,cycle,"--")
                    elif cycle.addr=="ia+1(nocarry)":
                        assert cycle.read
                        p(pr+"%s=SimplifiedRead(s,(M6502Word){.b.h=s->ia.b.h,.b.l=s->ia.b.l+1});\n"%what)
                    elif cycle.addr=="adl+index":
                        if cycle.read:
                            p(pr+"%s=SimplifiedReadZP(s,s->ad.b.l+s->%s);\n"%(what,gen.index))
                        else:
                            p(pr+"SimplifiedWriteZP(s,s->ad.b.l+s->%s,%s);\n"%(gen.index,what))
                    else:
                        addr_expr=addr_exprs[cycle.addr]
                        if cycle.read:
                            p(pr+"%s=SimplifiedRead%s(s,%s);\n"%(what_exprs[cycle.what],
                                                                 addr_expr.c_fun,
                                                                 addr_expr.c_expr))
                        else:
                            p(pr+"SimplifiedWrite%s(s,%s,%s);\n"%(addr_expr.c_fun,
                                                                  addr_expr.c_expr,
                                                                  what_exprs[cycle.what]))

                if cycle.action=="call": p(pr+"%s(s);\n"%ifun)

        p(pr+"//tfun=%s ifun=%s\n"%(tfun,ifun))
        p("        }\n")
        p("        break;\n")
        p("\n")

    p("    }\n")
    p("}\n")
    p("\n")


##########################################################################
##########################################################################

def main(options):
    global g_verbose ; g_verbose=options.verbose

    all=get_all()

    output_to(options.ofname)

    p("// Automatically generated by gen6502.py\n")
    p("\n")

    for g in all: g.gen_c()

    p("static const char *GetGeneratedFnName(M6502Fn tfn) {\n")
    for g in all:
        for fn_name in g.get_fn_names():
            p("    if(tfn==&%s) {\n"%fn_name)
            p("        return \"%s\";\n"%fn_name)
            p("    }\n")
            p("\n")
    p("    return NULL;\n")
    p("}\n")
    p("\n")

    gen_config("defined",defined_opcodes)
    gen_config("nmos6502",defined_opcodes,undefined_opcodes)
    gen_config("cmos6502",cmos_opcodes)

    output_to(options.sfname)

    gen_simplified("Defined",all,defined_opcodes)
    gen_simplified("NMOS6502",all,defined_opcodes,undefined_opcodes)
    #gen_simplified("CMOS65C02",all,cmos_opcodes)

    output_to(None)

##########################################################################
##########################################################################

if __name__=="__main__":
    parser=argparse.ArgumentParser()

    parser.add_argument("-v","--verbose",action="store_true",help="be more verbose (on stderr)")
    parser.add_argument("-o",dest="ofname",metavar="FILE",help="write code to %(metavar)s (- for stdout)")
    parser.add_argument("-s",dest="sfname",metavar="FILE",help="write simplified code to %(metavar)s (- for stdout)")

    main(parser.parse_args(sys.argv[1:]))
