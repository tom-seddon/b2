#define ENAME BBCUTF8ConvertMode
EBEGIN()
// Pass all values through. BBC £ will come through as `.
EPN(PassThrough)

// Translate to teletext chars. [ will come through as left arrow, etc.
EPN(SAA5050)

// Translate pound sign only.
EPN(OnlyGBP)

// Must be last
EPN(Count)
EEND()
#undef ENAME
