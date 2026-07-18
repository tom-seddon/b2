//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME SymbolGroupState
EBEGIN()
EPN(Disabled)
EPN(Indeterminate)
EPN(Enabled)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO: these need better names.
#define ENAME SymbolFileAddressSuffixMode
EBEGIN()

// Symbols are visible when any of the file's regions are in effect, and
// otherwise not visible, regardless of the symbol's address.
EPN(Exclusive)

// If a symbol's address is in one of the file's regions, it is visible when
// that region is in effect and otherwise not visible. Symbols in other regions
// are always visible.
EPN(Inclusive)

EEND_SERIALIZABLE("7d257a984fac8e94e1ee9cbd5692e1cc0384705d")
#undef ENAME
