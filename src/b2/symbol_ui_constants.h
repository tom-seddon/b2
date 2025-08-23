#ifndef SYMBOL_UI_CONSTANTS_H
#define SYMBOL_UI_CONSTANTS_H

#include <cstddef> // for size_t

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace SymbolUI {

    // Symbol Loading Window dimensions
    constexpr float LOADING_WINDOW_WIDTH_BASIC = 500.0f;
    constexpr float LOADING_WINDOW_HEIGHT_BASIC = 200.0f;
    constexpr float LOADING_WINDOW_WIDTH_ADVANCED = 600.0f;
    constexpr float LOADING_WINDOW_HEIGHT_ADVANCED = 500.0f;

    // Group Management Window dimensions
    constexpr float MANAGEMENT_WINDOW_WIDTH = 750.0f;
    constexpr float MANAGEMENT_WINDOW_HEIGHT = 500.0f;
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
    constexpr int CONTEXTS_PER_ROW = 8;          // For ROM banks display
    constexpr int CONTEXTS_PER_TOOLTIP_LINE = 6; // For tooltip formatting
    constexpr int MAX_GROUP_NAME_LENGTH = 255;
    constexpr size_t MAX_FILE_PATH_LENGTH = 511; // -1 for null terminator = 512

    // Help text sizing
    constexpr float CONTEXT_HELP_LINES = 11.0f; // Number of lines in context help

    // UI colors (ImGui RGBA format)
    constexpr unsigned int SELECTED_ROW_COLOR = 0x28C8DC46; // IM_COL32(70, 140, 200, 40)

    // Button spacing
    constexpr float ARROW_BUTTON_SPACING = 2.0f;

} // namespace SymbolUI

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // SYMBOL_UI_CONSTANTS_H
