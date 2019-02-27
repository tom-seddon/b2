#include "dear_imgui_hex_editor.h"
#include <algorithm>
#include <ctype.h>
#include <inttypes.h>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER

#pragma warning(push)
#pragma warning(disable:4458)//declaration of 'identifier' hides class member
#pragma warning(disable:4267)//'var' : conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable:4305)//'identifier' : truncation from 'type1' to 'type2'
#pragma warning(disable:4100)//'identifier' : unreferenced formal parameter
#pragma warning(disable:4800)//'type' : forcing value to bool 'true' or 'false' (performance warning)

#elif defined __GNUC__

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif

#endif

#include <imgui.h>

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined __GNUC__
#pragma GCC diagnostic pop
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char HEX_CHARS_UC[]="0123456789ABCDEF";
static const char HEX_CHARS_LC[]="0123456789abcdef";

static const char OPTIONS_POPUP_NAME[]="hex_editor_options";
static const char CONTEXT_POPUP_NAME[]="hex_editor_context";

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

void HexEditorHandler::DoOptionsPopupExtraGui() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandler::DoContextPopupExtraGui(bool hex,size_t offset) {
    (void)hex,(void)offset;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandler::DebugPrint(const char *fmt,...) {
    (void)fmt;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorHandlerWithBufferData::HexEditorHandlerWithBufferData(void *buffer,size_t buffer_size) {
    this->Construct(buffer,buffer,buffer_size);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorHandlerWithBufferData::HexEditorHandlerWithBufferData(const void *buffer,size_t buffer_size) {
    this->Construct(buffer,nullptr,buffer_size);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::ReadByte(HexEditorByte *byte,size_t offset) {
    IM_ASSERT(offset<m_buffer_size);

    byte->got_value=true;
    byte->value=m_read_buffer[offset];
    byte->can_write=!!m_write_buffer;
    byte->colour=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::WriteByte(size_t offset,uint8_t value) {
    IM_ASSERT(m_write_buffer);
    IM_ASSERT(offset<m_buffer_size);

    m_write_buffer[offset]=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t HexEditorHandlerWithBufferData::GetSize() {
    return m_buffer_size;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uintptr_t HexEditorHandlerWithBufferData::GetBaseAddress() {
    if(m_show_address) {
        return (uintptr_t)m_read_buffer;
    } else {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::DoOptionsPopupExtraGui() {
    ImGui::Checkbox("Show address",&m_show_address);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorHandlerWithBufferData::Construct(const void *read_buffer,void *write_buffer,size_t buffer_size) {
    m_read_buffer=(const uint8_t *)read_buffer;
    m_write_buffer=(uint8_t *)write_buffer;
    m_buffer_size=buffer_size;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditor::HexEditor(HexEditorHandler *handler):
    m_handler(handler)
{
    m_offset=0;
    m_hex=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditor::~HexEditor() {
    delete[] m_bytes;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoImGui() {
    const ImGuiStyle &style=ImGui::GetStyle();

    this->GetMetrics(&m_metrics,style);
    m_highlight_colour=ImGui::GetColorU32(ImGuiCol_TextSelectedBg);

    const float footer_height=style.ItemSpacing.y+ImGui::GetFrameHeightWithSpacing();

    const size_t data_size=m_handler->GetSize();

    m_set_new_offset=false;
    m_new_offset=INVALID_OFFSET;
    m_was_TextInput_visible=false;

    if(this->num_columns>m_num_bytes) {
        delete[] m_bytes;

        m_num_bytes=this->num_columns;
        m_bytes=new HexEditorByte[m_num_bytes];
    }

    // Main scroll area.
    int first_visible_row;
    int num_visible_rows;
    {
        ImGui::BeginChild("##scrolling",ImVec2(0,-footer_height),false,ImGuiWindowFlags_NoMove);

        num_visible_rows=(std::max)(1,(int)(ImGui::GetContentRegionAvail().y/m_metrics.line_height));

        if(m_next_frame_scroll_y>=0.f) {
            ImGui::SetScrollY(m_next_frame_scroll_y);
            m_next_frame_scroll_y=-1.f;
        }

        {
            m_draw_list=ImGui::GetWindowDrawList();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,0));
            {
                size_t num_lines=(m_handler->GetSize()+this->num_columns-1)/this->num_columns;
                IM_ASSERT(num_lines<=INT_MAX);
                ImGuiListClipper clipper((int)num_lines,m_metrics.line_height);

                first_visible_row=clipper.DisplayStart;

                uintptr_t base_address=m_handler->GetBaseAddress();

                char fmt[100];
                snprintf(fmt,sizeof fmt,"%%0*%s:",this->upper_case?PRIXPTR:PRIxPTR);

                for(int line_index=clipper.DisplayStart;line_index<clipper.DisplayEnd;++line_index) {
                    size_t line_begin_offset=(size_t)line_index*this->num_columns;

                    ImGui::Text(fmt,m_metrics.num_addr_digits,base_address+line_begin_offset);

                    size_t line_end_offset=(std::min)(line_begin_offset+this->num_columns,data_size);

                    {
                        HexEditorByte *byte=m_bytes;
                        for(size_t offset=line_begin_offset;offset!=line_end_offset;++offset) {
                            m_handler->ReadByte(byte++,offset);
                        }
                        IM_ASSERT(byte<=m_bytes+m_num_bytes);
                    }

                    ImGui::PushID(line_index);
                    this->DoHexPart(line_begin_offset,line_end_offset);

                    if(this->ascii) {
                        this->DoAsciiPart(line_begin_offset,line_end_offset);
                    }
                    ImGui::PopID();
                }

                clipper.End();
            }

            ImGui::PopStyleVar(2);

            m_draw_list=nullptr;
        }

        ImGui::EndChild();
    }


    // Options and stuff.
    if(ImGui::Button("Options")) {
        ImGui::OpenPopup(OPTIONS_POPUP_NAME);
    }

    if(ImGui::BeginPopup(OPTIONS_POPUP_NAME)) {
        this->DoOptionsPopup();
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    {
        ImGui::PushID("address");
        ImGui::PushItemWidth((m_metrics.num_addr_digits+2)*m_metrics.glyph_width);
        int flags=ImGuiInputTextFlags_CharsHexadecimal|ImGuiInputTextFlags_EnterReturnsTrue;
        if(ImGui::InputText("",m_new_offset_input_buffer,sizeof m_new_offset_input_buffer,flags)) {
            char *ep;
            unsigned long long value=strtoull(m_new_offset_input_buffer,&ep,16);
            if(*ep==0||isspace(*ep)) {
                this->SetNewOffset((size_t)value,0,false);
                strcpy(m_new_offset_input_buffer,"");
            }
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    if(m_offset!=INVALID_OFFSET) {
        this->UpdateOffsetByKey(ImGuiKey_UpArrow,-(int)this->num_columns,1);
        this->UpdateOffsetByKey(ImGuiKey_DownArrow,(int)this->num_columns,1);
        this->UpdateOffsetByKey(ImGuiKey_LeftArrow,-1,1);
        this->UpdateOffsetByKey(ImGuiKey_RightArrow,1,1);
        this->UpdateOffsetByKey(ImGuiKey_PageUp,-(int)this->num_columns,num_visible_rows);
        this->UpdateOffsetByKey(ImGuiKey_PageDown,(int)this->num_columns,num_visible_rows);

        if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))) {
            m_hex=!m_hex;
            m_take_focus_next_frame=true;
        } else if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
            this->SetNewOffset(INVALID_OFFSET,0,true);
        }
    }

    if(m_set_new_offset) {
        m_offset=m_new_offset;
        m_take_focus_next_frame=true;

        m_handler->DebugPrint("%zu - set new offset: now 0x%zx\n",m_num_calls,m_offset);
        //m_handler->DebugPrint("%zu - clipper = %d -> %d\n",m_num_calls,clipper_display_start,clipper_display_end);

        if(m_hex) {
            m_editing_high_nybble=true;
        }

        if(m_offset==INVALID_OFFSET) {
            m_got_edit_value=false;
        } else {
            HexEditorByte byte;
            m_handler->ReadByte(&byte,m_offset);

            if(byte.got_value&&byte.can_write) {
                m_got_edit_value=true;
                m_edit_value=byte.value;
            } else {
                m_got_edit_value=false;
            }

            size_t row=m_offset/this->num_columns;

            if(row<first_visible_row) {
                m_next_frame_scroll_y=row*m_metrics.line_height;
            } else if(row>=first_visible_row+num_visible_rows) {
                m_next_frame_scroll_y=(row-((size_t)num_visible_rows-1u))*m_metrics.line_height;
            }

            //m_handler->DebugPrint("row=%zu, clipper start=%d, clipper end=%d, scroll next frame=%f\n",row,clipper_display_start,clipper_display_end,m_next_frame_scroll_y);
        }
    } else {
        if(m_offset!=INVALID_OFFSET) {
            if(!m_was_TextInput_visible) {
                // sort out TextInput again when it next becomes visible.
                m_take_focus_next_frame=true;
            }
        }
    }

    ++m_num_calls;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static float GetHalf(float a,float b) {
    float a_linear=a*a;
    float b_linear=b*b;
    float half_linear=(a_linear+b_linear)*0.5f;
    return sqrtf(half_linear);
}

void HexEditor::GetMetrics(Metrics *metrics,const ImGuiStyle &style) {
    (void)style;

    metrics->num_addr_digits=0;
    {
        uintptr_t max=m_handler->GetBaseAddress()+(m_handler->GetSize()-1);
        while(max!=0) {
            ++metrics->num_addr_digits;
            max>>=4;
        }
    }

    metrics->line_height=ImGui::GetTextLineHeight();
    metrics->glyph_width=ImGui::CalcTextSize("F").x+1;

    metrics->hex_left_x=(metrics->num_addr_digits+2)*metrics->glyph_width;
    metrics->hex_column_width=2.5f*metrics->glyph_width;

    metrics->ascii_left_x=metrics->hex_left_x+this->num_columns*metrics->hex_column_width+2.f*metrics->glyph_width;

    const ImVec4 &text_colour=style.Colors[ImGuiCol_Text];
    const ImVec4 &text_disabled_colour=style.Colors[ImGuiCol_TextDisabled];
    ImVec4 grey_colour(GetHalf(text_colour.x,text_disabled_colour.x),
                       GetHalf(text_colour.y,text_disabled_colour.y),
                       GetHalf(text_colour.z,text_disabled_colour.z),
                       text_colour.w);
    metrics->grey_colour=ImGui::ColorConvertFloat4ToU32(grey_colour);

    metrics->disabled_colour=ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_TextDisabled]);
    metrics->text_colour=ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int ReportCharCallback(ImGuiTextEditCallbackData *data) {
    ImWchar *ch=(ImWchar *)data->UserData;

    switch(data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCharFilter:
        *ch=data->EventChar;

        // Block the input. The text input box is wide enough for the
        // cursor, but no more; if the string is allowed to grow, the
        // cursor will become invisible.
        //
        // (InputText is actually supplied an empty string each time,
        // but that's not enough, since it maintains its own buffer
        // internally.)
        return 1;

    default:
        *ch=0;
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TextColourHandler {
    uint32_t colour=0;
    bool pushed=false;

    TextColourHandler() {
    }

    //PushStyleColor(ImGuiCol_Text, GImGui->Style.Colors[ImGuiCol_TextDisabled]);

    ~TextColourHandler() {
        this->Pop();
    }

    void BeginColour(uint32_t new_colour) {
        if(new_colour!=this->colour) {
            this->Pop();

            ImGui::PushStyleColor(ImGuiCol_Text,new_colour);
            this->colour=new_colour;
            this->pushed=true;
        }
    }

    void Pop() {
        if(this->pushed) {
            ImGui::PopStyleColor(1);
            this->pushed=false;
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoHexPart(size_t begin_offset,size_t end_offset) {
    ImGui::PushID("hex");
    ImGui::PushID((void *)begin_offset);

    const char *hex_chars;
    if(this->upper_case) {
        hex_chars=HEX_CHARS_UC;
    } else {
        hex_chars=HEX_CHARS_LC;
    }

    float x=m_metrics.hex_left_x;
    ImGui::SameLine(x);
    ImVec2 screen_pos=ImGui::GetCursorScreenPos();

    {
        TextColourHandler tch;

        const HexEditorByte *byte=m_bytes;
        for(size_t offset=begin_offset;offset!=end_offset;++offset,++byte) {
            bool editing;
            bool editable=byte->got_value&&byte->can_write;

            if(offset==m_offset) {
                m_draw_list->AddRectFilled(screen_pos,ImVec2(screen_pos.x+m_metrics.glyph_width*2.f,screen_pos.y+m_metrics.line_height),m_highlight_colour);
            }

            if(offset==m_offset&&m_hex) {
                bool commit=false;
                editing=true;

                if(m_editing_high_nybble) {
                    ImGui::SameLine(x);
                } else {
                    ImGui::SameLine(x+m_metrics.glyph_width);
                }

                // Quickly check for a couple of keys it's a pain to
                // handle.
                if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
                    commit=true;
                    editing=false;
                } else if(editable) {
                    ImWchar ch;
                    this->GetChar(&ch,&editing,"hex_input");

                    if(editing) {
                        if(ch!=0) {
                            int nybble=-1;
                            if(ch>='0'&&ch<='9') {
                                nybble=ch-'0';
                            } else if(ch>='a'&&ch<='f') {
                                nybble=ch-'a'+10;
                            } else if(ch>='A'&&ch<='F') {
                                nybble=ch-'A'+10;
                            }

                            if(nybble>=0) {
                                if(m_editing_high_nybble) {
                                    m_edit_value&=0x0f;
                                    m_edit_value|=(uint8_t)nybble<<4;
                                    m_editing_high_nybble=false;
                                } else {
                                    m_edit_value&=0xf0;
                                    m_edit_value|=(uint8_t)nybble;
                                    commit=true;
                                }
                            }

                            m_handler->DebugPrint("%zu - got char: %u, 0x%04X, '%c'\n",m_num_calls,ch,ch,ch>=32&&ch<127?(char)ch:'?');
                        }
                    }
                } else {
                    editing=false;
                }

                if(commit) {
                    if(editable) {
                        IM_ASSERT(m_got_edit_value);
                        //IM_ASSERT(m_edit_value>=0&&m_edit_value<256);
                        m_handler->WriteByte(m_offset,(uint8_t)m_edit_value);
                    }

                    this->SetNewOffset(m_offset,1,true);
                    m_hex=true;
                }
            } else {
                editing=false;
            }

            if(editing) {
                ImGui::SameLine(x);
                if(m_editing_high_nybble) {
                    ImGui::Text("%c",hex_chars[m_edit_value>>4]);
                    ImGui::SameLine(x+m_metrics.glyph_width);
                    ImGui::TextDisabled("%c",hex_chars[m_edit_value&0xf]);
                } else {
                    ImGui::TextDisabled("%c",hex_chars[m_edit_value>>4]);
                    ImGui::SameLine(x+m_metrics.glyph_width);
                    ImGui::Text("%c",hex_chars[m_edit_value&0xf]);
                }
            } else {
                char text[4];

                if(byte->got_value) {
                    text[0]=hex_chars[byte->value>>4];
                    text[1]=hex_chars[byte->value&0xf];
                } else {
                    text[0]='-';
                    text[1]='-';
                }

                text[2]=' ';
                text[3]=0;

                ImGui::SameLine(x);

                if(!byte->got_value||(byte->colour==0&&!byte->can_write)) {
                    tch.BeginColour(m_metrics.disabled_colour);
                } else if(byte->colour==0&&(!byte->got_value||(byte->value==0&&this->grey_00s))) {
                    tch.BeginColour(m_metrics.grey_colour);
                } else if(byte->colour==0) {
                    tch.BeginColour(m_metrics.text_colour);
                } else {
                    tch.BeginColour(byte->colour);
                }

                    ImGui::TextUnformatted(text,text+sizeof text-1);

                if(ImGui::IsMouseClicked(0)) {
                    if(ImGui::IsItemHovered()) {
                        //m_handler->DebugPrint("hover item: %zx\n",offset);
                        this->SetNewOffset(offset,0,false);
                        m_hex=true;
                        //new_offset=offset;
                        //got_new_offset=true;
                    }
                }
            }

            this->OpenContextPopup(true,offset);

            x+=m_metrics.hex_column_width;
            screen_pos.x+=m_metrics.hex_column_width;
        }
    }

    ImGui::PopID();
    ImGui::PopID();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoAsciiPart(size_t begin_offset,size_t end_offset) {
    ImGui::PushID((void *)begin_offset);
    ImGui::PushID("ascii");

    float x=m_metrics.ascii_left_x;
    ImGui::SameLine(x);
    ImVec2 screen_pos=ImGui::GetCursorScreenPos();

    ImGui::PushItemWidth(m_metrics.glyph_width);

    {
        TextColourHandler tch;

        const HexEditorByte *byte=m_bytes;
        for(size_t offset=begin_offset;offset!=end_offset;++offset,++byte) {
            bool editable=byte->got_value&&byte->can_write;

            bool wasprint;
            char display_char[2];

            if(byte->got_value&&byte->value>=32&&byte->value<127) {
                wasprint=true;
                display_char[0]=(char)byte->value;
            } else {
                wasprint=false;
                display_char[0]='.';
            }

            display_char[1]=0;

            if(offset==m_offset) {
                m_draw_list->AddRectFilled(screen_pos,ImVec2(screen_pos.x+m_metrics.glyph_width,screen_pos.y+m_metrics.line_height),m_highlight_colour);
            }

            bool editing;
            if(offset==m_offset&&!m_hex&&editable) {
                editing=true;

                ImWchar ch;
                ImGui::SameLine(x);
                this->GetChar(&ch,&editing,"ascii_input");

                if(editing) {
                    if(ch>=32&&ch<127) {
                        m_handler->WriteByte(m_offset,(uint8_t)ch);

                        this->SetNewOffset(m_offset,1,true);
                        m_hex=false;
                    }
                }
            } else {
                editing=false;
            }

            ImGui::SameLine(x);
            if(!byte->got_value||(!byte->can_write&&byte->colour==0)) {
                tch.BeginColour(m_metrics.disabled_colour);
            } else if(!wasprint&&this->grey_nonprintables&&byte->colour==0) {
                tch.BeginColour(m_metrics.grey_colour);
            } else if(byte->colour==0) {
                tch.BeginColour(m_metrics.text_colour);
            } else {
                tch.BeginColour(byte->colour);
            }

            ImGui::TextUnformatted(display_char);

            if(!editing) {
                if(ImGui::IsItemHovered()) {
                    if(ImGui::IsMouseClicked(0)) {
                        m_handler->DebugPrint("%zu - clicked on offset 0x%zx\n",m_num_calls,offset);
                        this->SetNewOffset(offset,0,false);
                        m_hex=false;
                    }
                }
            }

            this->OpenContextPopup(false,offset);

            x+=m_metrics.glyph_width;
            screen_pos.x+=m_metrics.glyph_width;
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
static const ImGuiInputTextFlags INPUT_TEXT_FLAGS=(//ImGuiInputTextFlags_CharsHexadecimal|
                                                   //ImGuiInputTextFlags_EnterReturnsTrue|
                                                   ImGuiInputTextFlags_AutoSelectAll|
                                                   ImGuiInputTextFlags_NoHorizontalScroll|
                                                   ImGuiInputTextFlags_AlwaysInsertMode|
                                                   //ImGuiInputTextFlags_Multiline|
                                                   //ImGuiInputTextFlags_AllowTabInput|
                                                   //ImGuiInputTextFlags_CallbackAlways|
                                                   ImGuiInputTextFlags_CallbackCharFilter|
                                                   //ImGuiInputTextFlags_CallbackCompletion|
                                                   0);

void HexEditor::GetChar(uint16_t *ch,bool *editing,const char *id) {
    ImGui::PushID(id);

    bool take_focus_this_frame;
    if(m_take_focus_next_frame) {
        ImGui::SetKeyboardFocusHere();
        ImGui::CaptureKeyboardFromApp(true);

        take_focus_this_frame=true;
        m_take_focus_next_frame=false;
    } else {
        take_focus_this_frame=false;
    }

    ImGui::PushItemWidth(1.f);
    //ImGui::SameLine();

    *ch=0;
    char text[2]={};
    ImGui::InputText("",text,sizeof text,INPUT_TEXT_FLAGS,&ReportCharCallback,ch);

    if(!take_focus_this_frame&&!m_set_new_offset&&!ImGui::IsItemActive()) {
        m_handler->DebugPrint("%zu - m_offset=0x%zx: InputText inactive. Invalidating offset.\n",m_num_calls,m_offset);
        this->SetNewOffset(INVALID_OFFSET,0,false);
        *editing=false;
    }

    ImGui::PopItemWidth();

    ImGui::PopID();

    m_was_TextInput_visible=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::UpdateOffsetByKey(int key,int delta,int times) {
    if(!m_set_new_offset) {
        if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(key))) {
            for(int i=0;i<times;++i) {
                this->SetNewOffset(m_set_new_offset?m_new_offset:m_offset,delta,false);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::SetNewOffset(size_t base,int delta,bool invalidate_on_failure) {
    bool failed;

    if(delta<0) {
        if(base>=(size_t)-delta) {
            m_new_offset=base-(size_t)-delta;
            m_set_new_offset=true;
            failed=false;
        } else {
            failed=true;
        }
    } else if(delta>0) {
        if(base+(size_t)delta<m_handler->GetSize()) {
            m_new_offset=base+(size_t)delta;
            m_set_new_offset=true;
            failed=false;
        } else {
            failed=true;
        }
    } else {
        m_new_offset=base;
        m_set_new_offset=true;
        failed=false;
    }

    if(failed) {
        if(invalidate_on_failure) {
            m_new_offset=INVALID_OFFSET;
            m_set_new_offset=true;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::OpenContextPopup(bool hex,size_t offset) {
    // The popup ID is relative to the current ID stack, so
    // opens/closes have to be at the same level. This makes this a
    // little fiddly...
    //
    // See
    // https://github.com/ocornut/imgui/blob/d57fc7fb970524dbaadb9622704ba666053840c0/imgui.cpp#L5117.

    if(hex!=m_context_hex||m_context_offset!=offset) {
        // Check for opening popup here.
        if(ImGui::IsMouseClicked(1)) {
            if(ImGui::IsItemHovered()) {
                m_context_offset=offset;
                m_context_hex=hex;

                ImGui::OpenPopup(CONTEXT_POPUP_NAME);
            }
        }
    }

    if(hex==m_context_hex&&m_context_offset==offset) {
        if(ImGui::BeginPopup(CONTEXT_POPUP_NAME)) {
            this->DoContextPopup();
            ImGui::EndPopup();
        } else {
            m_context_offset=INVALID_OFFSET;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoOptionsPopup() {
    ImGui::Checkbox("Show ASCII",&this->ascii);
    ImGui::Checkbox("Grey 00s",&this->grey_00s);
    ImGui::Checkbox("Grey non-printables",&this->grey_nonprintables);
    ImGui::Checkbox("Upper case",&this->upper_case);
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

static void ShowValue(int32_t value,const char *prefix,int dec_width,int num_bytes) {
    char binary[33];
    {
        uint32_t mask=1<<(num_bytes*8-1);
        size_t i=0;
        IM_ASSERT(num_bytes*8+1<=sizeof binary);
        while(mask!=0) {
            IM_ASSERT(i<sizeof binary);
            binary[i]=(uint32_t)value&mask?'1':'0';
            mask>>=1;
            ++i;
        }

        IM_ASSERT(i<sizeof binary);
        binary[i++]=0;
    }

    int prefix_len=(int)strlen(prefix);

    ImGui::Text("%s: %0*" PRId32 " %0*" PRIu32 "u 0x%0*" PRIx32,prefix,dec_width,value,dec_width,(uint32_t)value,num_bytes*2,(uint32_t)value);
    ImGui::Text("%*s  %%%s",prefix_len,"",binary);
}

void HexEditor::DoContextPopup() {
    m_handler->DoContextPopupExtraGui(m_hex,m_context_offset);

    HexEditorByte bytes[4];
    for(size_t i=0,offset=m_context_offset;i<IM_ARRAYSIZE(bytes);++i,++offset) {
        if(offset<m_handler->GetSize()) {
            m_handler->ReadByte(&bytes[i],offset);
        }
    }

    int16_t wl=0,wb=0;
    bool wok=false;
    int32_t ll=0,lb=0;
    bool lok=false;

    if(bytes[0].got_value&&bytes[1].got_value) {
        wl=(int16_t)(bytes[0].value|bytes[1].value<<8);
        wb=(int16_t)(bytes[0].value<<8|bytes[1].value);
        wok=true;

        if(bytes[2].got_value&&bytes[3].got_value) {
            ll=bytes[0].value|bytes[1].value<<8|bytes[2].value<<16|bytes[3].value<<24;
            lb=bytes[0].value<<24|bytes[1].value<<16|bytes[2].value<<8|bytes[3].value;
            lok=true;
        }
    }

    if(bytes[0].got_value&&bytes[0].value>=32&&bytes[0].value<127) {
        ImGui::Text("c: '%c'",(char)bytes[0].value);
    }

    ShowValue((int32_t)(int8_t)bytes[0].value,"b",3,1);

    if(wok) {
        ShowValue(wl,"wL",5,2);
        ShowValue(wb,"wB",5,2);
    }

    if(lok) {
        ShowValue(ll,"lL",10,4);
        ShowValue(lb,"lB",10,4);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

