// UIFlag is arranged so that 0 means standard b2 behaviour.
#define ENAME UIFlag
EBEGIN()
EPNV(HideMessagesPopup, 1 << 0)
EPNV(HideLEDsPopup, 1 << 1)
EPNV(HideDebuggerUI, 1 << 2)
EPNV(HideJobsPopup, 1 << 3)
EPNV(HideExtrasUI, 1 << 4)

EQPNV(HideAllPopups, UIFlag_HideMessagesPopup | UIFlag_HideLEDsPopup | UIFlag_HideJobsPopup)
EEND()
#undef ENAME
