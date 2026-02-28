//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME ROMEditAction
EBEGIN()
EPNV(None, 0) //explicitly false
EPN(Edit)
EPN(MoveUp)
EPN(MoveDown)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME ROMEditFlag
EBEGIN_DERIVED(uint32_t)
EPNV(CanMoveUp, 1 << 0)
EPNV(CanMoveDown, 1 << 1)
EPNV(BSidewaysROMs, 1 << 2)
EPNV(BOSROMs, 1 << 3)
EPNV(BPlusSidewaysROMs, 1 << 4)
EPNV(BPlusOSROMs, 1 << 5)
EPNV(Master128SidewaysROMs, 1 << 6)
EPNV(Master128OSROMs, 1 << 7)
EPNV(ParasiteROMs, 1 << 8)
EPNV(MasterCompactSidewaysROMs, 1 << 9)
EPNV(MasterCompactOSROMs, 1 << 10)
EPNV(ContainedInOSROM, 1 << 11)
EPNV(NotAccessibleWithoutROMBoard, 1 << 12)
EPNV(NotAvailable, 1 << 13)
EPNV(ElectronSidewaysROMs, 1 << 14)
EPNV(ElectronOSROMs, 1 << 15)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#define ENAME ROMEditBankType
//EBEGIN_DERIVED(uint8_t)
//EPNV(Normal)
//EPNV(NotAvailable)
//EPNV(NotAccessibleWithoutROMBoard)
//EPNV(ContainedInOSROM)
//EEND()
//#undef ENAME
