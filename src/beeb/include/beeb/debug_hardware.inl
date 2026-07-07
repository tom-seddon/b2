#define ENAME DebugCommand
EBEGIN_DERIVED(uint8_t)
EQPNV(Reserved_00, 0x00)
EQPNV(Reserved_01, 0x01)
EPNV(Reset, 0x02)
EPNV(EnableSymbolGroup, 0x03)
EPNV(DisableSymbolGroup, 0x04)
EPNV(DisableAllSymbolGroups, 0x05)
EEND()
#undef ENAME
