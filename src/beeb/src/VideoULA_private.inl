#define ENAME VideoNuLAModeFlag
EBEGIN()

// 0,1
EPNV(ModeShift, 0)
EPNV(ModeMask, 3)

// 2
EPNV(Fast6845Shift, 2)
EPNV(Fast6845, 1 << VideoNuLAModeFlag_Fast6845Shift)

// 3
EQPNV(TextAttributeModeShift, 3)
EPNV(TextAttributeMode, 1 << VideoNuLAModeFlag_TextAttributeModeShift)

// 4,5
EQPNV(AttributeModeShift, 4)
EQPNV(AttributeModeMask, 3)

// 6
EPNV(LogicalPaletteShift, 6)
EPNV(LogicalPalette, 1 << VideoNuLAModeFlag_LogicalPaletteShift)
EEND()
#undef ENAME