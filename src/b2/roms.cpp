#include <shared/system.h>
#include <shared/debug.h>
#include "roms.h"
#include "load_save.h"

#include <shared/enum_def.h>
#include "roms.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string BeebROM::GetAssetPath() const {
    return ::GetAssetPath("roms",this->path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebROM BEEB_ROM_OS12={"OS12.ROM","OS 1.20",StandardROM_OS12};
const BeebROM BEEB_ROM_BPLUS_MOS={"B+MOS.rom","B+ MOS",StandardROM_BPlusMOS};

const BeebROM BEEB_ROM_BASIC2={"BASIC2.ROM","BASIC II",StandardROM_BASIC2};
const BeebROM BEEB_ROM_ACORN_DFS={"DFS-2.26.rom","Acorn 1770 DFS",StandardROM_Acorn1770DFS};
const BeebROM BEEB_ROM_WATFORD_DDFS_DDB2={"DDFS-1.53.rom","Watford DDFS (DDB2)",StandardROM_WatfordDDFS_DDB2};
const BeebROM BEEB_ROM_WATFORD_DDFS_DDB3={"DDFS-1.54T.rom","Watford DDFS (DDB3)",StandardROM_WatfordDDFS_DDB3};
const BeebROM BEEB_ROM_OPUS_DDOS={"OPUS-DDOS-3.45.rom","Opus DDOS",StandardROM_OpusDDOS};
const BeebROM BEEB_ROM_OPUS_CHALLENGER={"challenger-1.01.rom","Opus Challenger",StandardROM_OpusChallenger};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebROM BEEB_ROMS_MOS320[8]={
    {"m128/3.20/dfs.rom","MOS 3.20 DFS (9)",StandardROM_MOS320_DFS},
    {"m128/3.20/viewsht.rom","MOS 3.20 Viewsheet (a)",StandardROM_MOS320_VIEWSHEET},
    {"m128/3.20/edit.rom","MOS 3.20 Edit (b)",StandardROM_MOS320_EDIT},
    {"m128/3.20/basic4.rom","MOS 3.20 BASIC (c)",StandardROM_MOS320_BASIC4},
    {"m128/3.20/adfs.rom","MOS 3.20 ADFS (d)",StandardROM_MOS320_ADFS},
    {"m128/3.20/view.rom","MOS 3.20 View (e)",StandardROM_MOS320_VIEW},
    {"m128/3.20/terminal.rom","MOS 3.20 Terminal (f)",StandardROM_MOS320_TERMINAL},
    {"m128/3.20/mos.rom","MOS 3.20 MOS (OS)",StandardROM_MOS320_MOS},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebROM BEEB_ROMS_MOS350[8]={
    {"m128/3.50/dfs.rom","MOS 3.50 DFS (9)",StandardROM_MOS350_DFS},
    {"m128/3.50/viewsht.rom","MOS 3.50 Viewsheet (a)",StandardROM_MOS350_VIEWSHEET},
    {"m128/3.50/edit.rom","MOS 3.50 Edit (b)",StandardROM_MOS350_EDIT},
    {"m128/3.50/basic4.rom","MOS 3.50 BASIC (c)",StandardROM_MOS350_BASIC4},
    {"m128/3.50/adfs.rom","MOS 3.50 ADFS (d)",StandardROM_MOS350_ADFS},
    {"m128/3.50/view.rom","MOS 3.50 View (e)",StandardROM_MOS350_VIEW},
    {"m128/3.50/terminal.rom","MOS 3.50 Terminal (f)",StandardROM_MOS350_TERMINAL},
    {"m128/3.50/mos.rom","MOS 3.50 MOS (OS)",StandardROM_MOS350_MOS},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebROM *const BEEB_ROMS[]={
    &BEEB_ROM_OS12,
    &BEEB_ROM_BPLUS_MOS,
    &BEEB_ROM_BASIC2,
    &BEEB_ROM_ACORN_DFS,
    &BEEB_ROM_WATFORD_DDFS_DDB2,
    &BEEB_ROM_WATFORD_DDFS_DDB3,
    &BEEB_ROM_OPUS_DDOS,
    &BEEB_ROM_OPUS_CHALLENGER,
    &BEEB_ROMS_MOS320[0],
    &BEEB_ROMS_MOS320[1],
    &BEEB_ROMS_MOS320[2],
    &BEEB_ROMS_MOS320[3],
    &BEEB_ROMS_MOS320[4],
    &BEEB_ROMS_MOS320[5],
    &BEEB_ROMS_MOS320[6],
    &BEEB_ROMS_MOS320[7],
    &BEEB_ROMS_MOS350[0],
    &BEEB_ROMS_MOS350[1],
    &BEEB_ROMS_MOS350[2],
    &BEEB_ROMS_MOS350[3],
    &BEEB_ROMS_MOS350[4],
    &BEEB_ROMS_MOS350[5],
    &BEEB_ROMS_MOS350[6],
    &BEEB_ROMS_MOS350[7],
    nullptr,
};

const BeebROM *FindBeebROM(StandardROM rom) {
    for(size_t i=0;BEEB_ROMS[i];++i) {
        if(BEEB_ROMS[i]->rom==rom) {
            return BEEB_ROMS[i];
        }
    }

    ASSERT(false);
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
