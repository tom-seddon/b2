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
    return ::GetAssetPath("roms", this->path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebROM BEEB_ROM_OS12 = {"OS12.ROM", "OS 1.20", StandardROM_OS12};
const BeebROM BEEB_ROM_BPLUS_MOS = {"B+MOS.rom", "B+ MOS", StandardROM_BPlusMOS};

const BeebROM BEEB_ROM_BASIC2 = {"BASIC2.ROM", "BASIC II", StandardROM_BASIC2};
const BeebROM BEEB_ROM_ACORN_DFS = {"DFS-2.26.rom", "Acorn 1770 DFS", StandardROM_Acorn1770DFS};
const BeebROM BEEB_ROM_WATFORD_DDFS_DDB2 = {"DDFS-1.53.rom", "Watford DDFS (DDB2)", StandardROM_WatfordDDFS_DDB2};
const BeebROM BEEB_ROM_WATFORD_DDFS_DDB3 = {"DDFS-1.54T.rom", "Watford DDFS (DDB3)", StandardROM_WatfordDDFS_DDB3};
const BeebROM BEEB_ROM_OPUS_DDOS = {"OPUS-DDOS-3.45.rom", "Opus DDOS", StandardROM_OpusDDOS};
const BeebROM BEEB_ROM_OPUS_CHALLENGER = {"challenger-1.01.rom", "Opus Challenger", StandardROM_OpusChallenger};

const BeebROM BEEB_ROM_MASTER_TURBO_PARASITE = {"MasterTurboParasite.rom", "65C102 TUBE 1.20 (Master Turbo)", StandardROM_MasterTurboParasite};
const BeebROM BEEB_ROM_TUBE110 = {"TUBE110.rom", "6502 TUBE 1.10 (6502 Second Processor)", StandardROM_TUBE110};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const BeebROM BEEB_ROM_MOS320_SIDEWAYS_ROM_9 = {"m128/3.20/dfs.rom", "MOS 3.20 DFS (9)", StandardROM_MOS320_DFS};
extern const BeebROM BEEB_ROM_MOS320_SIDEWAYS_ROM_A = {"m128/3.20/viewsht.rom", "MOS 3.20 Viewsheet (a)", StandardROM_MOS320_VIEWSHEET};
extern const BeebROM BEEB_ROM_MOS320_SIDEWAYS_ROM_B = {"m128/3.20/edit.rom", "MOS 3.20 Edit (b)", StandardROM_MOS320_EDIT};
extern const BeebROM BEEB_ROM_MOS320_SIDEWAYS_ROM_C = {"m128/3.20/basic4.rom", "MOS 3.20 BASIC (c)", StandardROM_MOS320_BASIC4};
extern const BeebROM BEEB_ROM_MOS320_SIDEWAYS_ROM_D = {"m128/3.20/adfs.rom", "MOS 3.20 ADFS (d)", StandardROM_MOS320_ADFS};
extern const BeebROM BEEB_ROM_MOS320_SIDEWAYS_ROM_E = {"m128/3.20/view.rom", "MOS 3.20 View (e)", StandardROM_MOS320_VIEW};
extern const BeebROM BEEB_ROM_MOS320_SIDEWAYS_ROM_F = {"m128/3.20/terminal.rom", "MOS 3.20 Terminal (f)", StandardROM_MOS320_TERMINAL};
extern const BeebROM BEEB_ROM_MOS320_MOS_ROM = {"m128/3.20/mos.rom", "MOS 3.20 MOS (OS)", StandardROM_MOS320_MOS};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const BeebROM BEEB_ROM_MOS350_SIDEWAYS_ROM_9 = {"m128/3.50/dfs.rom", "MOS 3.50 DFS (9)", StandardROM_MOS350_DFS};
extern const BeebROM BEEB_ROM_MOS350_SIDEWAYS_ROM_A = {"m128/3.50/viewsht.rom", "MOS 3.50 Viewsheet (a)", StandardROM_MOS350_VIEWSHEET};
extern const BeebROM BEEB_ROM_MOS350_SIDEWAYS_ROM_B = {"m128/3.50/edit.rom", "MOS 3.50 Edit (b)", StandardROM_MOS350_EDIT};
extern const BeebROM BEEB_ROM_MOS350_SIDEWAYS_ROM_C = {"m128/3.50/basic4.rom", "MOS 3.50 BASIC (c)", StandardROM_MOS350_BASIC4};
extern const BeebROM BEEB_ROM_MOS350_SIDEWAYS_ROM_D = {"m128/3.50/adfs.rom", "MOS 3.50 ADFS (d)", StandardROM_MOS350_ADFS};
extern const BeebROM BEEB_ROM_MOS350_SIDEWAYS_ROM_E = {"m128/3.50/view.rom", "MOS 3.50 View (e)", StandardROM_MOS350_VIEW};
extern const BeebROM BEEB_ROM_MOS350_SIDEWAYS_ROM_F = {"m128/3.50/terminal.rom", "MOS 3.50 Terminal (f)", StandardROM_MOS350_TERMINAL};
extern const BeebROM BEEB_ROM_MOS350_MOS_ROM = {"m128/3.50/mos.rom", "MOS 3.50 MOS (OS)", StandardROM_MOS350_MOS};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BeebROM *const BEEB_ROMS[] = {
    &BEEB_ROM_OS12,
    &BEEB_ROM_BPLUS_MOS,

    &BEEB_ROM_BASIC2,
    &BEEB_ROM_ACORN_DFS,
    &BEEB_ROM_WATFORD_DDFS_DDB2,
    &BEEB_ROM_WATFORD_DDFS_DDB3,
    &BEEB_ROM_OPUS_DDOS,
    &BEEB_ROM_OPUS_CHALLENGER,

    &BEEB_ROM_MOS320_SIDEWAYS_ROM_9,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_A,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_B,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_C,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOS320_SIDEWAYS_ROM_F,
    &BEEB_ROM_MOS320_MOS_ROM,

    &BEEB_ROM_MOS350_SIDEWAYS_ROM_9,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_A,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_B,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_C,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_D,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_E,
    &BEEB_ROM_MOS350_SIDEWAYS_ROM_F,
    &BEEB_ROM_MOS350_MOS_ROM,

    &BEEB_ROM_MASTER_TURBO_PARASITE,
    &BEEB_ROM_TUBE110,

    nullptr,
};

const BeebROM *FindBeebROM(StandardROM rom) {
    for (size_t i = 0; BEEB_ROMS[i]; ++i) {
        if (BEEB_ROMS[i]->rom == rom) {
            return BEEB_ROMS[i];
        }
    }

    ASSERT(false);
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
