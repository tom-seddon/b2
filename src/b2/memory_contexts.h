#ifndef MEMORY_CONTEXTS_H
#define MEMORY_CONTEXTS_H

#include <array>
#include <string>
#include <set>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace MemoryContexts {

    // All valid BBC Micro memory contexts (40 total)
    constexpr std::array<char, 40> ALL_CONTEXTS = {
        // Core contexts (8)
        'm', 'o', 's', 'n', 'h', 'i', 'p', 'r',
        // Standard ROM banks 0-15 (16)
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
        // ROM mapper contexts A-P (16)
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P'};

    // Universal contexts (common ones for cache optimization)
    constexpr std::array<char, 22> UNIVERSAL_CACHE_CONTEXTS = {
        'm', 'o', '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', 's', 'n', 'h', 'i'};

    // Context categories for UI organization
    struct ContextInfo {
        char context;
        const char *description;
    };

    struct ContextGroup {
        const char *name;
        std::array<ContextInfo, 16> contexts; // Max 16 contexts per group
        size_t count;
        bool collapsible;
    };

    // Organized context groups for UI display
    constexpr std::array<ContextGroup, 6> CONTEXT_GROUPS = {{{"Common",
                                                              {{{'m', "Main RAM"},
                                                                {'o', "OS ROM (MOS)"}}},
                                                              2,
                                                              false},
                                                             {"ROM Banks 0-15",
                                                              {{{'0', ""}, {'1', ""}, {'2', ""}, {'3', ""}, {'4', ""}, {'5', ""}, {'6', ""}, {'7', ""}, {'8', ""}, {'9', ""}, {'a', ""}, {'b', ""}, {'c', ""}, {'d', ""}, {'e', ""}, {'f', ""}}},
                                                              16,
                                                              false},
                                                             {"Shadow/Extra RAM",
                                                              {{{'s', "Shadow RAM"},
                                                                {'n', "ANDY (extra RAM)"},
                                                                {'h', "HAZEL (Master)"}}},
                                                              3,
                                                              true},
                                                             {"Parasite (Second Processor)",
                                                              {{{'p', "Parasite RAM"},
                                                                {'r', "Parasite boot ROM"}}},
                                                              2,
                                                              true},
                                                             {"ROM Mappers (A-P)",
                                                              {{{'A', ""}, {'B', ""}, {'C', ""}, {'D', ""}, {'E', ""}, {'F', ""}, {'G', ""}, {'H', ""}, {'I', ""}, {'J', ""}, {'K', ""}, {'L', ""}, {'M', ""}, {'N', ""}, {'O', ""}, {'P', ""}}},
                                                              16,
                                                              true},
                                                             {"Other",
                                                              {{{'i', "I/O area"}}},
                                                              1,
                                                              true}}};

    // Context validation and utility functions
    bool IsValidContext(char c);

} // namespace MemoryContexts

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // MEMORY_CONTEXTS_H
