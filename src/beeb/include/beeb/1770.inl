//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME WD1770State
EBEGIN()

// If disc motor is on, wait 10 revolutions then go to SpinDown;
// otherwise, go to IdleWithMotorOff.
EPN(BeginIdle)

// Do nothing.
EPN(IdleWithMotorOff)

// Switch disc motor off then go to IdleWithMotorOff.
EPN(SpinDown)

// Wait for WAIT_US microseconds. Then go to NEXT_STATE.
EPN(Wait)

// Wait for WAIT_US microseconds. Then set the SPIN-UP status flag and
// go to NEXT_STATE.
EPN(WaitForSpinUp)

// If command v bit is set, do the settle delay, then go to
// FinishCommand; else, go to FinishCommand.
EPN(FinishTypeI)

// Set the Record Not Found status bit, then go to FinishCommand.
EPN(RecordNotFound)

// Reset the busy flag, request an interrupt, and go to Idle.
EPN(FinishCommand)

// If drive is at track 0, FinishTypeI. Otherwise, set DIRECTION to 0, set
// NEXT_STATE to Restore, and go to StepOnce. (So this loops doing a
// step in until track 0 is reached.)
EPN(Restore)

// If DSR==TRACK, FinishTypeI. Otherwise, set DIRECTION based on the
// difference between DSR and TRACK, and go to StepOnce.
EPN(Seek)
EPN(Seek2)

// Set NEXT_STATE to Idle, then go to StepOnce.
EPN(Step)

EPN(FinishStep)

// Set NEXT_STATE to Idle, set DIRECTION to 0, then go to StepOnce.
EPN(StepIn)

// Set NEXT_STATE to Idle, set DIRECTION to 1, then go to StepOnce.
EPN(StepOut)

// If the E command bit is set, wait 30ms. Next state is
// ReadSectorFindSector anyway.
EPN(ReadSector)

// Checks the sector address is valid and gets that sector's size. Go
// to ReadSectorNextByte if it's OK, or RecordNotFound of not.
EPN(ReadSectorFindSector)

// Present CPU with the next byte, then wait for 1 byte, then go to
// ReadSectorNextByte.
EPN(ReadSectorReadByte)

// Advance to next read address in sector. If that was the last byte,
// go to FinishCommand (single sector)/ReadSectorFindSector (multi
// sector); otherwise, go to ReadSectorReadByte.
EPN(ReadSectorNextByte)

// If the E bit is set, wait 30ms. Next state is
// WriteSectorFindSector.
EPN(WriteSector)

// Checks the sector address is valid and gets that sector's size. If
// it's OK, go to WriteSectorSetFirstDRQ after a 2 byte delay; if it's
// not, go straight away to RecordNotFound.
EPN(WriteSectorFindSector)

// Set DRQ, delay 9 bytes, then go to WriteSectorReceiveFirstDataByte.
EPN(WriteSectorSetFirstDRQ)

// If data register empty, cancel command. Otherwise go to
// WriteSectorWriteByte after a 1 byte delay.
EPN(WriteSectorReceiveFirstDataByte)

// Write data register (or 0 if empty) to disc. If that was the last
// byte, go to FinishCommand (single sector)/WriteSectorFindSector
// (multi sector); otherwise, go to WriteSectorRequestNextByte.
EPN(WriteSectorWriteByte)

// Set DRQ, delay 1 byte, then go to WriteSectorWriteByte.
EPN(WriteSectorNextByte)

// Do the 30ms E delay if necessary. Next state is ReadAddressFindSector.
EPN(ReadAddress)

// Find the sector. Finish command if not found or invalid sector
// size; otherwise, go to ReadAddressNextByte.
EPN(ReadAddressFindSector)

// If another address byte to deliver, put byte in data register and
// set DRQ. Otherwise, go to FinishCommand.
EPN(ReadAddressNextByte)

// Set a timer to do the actual Force Interrupt after a short delay.
EPN(ForceInterrupt)

// Do the actual Force Interrupt.
EPN(ForceInterrupt2)

// Do the E delay then go to UnsupportedCommand.
EPN(ReadTrack)

// Do the E delay then go to UnsupportedCommand.
EPN(WriteTrack)

// Sets RNF and CRC error bits and goes to FinishCommand.
EPN(UnsupportedCommand)

// Sets WRITE PROTECT error bit and goes to FinishCommand.
EPN(WriteProtectError)

EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
