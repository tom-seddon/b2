// UIFlag is arranged so that 0 means standard b2 behaviour.
#define ENAME UIFlag
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(HideMessagesPopup, 0)
EPN_BIT_FLAG(HideLEDsPopup, 1)
EPN_BIT_FLAG(HideDebuggerUI, 2)
EPN_BIT_FLAG(HideJobsPopup, 3)
EPN_BIT_FLAG(HideExtrasUI, 4)

EQPNV(HideAllPopups, UIFlag_HideMessagesPopup | UIFlag_HideLEDsPopup | UIFlag_HideJobsPopup)
EEND()
#undef ENAME
