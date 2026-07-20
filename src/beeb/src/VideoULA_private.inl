#define ENAME VideoNuLAModeFlag
EBEGIN_DERIVED(uint32_t)

// 0,1
EPN_BIT_FIELD(Mode, 0, 2)

// 2
EPN_BIT_FLAG(Fast6845, 2)

// 3
EPN_BIT_FLAG(TextAttributeMode, 3)

// 4,5
EPN_BIT_FIELD(AttributeMode, 4, 2)

// 6
EPN_BIT_FLAG(LogicalPalette, 6)
EEND()
#undef ENAME
