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
EPN_BIT_FLAG(CanMoveUp, 0)
EPN_BIT_FLAG(CanMoveDown, 1)
EPN_BIT_FLAG(BSidewaysROMs, 2)
EPN_BIT_FLAG(BOSROMs, 3)
EPN_BIT_FLAG(BPlusSidewaysROMs, 4)
EPN_BIT_FLAG(BPlusOSROMs, 5)
EPN_BIT_FLAG(Master128SidewaysROMs, 6)
EPN_BIT_FLAG(Master128OSROMs, 7)
EPN_BIT_FLAG(ParasiteROMs, 8)
EPN_BIT_FLAG(MasterCompactSidewaysROMs, 9)
EPN_BIT_FLAG(MasterCompactOSROMs, 10)
EPN_BIT_FLAG(ContainedInOSROM, 11)
EPN_BIT_FLAG(NotAccessibleWithoutROMBoard, 12)
EPN_BIT_FLAG(NotAvailable, 13)
EPN_BIT_FLAG(ElectronSidewaysROMs, 14)
EPN_BIT_FLAG(ElectronOSROMs, 15)
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
