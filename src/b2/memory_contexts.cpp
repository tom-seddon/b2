#include "memory_contexts.h"
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace MemoryContexts {

    bool IsValidContext(char c) {
        return std::find(ALL_CONTEXTS.begin(), ALL_CONTEXTS.end(), c) != ALL_CONTEXTS.end();
    }

} // namespace MemoryContexts
