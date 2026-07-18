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
EEND_SERIALIZABLE("bb3d245d07a47d86b39db4454c9e84c82ec523e1")
#undef ENAME
