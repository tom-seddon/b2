#include "dear_imgui_hex_editor.h"
#include <algorithm>
#include <ctype.h>
#include <inttypes.h>
#include <vector>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER

#pragma warning(push)
#pragma warning(disable : 4458) //declaration of 'identifier' hides class member
#pragma warning(disable : 4267) //'var' : conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable : 4305) //'identifier' : truncation from 'type1' to 'type2'
#pragma warning(disable : 4100) //'identifier' : unreferenced formal parameter
#pragma warning(disable : 4800) //'type' : forcing value to bool 'true' or 'false' (performance warning)

#elif defined __GNUC__

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif

#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined __GNUC__
#pragma GCC diagnostic pop
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char HEX_CHARS_UC[] = "0123456789ABCDEF";
static const char HEX_CHARS_LC[] = "0123456789abcdef";

static const char OPTIONS_POPUP_NAME[] = "hex_editor_options";
static const char CONTEXT_POPUP_NAME[] = "hex_editor_context";

static constexpr int NUM_SIZE_T_CHARS = sizeof(size_t) * 2;
static constexpr int NUM_PTR_CHARS = sizeof(void *) * 2;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TextColourHandler {
    uint32_t colour = 0;
    bool pushed = false;

    TextColourHandler() {
    }

    ~TextColourHandler() {
        this->Pop();
    }

    void BeginColour(uint32_t new_colour) {
        if (new_colour != this->colour) {
            this->Pop();

            ImGui::PushStyleColor(ImGuiCol_Text, new_colour);
            this->colour = new_colour;
            this->pushed = true;
        }
    }

    void Pop() {
        if (this->pushed) {
            ImGui::PopStyleColor(1);
            this->pushed = false;
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorHandler::HexEditorHandler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorHandler::~HexEditorHandler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandler::GetAddressText(char *text,
                                      size_t text_size,
                                      size_t offset,
                                      bool upper_case) {
    if (upper_case) {
        snprintf(text, text_size, "%0*zX", NUM_SIZE_T_CHARS, offset);
    } else {
        snprintf(text, text_size, "%0*zx", NUM_SIZE_T_CHARS, offset);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HexEditorHandler::ParseAddressText(size_t *offset, const char *text) {
    char *ep;
    unsigned long long value = strtoull(text, &ep, 16);

    if (*ep != 0 && !isspace(*ep)) {
        return false;
    }

    if (value > SIZE_MAX) {
        return false;
    }

    *offset = (size_t)value;
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandler::DoOptionsPopupExtraGui() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandler::DoContextPopupExtraGui(bool hex, size_t offset) {
    (void)hex, (void)offset;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandler::DebugPrint(const char *fmt, ...) {
    (void)fmt;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HexEditorHandler::GetNumAddressChars() {
    return NUM_SIZE_T_CHARS;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<std::string> *HexEditorHandler::GetCharFromByteTranslationTable() {
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HexEditorHandler::GetByteForWchar(uint32_t ch) {
    if (ch >= 32 && ch < 127) {
        return (int)ch;
    } else {
        return -1;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorHandlerWithBufferData::HexEditorHandlerWithBufferData(void *buffer, size_t buffer_size) {
    this->Construct(buffer, buffer, buffer_size);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorHandlerWithBufferData::HexEditorHandlerWithBufferData(const void *buffer, size_t buffer_size) {
    this->Construct(buffer, nullptr, buffer_size);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::ReadByte(HexEditorByte *byte, size_t offset) {
    IM_ASSERT(offset < m_buffer_size);

    byte->got_value = true;
    byte->value = m_read_buffer[offset];
    byte->can_write = !!m_write_buffer;
    byte->colour = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::WriteByte(size_t offset, uint8_t value) {
    IM_ASSERT(m_write_buffer);
    IM_ASSERT(offset < m_buffer_size);

    m_write_buffer[offset] = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t HexEditorHandlerWithBufferData::GetSize() {
    return m_buffer_size;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::GetAddressText(char *text,
                                                    size_t text_size,
                                                    size_t offset,
                                                    bool upper_case) {
    if (m_show_address) {
        if (upper_case) {
            snprintf(text, text_size, "%0*" PRIXPTR, NUM_PTR_CHARS, (uintptr_t)(m_read_buffer + offset));
        } else {
            snprintf(text, text_size, "%0*" PRIxPTR, NUM_PTR_CHARS, (uintptr_t)(m_read_buffer + offset));
        }
    } else {
        this->HexEditorHandler::GetAddressText(text, text_size, offset, upper_case);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::DoOptionsPopupExtraGui() {
    ImGui::Checkbox("Show address", &m_show_address);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HexEditorHandlerWithBufferData::GetNumAddressChars() {
    if (m_show_address) {
        return NUM_PTR_CHARS;
    } else {
        return this->HexEditorHandler::GetNumAddressChars();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::Construct(const void *read_buffer, void *write_buffer, size_t buffer_size) {
    m_read_buffer = (const uint8_t *)read_buffer;
    m_write_buffer = (uint8_t *)write_buffer;
    m_buffer_size = buffer_size;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditor::HexEditor(HexEditorHandler *handler)
    : m_handler(handler) {
    this->SetNumColumns(DEFAULT_NUM_COLUMNS);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditor::~HexEditor() {
    delete[] m_bytes;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoImGui() {
    ImGui::BeginChild("hex");

    const ImGuiStyle &style = ImGui::GetStyle();

    this->GetMetrics(&m_metrics, style);
    m_highlight_colour = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);

    m_style_frame_padding_x = style.FramePadding.x;
    m_style_frame_padding_y = style.FramePadding.y;

    m_style_item_spacing_x = style.ItemSpacing.x;
    m_style_item_spacing_y = style.ItemSpacing.y;

    const float footer_height = style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    const size_t data_size = m_handler->GetSize();

    m_was_TextInput_visible = false;

    if (m_num_columns > m_num_bytes) {
        delete[] m_bytes;

        m_num_bytes = m_num_columns;
        m_bytes = new HexEditorByte[m_num_bytes];
    }

    if (this->options.headers) {
        TextColourHandler tch;
        tch.BeginColour(m_metrics.grey_colour);

        ImGui::SetCursorPosX(-m_scroll_x);
        ImGui::Text("Address");

        const char *hex_chars = this->GetHexChars();

        if (this->options.hex) {
            for (size_t i = 0; i < m_num_columns; ++i) {
                // Do this 2-step thing to avoid some flickering when SameLine
                // would end up being called with (0,0).
                ImGui::SameLine(0.f, 0.f);
                ImGui::SetCursorPosX(-m_scroll_x + m_metrics.hex_left_x + i * m_metrics.hex_column_width);
                ImGui::Text("%c%c", hex_chars[i >> 4 & 0xf], hex_chars[i >> 0 & 0xf]);
            }
        }

        if (this->options.ascii) {
            for (size_t i = 0; i < m_num_columns; ++i) {
                ImGui::SameLine(0.f, 0.f);
                ImGui::SetCursorPosX(-m_scroll_x + m_metrics.ascii_left_x + i * m_metrics.glyph_width);
                ImGui::Text("%c", hex_chars[i & 0xf]);
            }
        }
    }

    // Main scroll area.
    int first_visible_row = 0;
    int num_visible_rows;
    {
        ImGui::BeginChild("##scrolling",
                          ImVec2(0, -footer_height),
                          false,
                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

        m_scroll_x = ImGui::GetScrollX();

        num_visible_rows = (std::max)(1, (int)(ImGui::GetContentRegionAvail().y / m_metrics.line_height));

        if (m_next_frame_scroll_y >= 0.f) {
            ImGui::SetScrollY(m_next_frame_scroll_y);
            m_next_frame_scroll_y = -1.f;
        }

        {
            m_draw_list = ImGui::GetWindowDrawList();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            {
                size_t offset0_column = m_offset0_column % m_num_columns;
                size_t num_lines = (offset0_column + m_handler->GetSize() + m_num_columns - 1) / m_num_columns;
                IM_ASSERT(num_lines <= INT_MAX);
                ImGuiListClipper clipper;
                clipper.Begin((int)num_lines, m_metrics.line_height);

                while (clipper.Step()) {
                    first_visible_row = clipper.DisplayStart;

                    for (int line_index = clipper.DisplayStart; line_index < clipper.DisplayEnd; ++line_index) {
                        size_t line_begin_offset = (size_t)line_index * m_num_columns;
                        size_t line_end_offset = line_begin_offset + m_num_columns;
                        size_t num_skip_columns;
                        if (line_index == 0) {
                            num_skip_columns = offset0_column;
                            line_end_offset -= offset0_column;
                        } else {
                            num_skip_columns = 0;
                            line_begin_offset -= offset0_column;
                            line_end_offset -= offset0_column;
                        }
                        line_end_offset = (std::min)(line_end_offset, data_size);

                        char addr_text[100];
                        m_handler->GetAddressText(addr_text, sizeof addr_text, line_begin_offset, this->options.upper_case);

                        ImGui::Text("%.*s", m_metrics.num_addr_chars, addr_text);

                        {
                            HexEditorByte *byte = m_bytes;
                            for (size_t offset = line_begin_offset; offset != line_end_offset; ++offset) {
                                m_handler->ReadByte(byte++, offset);
                            }
                            IM_ASSERT(byte <= m_bytes + m_num_bytes);
                        }

                        ImGui::PushID(line_index);
                        if (this->options.hex) {
                            this->DoHexPart(num_skip_columns, line_begin_offset, line_end_offset);
                        }

                        if (this->options.ascii) {
                            this->DoAsciiPart(line_begin_offset, line_end_offset);
                        }
                        ImGui::PopID();
                    }
                }
            }

            ImGui::PopStyleVar(2);

            m_draw_list = nullptr;
        }

        ImGui::EndChild();
    }

    // Options and stuff.
    if (ImGui::Button("Options")) {
        ImGui::OpenPopup(OPTIONS_POPUP_NAME);
    }

    if (ImGui::BeginPopup(OPTIONS_POPUP_NAME)) {
        this->DoOptionsPopup();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    {
        ImGui::PushID("address");
        ImGui::PushItemWidth((m_metrics.num_addr_chars + 2) * m_metrics.glyph_width);
        int flags = ImGuiInputTextFlags_EnterReturnsTrue;
        bool entered_new_address = ImGui::InputText("Address", m_new_offset_input_buffer, sizeof m_new_offset_input_buffer, flags);
        ImGui::SameLine();
        bool clicked_go = ImGui::Button("Go");
        ImGui::SameLine();
        bool clicked_go_realign = ImGui::Button("Go (realign)");
        if (entered_new_address || clicked_go || clicked_go_realign) {
            size_t offset;
            if (m_handler->ParseAddressText(&offset, m_new_offset_input_buffer)) {
                this->SetNewOffset((size_t)offset, 0, false);
                strcpy(m_new_offset_input_buffer, "");
                if (clicked_go_realign) {
                    if (m_new_offset != INVALID_OFFSET) {
                        m_offset0_column = m_num_columns - m_new_offset % m_num_columns;
                    }
                }
            }
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    if (m_offset != INVALID_OFFSET) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            this->UpdateOffsetByKey(ImGuiKey_UpArrow, -(int)m_num_columns, 1);
            this->UpdateOffsetByKey(ImGuiKey_DownArrow, (int)m_num_columns, 1);
            this->UpdateOffsetByKey(ImGuiKey_LeftArrow, -1, 1);
            this->UpdateOffsetByKey(ImGuiKey_RightArrow, 1, 1);
            this->UpdateOffsetByKey(ImGuiKey_PageUp, -(int)m_num_columns, num_visible_rows);
            this->UpdateOffsetByKey(ImGuiKey_PageDown, (int)m_num_columns, num_visible_rows);

            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))) {
                m_hex = !m_hex;
                m_input_take_focus_next_frame = true;
            } else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
                this->SetNewOffset(INVALID_OFFSET, 0, true);
            }
        }
    }

    // Update stuff. One slightly annoying thing about the imgui paradigm is
    // that values can change mid-render, as the rendering is the update as
    // well. Try to avoid this by doing all the major updates at the end.

    if (m_set_new_offset) {
        m_offset = m_new_offset;

        if (!m_set_new_offset_via_SetOffset) {
            m_input_take_focus_next_frame = true;
        } else {
            m_input_take_focus_next_frame = false;
        }

        m_handler->DebugPrint("%zu - set new offset: now 0x%zx\n", m_num_calls, m_offset);
        //m_handler->DebugPrint("%zu - clipper = %d -> %d\n",m_num_calls,clipper_display_start,clipper_display_end);

        if (m_hex) {
            m_editing_high_nybble = true;
        }

        if (m_offset == INVALID_OFFSET) {
            m_got_edit_value = false;
        } else {
            HexEditorByte byte;
            m_handler->ReadByte(&byte, m_offset);

            if (byte.got_value) {
                m_got_edit_value = true;
                m_edit_value = byte.value;
            } else {
                m_got_edit_value = false;
            }

            size_t row = m_offset / m_num_columns;
            if (row > INT_MAX) {
                row = INT_MAX;
            }

            if ((int)row < first_visible_row) {
                m_next_frame_scroll_y = row * m_metrics.line_height;
            } else if ((int)row >= first_visible_row + num_visible_rows) {
                m_next_frame_scroll_y = (row - ((size_t)num_visible_rows - 1u)) * m_metrics.line_height;
            }

            //m_handler->DebugPrint("row=%zu, clipper start=%d, clipper end=%d, scroll next frame=%f\n",row,clipper_display_start,clipper_display_end,m_next_frame_scroll_y);
        }
    } else {
        if (m_offset != INVALID_OFFSET) {
            if (!m_was_TextInput_visible) {
                // sort out TextInput again when it next becomes visible.
                if (!m_set_new_offset_via_SetOffset) {
                    m_input_take_focus_next_frame = true;
                } else {
                    m_input_take_focus_next_frame = false;
                }
            }
        }

        m_set_new_offset_via_SetOffset = false;
    }

    if (m_set_new_num_columns) {
        this->SetNumColumns(m_new_num_columns);

        m_set_new_num_columns = false;
    }

    ImGui::EndChild();

    ++m_num_calls;

    m_set_new_offset = false;
    m_new_offset = INVALID_OFFSET;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t HexEditor::GetOffset() const {
    if (m_set_new_offset) {
        return m_new_offset;
    } else {
        return m_offset;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::SetOffset(size_t offset) {
    this->SetNewOffset(offset, 0, false);
    m_set_new_offset_via_SetOffset = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t HexEditor::GetNumColumns() const {
    return m_num_columns;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::SetNumColumns(size_t num_columns) {
    m_num_columns = num_columns;

    snprintf(m_new_num_columns_input_buffer,
             sizeof m_new_num_columns_input_buffer,
             "%zu",
             m_num_columns);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static float GetHalf(float a, float b) {
    float a_linear = a * a;
    float b_linear = b * b;
    float half_linear = (a_linear + b_linear) * 0.5f;
    return sqrtf(half_linear);
}

void HexEditor::GetMetrics(Metrics *metrics, const ImGuiStyle &style) {
    (void)style;

    metrics->num_addr_chars = m_handler->GetNumAddressChars();

    metrics->line_height = ImGui::GetTextLineHeight();
    metrics->glyph_width = ImGui::CalcTextSize("F").x + 1;

    metrics->hex_left_x = (metrics->num_addr_chars + 2) * metrics->glyph_width;
    metrics->hex_column_width = 2.5f * metrics->glyph_width;

    metrics->ascii_left_x = metrics->hex_left_x;
    if (this->options.hex) {
        metrics->ascii_left_x += m_num_columns * metrics->hex_column_width + 2.f * metrics->glyph_width;
    }

    const ImVec4 &text_colour = style.Colors[ImGuiCol_Text];
    const ImVec4 &text_disabled_colour = style.Colors[ImGuiCol_TextDisabled];
    ImVec4 grey_colour(GetHalf(text_colour.x, text_disabled_colour.x),
                       GetHalf(text_colour.y, text_disabled_colour.y),
                       GetHalf(text_colour.z, text_disabled_colour.z),
                       text_colour.w);
    metrics->grey_colour = ImGui::ColorConvertFloat4ToU32(grey_colour);

    metrics->disabled_colour = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_TextDisabled]);
    metrics->text_colour = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int ReportCharCallback(ImGuiInputTextCallbackData *data) {
    ImWchar *ch = (ImWchar *)data->UserData;

    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCharFilter:
        *ch = data->EventChar;

        // Block the input. The text input box is wide enough for the
        // cursor, but no more; if the string is allowed to grow, the
        // cursor will become invisible.
        //
        // (InputText is actually supplied an empty string each time,
        // but that's not enough, since it maintains its own buffer
        // internally.)
        return 1;

    default:
        *ch = 0;
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoHexPart(size_t num_skip_columns, size_t begin_offset, size_t end_offset) {
    ImGui::PushID("hex");
    ImGui::PushID((void *)begin_offset);

    const char *hex_chars = this->GetHexChars();

    float x = m_metrics.hex_left_x;
    ImGui::SameLine(x);
    ImVec2 screen_pos = ImGui::GetCursorScreenPos();

    {
        TextColourHandler tch;

        if (num_skip_columns > 0) {
            ImGui::SameLine(x + num_skip_columns * m_metrics.hex_column_width);
            x += num_skip_columns * m_metrics.hex_column_width;
        }

        const HexEditorByte *byte = m_bytes;
        for (size_t offset = begin_offset; offset != end_offset; ++offset, ++byte) {
            bool editing;

            if (offset == m_offset) {
                m_draw_list->AddRectFilled(screen_pos, ImVec2(screen_pos.x + m_metrics.glyph_width * 2.f, screen_pos.y + m_metrics.line_height), m_highlight_colour);
            }

            if (!byte->got_value || (byte->colour == 0 && !byte->can_write)) {
                tch.BeginColour(m_metrics.disabled_colour);
            } else if (byte->colour == 0 && (!byte->got_value || (byte->value == 0 && this->options.grey_00s))) {
                tch.BeginColour(m_metrics.grey_colour);
            } else if (byte->colour == 0) {
                tch.BeginColour(m_metrics.text_colour);
            } else {
                tch.BeginColour(byte->colour);
            }

            if (offset == m_offset && m_hex) {
                bool commit = false;
                editing = true;

                if (m_editing_high_nybble) {
                    ImGui::SameLine(x);
                } else {
                    ImGui::SameLine(x + m_metrics.glyph_width);
                }

                const bool editable = byte->got_value && byte->can_write;

                // Quickly check for a couple of keys it's a pain to
                // handle.
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
                    commit = true;
                    editing = false;
                } else {
                    ImWchar ch;

                    this->GetChar(&ch, &editing, "hex_input");

                    if (!editable) {
                        editing = false;
                    }

                    if (editing) {
                        if (ch != 0) {
                            int nybble = -1;
                            if (ch >= '0' && ch <= '9') {
                                nybble = ch - '0';
                            } else if (ch >= 'a' && ch <= 'f') {
                                nybble = ch - 'a' + 10;
                            } else if (ch >= 'A' && ch <= 'F') {
                                nybble = ch - 'A' + 10;
                            }

                            if (nybble >= 0) {
                                if (m_editing_high_nybble) {
                                    m_edit_value &= 0x0f;
                                    m_edit_value |= (uint8_t)nybble << 4;
                                    m_editing_high_nybble = false;
                                } else {
                                    m_edit_value &= 0xf0;
                                    m_edit_value |= (uint8_t)nybble;
                                    commit = true;
                                }
                            }

                            m_handler->DebugPrint("%zu - got char: %u, 0x%04X, '%c'\n", m_num_calls, ch, ch, ch >= 32 && ch < 127 ? (char)ch : '?');
                        }
                    }
                }

                if (commit) {
                    if (editable) {
                        if (m_got_edit_value) {
                            //IM_ASSERT(m_edit_value>=0&&m_edit_value<256);
                            m_handler->WriteByte(m_offset, (uint8_t)m_edit_value);
                        }
                    }

                    this->SetNewOffset(m_offset, 1, true);
                    m_hex = true;
                }
            } else {
                editing = false;
            }

            if (editing) {
                ImGui::SameLine(x);
                if (m_editing_high_nybble) {
                    ImGui::Text("%c", hex_chars[m_edit_value >> 4]);
                    ImGui::SameLine(x + m_metrics.glyph_width);
                    ImGui::TextDisabled("%c", hex_chars[m_edit_value & 0xf]);
                } else {
                    ImGui::TextDisabled("%c", hex_chars[m_edit_value >> 4]);
                    ImGui::SameLine(x + m_metrics.glyph_width);
                    ImGui::Text("%c", hex_chars[m_edit_value & 0xf]);
                }
            } else {
                char text[4];

                if (byte->got_value) {
                    text[0] = hex_chars[byte->value >> 4];
                    text[1] = hex_chars[byte->value & 0xf];
                } else {
                    text[0] = '-';
                    text[1] = '-';
                }

                text[2] = ' ';
                text[3] = 0;

                ImGui::SameLine(x);

                ImGui::TextUnformatted(text, text + sizeof text - 1);

                if (ImGui::IsMouseClicked(0)) {
                    if (ImGui::IsItemHovered()) {
                        m_handler->DebugPrint("hover item: %zx\n", offset);
                        this->SetNewOffset(offset, 0, false);
                        m_hex = true;
                        //new_offset=offset;
                        //got_new_offset=true;
                    }
                }
            }

            this->OpenContextPopup(true, offset);

            x += m_metrics.hex_column_width;
            screen_pos.x += m_metrics.hex_column_width;
        }
    }

    ImGui::PopID();
    ImGui::PopID();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string UNPRINTABLE_CHAR = ".";

void HexEditor::DoAsciiPart(size_t begin_offset, size_t end_offset) {
    ImGui::PushID((void *)begin_offset);
    ImGui::PushID("ascii");

    float x = m_metrics.ascii_left_x;
    ImGui::SameLine(x);
    ImVec2 screen_pos = ImGui::GetCursorScreenPos();

    const std::vector<std::string> *char_from_byte = m_handler->GetCharFromByteTranslationTable();

    ImGui::PushItemWidth(m_metrics.glyph_width);

    {
        TextColourHandler tch;

        // only ever 1 byte, so should benefit from small string optimisation.
        std::string display_char_buffer;

        const HexEditorByte *byte = m_bytes;
        for (size_t offset = begin_offset; offset != end_offset; ++offset, ++byte) {
            bool editable = byte->got_value && byte->can_write;

            const std::string *display_char = nullptr;

            if (byte->got_value) {
                if (char_from_byte) {
                    if (byte->value < char_from_byte->size()) {
                        display_char = &(*char_from_byte)[byte->value];
                        if (display_char->empty()) {
                            display_char = nullptr;
                        }
                    }
                } else {
                    if (byte->value >= 32 && byte->value < 127) {
                        display_char_buffer.assign(1, (char)byte->value);
                        display_char = &display_char_buffer;
                    }
                }
            }

            bool wasprint;
            if (display_char) {
                wasprint = true;
            } else {
                wasprint = false;
                display_char = &UNPRINTABLE_CHAR;
            }

            if (offset == m_offset) {
                m_draw_list->AddRectFilled(screen_pos, ImVec2(screen_pos.x + m_metrics.glyph_width, screen_pos.y + m_metrics.line_height), m_highlight_colour);
            }

            bool editing;
            if (offset == m_offset && !m_hex && editable) {
                editing = true;

                ImWchar ch;
                ImGui::SameLine(x);
                this->GetChar(&ch, &editing, "ascii_input");

                int new_value = -1;
                if (ch != 0) {
                    new_value = m_handler->GetByteForWchar(ch);
                }

                if (editing) {
                    if (new_value >= 0 && new_value < 256) {
                        m_handler->WriteByte(m_offset, (uint8_t)new_value);

                        this->SetNewOffset(m_offset, 1, true);
                        m_hex = false;
                    }
                }
            } else {
                editing = false;
            }

            ImGui::SameLine(x);
            if (!byte->got_value || (!byte->can_write && byte->colour == 0)) {
                tch.BeginColour(m_metrics.disabled_colour);
            } else if (!wasprint && this->options.grey_nonprintables && byte->colour == 0) {
                tch.BeginColour(m_metrics.grey_colour);
            } else if (byte->colour == 0) {
                tch.BeginColour(m_metrics.text_colour);
            } else {
                tch.BeginColour(byte->colour);
            }

            ImGui::TextUnformatted(display_char->c_str());

            if (!editing) {
                if (ImGui::IsMouseClicked(0)) {
                    if (ImGui::IsItemHovered()) {
                        m_handler->DebugPrint("%zu - clicked on offset 0x%zx\n", m_num_calls, offset);
                        this->SetNewOffset(offset, 0, false);
                        m_hex = false;
                    }
                }
            }

            this->OpenContextPopup(false, offset);

            x += m_metrics.glyph_width;
            screen_pos.x += m_metrics.glyph_width;
        }
    }

    ImGui::PopItemWidth();

    ImGui::PopID();
    ImGui::PopID();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The TextInput is just a fudgy way of getting some keyboard input,
// so its flags are a random mishmash of whatever made this work. I
// tried to make this a bit simpler than the imgui_club hex editor in
// terms of fiddling with the input buffer and so on, but whether it's
// any clearer overall... I wouldn't like to guess.
static const ImGuiInputTextFlags INPUT_TEXT_FLAGS = ( //ImGuiInputTextFlags_CharsHexadecimal|
    //ImGuiInputTextFlags_EnterReturnsTrue|
    ImGuiInputTextFlags_AutoSelectAll |
    ImGuiInputTextFlags_NoHorizontalScroll |
    ImGuiInputTextFlags_AlwaysOverwrite |
    //ImGuiInputTextFlags_Multiline|
    //ImGuiInputTextFlags_AllowTabInput|
    //ImGuiInputTextFlags_CallbackAlways|
    ImGuiInputTextFlags_CallbackCharFilter |
    //ImGuiInputTextFlags_CallbackCompletion|
    0);

void HexEditor::GetChar(uint16_t *ch, bool *editing, const char *id) {
    ImGui::PushID(id);

    bool take_focus_this_frame;
    if (m_input_take_focus_next_frame) {
        ImGui::SetKeyboardFocusHere();
        ImGui::SetNextFrameWantCaptureKeyboard(true);

        m_handler->DebugPrint("Taking focus.\n");

        take_focus_this_frame = true;
        m_input_take_focus_next_frame = false;
    } else {
        take_focus_this_frame = false;
    }

    ImGui::PushItemWidth(1.f);
    //ImGui::SameLine();

    *ch = 0;
    char text[2] = {};
    ImGui::InputText("", text, sizeof text, INPUT_TEXT_FLAGS, &ReportCharCallback, ch);

    if (!take_focus_this_frame && !m_set_new_offset && !ImGui::IsItemActive()) {
        m_handler->DebugPrint("%zu - m_offset=0x%zx: InputText inactive. Invalidating offset.\n", m_num_calls, m_offset);
        this->SetNewOffset(INVALID_OFFSET, 0, false);
        *editing = false;
    }

    ImGui::PopItemWidth();

    ImGui::PopID();

    m_was_TextInput_visible = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::UpdateOffsetByKey(ImGuiKey key, int delta, int times) {
    if (!m_set_new_offset) {
        if (ImGui::IsKeyPressed(key)) {
            for (int i = 0; i < times; ++i) {
                this->SetNewOffset(m_set_new_offset ? m_new_offset : m_offset, delta, false);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::SetNewOffset(size_t base, int delta, bool invalidate_on_failure) {
    bool failed;

    m_set_new_offset_via_SetOffset = false;

    if (delta < 0) {
        if (base >= (size_t)-delta) {
            m_new_offset = base - (size_t)-delta;
            m_set_new_offset = true;
            failed = false;
        } else {
            failed = true;
        }
    } else if (delta > 0) {
        if (base + (size_t)delta < m_handler->GetSize()) {
            m_new_offset = base + (size_t)delta;
            m_set_new_offset = true;
            failed = false;
        } else {
            failed = true;
        }
    } else {
        m_new_offset = base;
        m_set_new_offset = true;
        failed = false;
    }

    if (failed) {
        if (invalidate_on_failure) {
            m_new_offset = INVALID_OFFSET;
            m_set_new_offset = true;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::OpenContextPopup(bool hex, size_t offset) {
    // The popup ID is relative to the current ID stack, so
    // opens/closes have to be at the same level. This makes this a
    // little fiddly...
    //
    // See
    // https://github.com/ocornut/imgui/blob/d57fc7fb970524dbaadb9622704ba666053840c0/imgui.cpp#L5117.

    if (hex != m_context_hex || m_context_offset != offset) {
        // Check for opening popup here.
        if (ImGui::IsMouseClicked(1)) {
            if (ImGui::IsItemHovered()) {
                m_context_offset = offset;
                m_context_hex = hex;

                ImGui::OpenPopup(CONTEXT_POPUP_NAME);
            }
        }
    }

    if (hex == m_context_hex && m_context_offset == offset) {
        if (ImGui::BeginPopup(CONTEXT_POPUP_NAME)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                ImVec2(m_style_frame_padding_x,
                                       m_style_frame_padding_y));

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                ImVec2(m_style_item_spacing_x,
                                       m_style_item_spacing_y));

            ImGui::PushStyleColor(ImGuiCol_Text, m_metrics.text_colour);

            this->DoContextPopup();

            ImGui::PopStyleColor(1);
            ImGui::PopStyleVar(2);

            ImGui::EndPopup();
        } else {
            m_context_offset = INVALID_OFFSET;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoOptionsPopup() {
    ImGui::Checkbox("Column headers", &this->options.headers);
    ImGui::Checkbox("Show hex", &this->options.hex);
    ImGui::Checkbox("Show ASCII", &this->options.ascii);
    ImGui::Checkbox("Grey 00s", &this->options.grey_00s);
    ImGui::Checkbox("Grey non-printables", &this->options.grey_nonprintables);
    ImGui::Checkbox("Upper case", &this->options.upper_case);

    if (ImGui::InputText("Columns",
                         m_new_num_columns_input_buffer,
                         sizeof m_new_num_columns_input_buffer,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        char *ep;
        m_new_num_columns = strtoull(m_new_num_columns_input_buffer, &ep, 0);
        if (*ep == 0 || isspace(*ep)) {
            // Feels like there ought to be an upper limit. 1024 was chosen
            // pretty much at random.
            if (m_new_num_columns > 0 && m_new_num_columns <= 1024) {
                m_set_new_num_columns = true;
            }
        }
    }

    m_handler->DoOptionsPopupExtraGui();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static int64_t GetValue(const int *bytes,int n,int index,int dindex) {
//    for(int i=0;i<n;++i) {
//        if(bytes[i]<0) {
//            return -1;
//        }
//    }
//
//    int64_t value=0;
//
//    while(n>0) {
//        value<<=8;
//        value|=(uint8_t)bytes[index];
//
//        index+=dindex;
//        --n;
//    }
//
//    return value;
//}

static void ShowValue(int32_t value, const char *prefix, unsigned num_bytes) {
    char binary[33];
    {
        uint32_t mask = 1u << (num_bytes * 8 - 1);
        size_t i = 0;
        IM_ASSERT(num_bytes * 8u + 1u <= sizeof binary);
        while (mask != 0) {
            IM_ASSERT(i < sizeof binary);
            binary[i] = (uint32_t)value & mask ? '1' : '0';
            mask >>= 1;
            ++i;
        }

        IM_ASSERT(i < sizeof binary);
        binary[i++] = 0;
    }

    uint32_t uvalue = (uint32_t)value;
    if (num_bytes < 4) {
        uvalue &= (1u << num_bytes * 8u) - 1u;
    }

    int prefix_len = (int)strlen(prefix);

    ImGui::Text("%s: %" PRId32 " %" PRIu32 "u 0x%0*" PRIx32,
                prefix,
                value,
                uvalue,
                num_bytes * 2, uvalue);
    ImGui::Text("%*s  %%%s", prefix_len, "", binary);
}

void HexEditor::DoContextPopup() {
    m_handler->DoContextPopupExtraGui(m_hex, m_context_offset);

    HexEditorByte bytes[4];
    for (size_t i = 0, offset = m_context_offset; i < IM_ARRAYSIZE(bytes); ++i, ++offset) {
        if (offset < m_handler->GetSize()) {
            m_handler->ReadByte(&bytes[i], offset);
        }
    }

    int16_t wl = 0, wb = 0;
    bool wok = false;
    int32_t ll = 0, lb = 0;
    bool lok = false;

    if (bytes[0].got_value && bytes[1].got_value) {
        wl = (int16_t)(bytes[0].value | bytes[1].value << 8);
        wb = (int16_t)(bytes[0].value << 8 | bytes[1].value);
        wok = true;

        if (bytes[2].got_value && bytes[3].got_value) {
            ll = bytes[0].value | bytes[1].value << 8 | bytes[2].value << 16 | bytes[3].value << 24;
            lb = bytes[0].value << 24 | bytes[1].value << 16 | bytes[2].value << 8 | bytes[3].value;
            lok = true;
        }
    }

    if (bytes[0].got_value && bytes[0].value >= 32 && bytes[0].value < 127) {
        ImGui::Text("c: '%c'", (char)bytes[0].value);
    }

    ShowValue((int8_t)bytes[0].value, "b", 1);

    if (wok) {
        ShowValue(wl, "wL", 2);
        ShowValue(wb, "wB", 2);
    }

    if (lok) {
        ShowValue(ll, "lL", 4);
        ShowValue(lb, "lB", 4);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *HexEditor::GetHexChars() const {
    if (this->options.upper_case) {
        return HEX_CHARS_UC;
    } else {
        return HEX_CHARS_LC;
    }
}
