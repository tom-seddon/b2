//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME WD1770State
EBEGIN()

EPN(BeginIdle)
EPN(IdleWithMotorOff)
EPN(SpinDown)
EPN(Wait)
EPN(WaitForSpinUp)
EPN(FinishTypeI)
EPN(RecordNotFound)
EPN(FinishCommand)
EPN(Restore)
EPN(Seek)
EPN(Seek2)
EPN(StepThenSeek2)
EPN(StepThenFinishStep)
EPN(StepThenRestore)
EPN(FinishStep)
EPN(StepIn)
EPN(StepOut)
EPN(ReadSector)
EPN(ReadSectorFindSector)
EPN(ReadSectorReadByte)
EPN(ReadSectorNextByte)
EPN(WriteSector)
EPN(WriteSectorFindSector)
EPN(WriteSectorSetFirstDRQ)
EPN(WriteSectorReceiveFirstDataByte)
EPN(WriteSectorWriteByte)
EPN(WriteSectorNextByte)
EPN(ReadAddress)
EPN(ReadAddressFindSector)
EPN(ReadAddressNextByte)
EPN(ForceInterrupt)
EPN(ForceInterrupt2)
EPN(ReadTrack)
EPN(WriteTrack)
EPN(UnsupportedCommand)
EPN(WriteProtectError)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
