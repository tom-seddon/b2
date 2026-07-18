#define ENAME BBCUTF8ConvertMode
EBEGIN()
// Pass all values through. BBC pound sign will come through as `.
EPN(PassThrough)

// Translate to teletext chars. [ will come through as left arrow, etc.
EPN(SAA5050)

// Translate pound sign only.
EPN(OnlyGBP)

// Must be last
EPN(Count)
EEND_SERIALIZABLE("a81331b6d2a38264fbe129b2b98af9206dbd7a80")
#undef ENAME
