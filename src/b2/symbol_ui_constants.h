#ifndef HEADER_E0EBF641_761B_49B6_A63A_F058D534FBFC
#define HEADER_E0EBF641_761B_49B6_A63A_F058D534FBFC

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace SymbolUI {

    // Group Management Window dimensions
    constexpr float MANAGEMENT_TABLE_RESERVED_HEIGHT = 100.0f; // Space for buttons below table

    // Context Edit Modal dimensions
    constexpr float CONTEXT_MODAL_WIDTH = 600.0f;
    constexpr float CONTEXT_MODAL_HEIGHT = 400.0f;

    // Table column widths
    constexpr float COL_INDEX_WIDTH = 20.0f;
    constexpr float COL_SELECT_WIDTH = 15.0f;
    constexpr float COL_MOVE_WIDTH = 60.0f;
    constexpr float COL_ENABLED_WIDTH = 80.0f;
    constexpr float COL_GROUP_NAME_WIDTH = 120.0f;
    constexpr float COL_COUNT_WIDTH = 40.0f;
    constexpr float COL_CONTEXTS_WIDTH = 100.0f;
    constexpr float COL_SOURCE_FILE_WIDTH = 220.0f;

    // UI Layout constants
    constexpr int MAX_GROUP_NAME_LENGTH = 255;

    // Help text sizing
    constexpr float CONTEXT_HELP_LINES = 11.0f; // Number of lines in context help

    // Button spacing
    constexpr float ARROW_BUTTON_SPACING = 2.0f;

} // namespace SymbolUI

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // SYMBOL_UI_CONSTANTS_H
