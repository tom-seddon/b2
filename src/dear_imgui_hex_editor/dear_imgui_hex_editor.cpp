#include "dear_imgui_hex_editor.h"
#include <algorithm>
#include <ctype.h>

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorData::HexEditorData() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorData::~HexEditorData() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorBufferData::HexEditorBufferData(void *buffer,size_t buffer_size) {
    this->Construct(buffer,buffer,buffer_size);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditorBufferData::HexEditorBufferData(const void *buffer,size_t buffer_size) {
    this->Construct(buffer,nullptr,buffer_size);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t HexEditorBufferData::ReadByte(size_t offset) {
    IM_ASSERT(offset<m_buffer_size);

    return m_read_buffer[offset];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorBufferData::WriteByte(size_t offset,uint8_t value) {
    IM_ASSERT(m_write_buffer);
    IM_ASSERT(offset<m_buffer_size);

    m_write_buffer[offset]=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HexEditorBufferData::CanWrite() const {
    return !!m_write_buffer;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t HexEditorBufferData::GetSize() const {
    return m_buffer_size;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditorBufferData::Construct(const void *read_buffer,void *write_buffer,size_t buffer_size) {
    m_read_buffer=(const uint8_t *)read_buffer;
    m_write_buffer=(uint8_t *)write_buffer;
    m_buffer_size=buffer_size;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HexEditor::HexEditor() {
    m_offset=0;
    m_hex=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoImGui(HexEditorData *data,size_t base_address) {
    const ImGuiStyle &style=ImGui::GetStyle();

    this->GetMetrics(&m_metrics,style,data,base_address);
    m_data=data;
    m_highlight_colour=ImGui::GetColorU32(ImGuiCol_TextSelectedBg);

    const float footer_height=style.ItemSpacing.y+ImGui::GetFrameHeightWithSpacing();

    const size_t data_size=data->GetSize();
    const bool data_can_write=data->CanWrite();

    m_set_new_offset=false;
    m_new_offset=INVALID_OFFSET;
    m_was_TextInput_visible=false;

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
                size_t num_lines=(data->GetSize()+this->num_columns-1)/this->num_columns;
                IM_ASSERT(num_lines<=INT_MAX);
                ImGuiListClipper clipper((int)num_lines,m_metrics.line_height);

                first_visible_row=clipper.DisplayStart;

                for(int line_index=clipper.DisplayStart;line_index<clipper.DisplayEnd;++line_index) {
                    size_t line_begin_offset=(size_t)line_index*this->num_columns;
                    ImGui::Text("%0*zX:",m_metrics.num_addr_digits,base_address+line_begin_offset);

                    size_t line_end_offset=(std::min)(line_begin_offset+this->num_columns,data_size);

                    ImGui::PushID(line_index);
                    this->DoHexPart(line_begin_offset,line_end_offset,base_address);

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
        ImGui::Checkbox("Show ASCII",&this->ascii);
        ImGui::Checkbox("Grey 00s",&this->grey_00s);
        ImGui::Checkbox("Grey non-printables",&this->grey_nonprintables);
        ImGui::Checkbox("Upper case",&this->upper_case);
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
    }

    if(m_set_new_offset) {
        m_offset=m_new_offset;
        m_take_focus_next_frame=true;

        printf("%zu - set new offset: now 0x%zx\n",m_num_calls,m_offset);
        //printf("%zu - clipper = %d -> %d\n",m_num_calls,clipper_display_start,clipper_display_end);

        if(m_hex) {
            m_high_nybble=true;
        }

        if(m_offset!=INVALID_OFFSET) {
            m_value=m_data->ReadByte(m_offset);

            size_t row=m_offset/this->num_columns;

            if(row<first_visible_row) {
                m_next_frame_scroll_y=row*m_metrics.line_height;
            } else if(row>=first_visible_row+num_visible_rows) {
                m_next_frame_scroll_y=(row-(num_visible_rows-1))*m_metrics.line_height;
            }

            //printf("row=%zu, clipper start=%d, clipper end=%d, scroll next frame=%f\n",row,clipper_display_start,clipper_display_end,m_next_frame_scroll_y);
        }
    } else {
        if(m_offset!=INVALID_OFFSET) {
            if(!m_was_TextInput_visible) {
                // sort out TextInput again when it next becomes visible.
                m_take_focus_next_frame=true;
            }
        }
    }

    m_data=nullptr;

    ++m_num_calls;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::GetMetrics(Metrics *metrics,const ImGuiStyle &style,HexEditorData *data,size_t base_address) {
    (void)style;

    metrics->num_addr_digits=0;
    {
        size_t n=base_address+data->GetSize()-1;
        while(n!=0) {
            ++metrics->num_addr_digits;
            n>>=4;
        }
    }

    metrics->line_height=ImGui::GetTextLineHeight();
    metrics->glyph_width=ImGui::CalcTextSize("F").x+1;

    metrics->hex_left_x=(metrics->num_addr_digits+2)*metrics->glyph_width;
    metrics->hex_column_width=2.5f*metrics->glyph_width;

    metrics->ascii_left_x=metrics->hex_left_x+this->num_columns*metrics->hex_column_width+2.f*metrics->glyph_width;
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

void HexEditor::DoHexPart(size_t begin_offset,size_t end_offset,size_t base_address) {
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

    for(size_t offset=begin_offset;offset!=end_offset;++offset) {
        uint8_t value=m_data->ReadByte(offset);

        bool editing;

        if(offset==m_offset) {
            m_draw_list->AddRectFilled(screen_pos,ImVec2(screen_pos.x+m_metrics.glyph_width*2.f,screen_pos.y+m_metrics.line_height),m_highlight_colour);
        }

        if(offset==m_offset&&m_hex) {
            bool commit=false;
            editing=true;

            if(m_high_nybble) {
                ImGui::SameLine(x);
            } else {
                ImGui::SameLine(x+m_metrics.glyph_width);
            }

            // The TextInput is just a fudgy way of getting some
            // keyboard input, so its flags are a random mishmash of
            // whatever made this work. I tried to make this a bit
            // simpler than the imgui_club hex editor in terms of
            // fiddling with the input buffer and so on, but whether
            // it's any clearer overall... I wouldn't like to guess.

            // Quickly check for a couple of keys it's a pain to
            // handle.
            //
            // Tab could be handled in the input text callback, but
            // might as well do it here...
            if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))||ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))) {
                commit=true;
                editing=false;
            } else if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
                this->SetNewOffset(INVALID_OFFSET,0,true);
                m_hex=true;
                editing=false;
            } else {
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
                            if(m_high_nybble) {
                                m_value&=0x0f;
                                m_value|=(uint8_t)nybble<<4;
                                m_high_nybble=false;
                            } else {
                                m_value&=0xf0;
                                m_value|=(uint8_t)nybble;
                                commit=true;
                            }
                        }

                        printf("%zu - got char: %u, 0x%04X, '%c'\n",m_num_calls,ch,ch,ch>=32&&ch<127?(char)ch:'?');
                    }
                }
            }

            if(commit) {
                m_data->WriteByte(m_offset,m_value);

                this->SetNewOffset(m_offset,1,true);
                m_hex=true;
            }
        } else {
            editing=false;
        }

        if(editing) {
            ImGui::SameLine(x);
            if(m_high_nybble) {
                ImGui::Text("%c",hex_chars[m_value>>4]);
                ImGui::SameLine(x+m_metrics.glyph_width);
                ImGui::TextDisabled("%c",hex_chars[m_value&0xf]);
            } else {
                ImGui::TextDisabled("%c",hex_chars[m_value>>4]);
                ImGui::SameLine(x+m_metrics.glyph_width);
                ImGui::Text("%c",hex_chars[m_value&0xf]);
            }
        } else {
            char text[]={
                hex_chars[value>>4],
                hex_chars[value&0xf],
                ' ',
                0,
            };

            ImGui::SameLine(x);

            if(value==0&&this->grey_00s) {
                ImGui::TextDisabled(text);
            } else {
                ImGui::TextUnformatted(text,text+sizeof text-1);
            }

            if(ImGui::IsItemHovered()) {
                //printf("hover item: %zx\n",offset);
                if(ImGui::IsMouseClicked(0)) {
                    this->SetNewOffset(offset,0,false);
                    m_hex=true;
                    //new_offset=offset;
                    //got_new_offset=true;
                }
            }
        }

        x+=m_metrics.hex_column_width;
        screen_pos.x+=m_metrics.hex_column_width;
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

    for(size_t offset=begin_offset;offset!=end_offset;++offset) {
        uint8_t value=m_data->ReadByte(offset);

        bool wasprint;
        char display_char[2]={
            this->GetDisplayChar(value,&wasprint),
        };

        if(offset==m_offset) {
            m_draw_list->AddRectFilled(screen_pos,ImVec2(screen_pos.x+m_metrics.glyph_width,screen_pos.y+m_metrics.line_height),m_highlight_colour);
        }

        bool editing;
        if(offset==m_offset&&!m_hex) {
            editing=true;

            ImWchar ch;
            ImGui::SameLine(x);
            this->GetChar(&ch,&editing,"ascii_input");

            if(editing) {
                if(ch>=32&&ch<127) {
                    m_data->WriteByte(m_offset,(uint8_t)ch);

                    this->SetNewOffset(m_offset,1,true);
                    m_hex=false;
                }
            }
        } else {
            editing=false;
        }

        ImGui::SameLine(x);
        if(!wasprint&&this->grey_nonprintables) {
            ImGui::TextDisabled(display_char);
        } else {
            ImGui::TextUnformatted(display_char);
        }

        if(!editing) {
            if(ImGui::IsItemHovered()) {
                if(ImGui::IsMouseClicked(0)) {
                    printf("%zu - clicked on offset 0x%zx\n",m_num_calls,offset);
                    this->SetNewOffset(offset,0,false);
                    m_hex=false;
                }
            }
        }

        x+=m_metrics.glyph_width;
        screen_pos.x+=m_metrics.glyph_width;
    }

    ImGui::PopItemWidth();

    ImGui::PopID();
    ImGui::PopID();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
        printf("%zu - m_offset=0x%zx: InputText inactive. Invalidating offset.\n",m_num_calls,m_offset);
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
            m_new_offset=base+delta;
            m_set_new_offset=true;
            failed=false;
        } else {
            failed=true;
        }
    } else if(delta>0) {
        if(base+(size_t)delta<m_data->GetSize()) {
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

char HexEditor::GetDisplayChar(uint8_t value,bool *wasprint) const {
    if(value>=32&&value<127) {
        if(wasprint) {
            *wasprint=true;
        }

        return (char)value;
    } else {
        if(wasprint) {
            *wasprint=false;
        }

        return '.';
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
