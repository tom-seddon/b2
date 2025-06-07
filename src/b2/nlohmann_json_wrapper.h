// See https://github.com/cspiegel/garglk/commit/1734a95cca91d923dc3cc254b12944805c57f7df

// See https://github.com/nlohmann/json/issues/3808

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#include <nlohmann/json.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
