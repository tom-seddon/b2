#include "hex_editor.h"
#include "imgui_no_warnings.h"
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char HEX_CHARS_UC[]="0123456789ABCDEF";
static const char HEX_CHARS_LC[]="0123456789abcdef";

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

HexEditor::HexEditor():
    highlight_colour(IM_COL32(255,255,255,40))
{
    m_offset=0;
    m_hex=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HexEditor::DoImGui(HexEditorData *data,size_t base_address) {
    const ImGuiStyle &style=ImGui::GetStyle();

    this->GetMetrics(&m_metrics,style,data,base_address);
    m_data=data;

    const float footer_height=style.ItemSpacing.y+ImGui::GetFrameHeightWithSpacing();

    const size_t data_size=data->GetSize();
    const bool data_can_write=data->CanWrite();

    m_set_new_offset=false;
    m_new_offset=INVALID_OFFSET;

    ImGui::BeginChild("##scrolling",ImVec2(0,-footer_height),false,ImGuiWindowFlags_NoMove);
    {
        m_draw_list=ImGui::GetWindowDrawList();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,0));
        {
            size_t num_lines=(data->GetSize()+this->num_columns-1)/this->num_columns;
            IM_ASSERT(num_lines<=INT_MAX);
            ImGuiListClipper clipper((int)num_lines,m_metrics.line_height);

            for(int line_index=clipper.DisplayStart;line_index<clipper.DisplayEnd;++line_index) {
                size_t line_begin_offset=(size_t)line_index*this->num_columns;
                ImGui::Text("%0*zX:",m_metrics.num_addr_digits,base_address+line_begin_offset);

                size_t line_end_offset=(std::min)(line_begin_offset+this->num_columns,data_size);

                ImGui::PushID(line_index);
                this->DoHexPart(line_begin_offset,line_end_offset,base_address);
                this->DoAsciiPart(line_begin_offset,line_end_offset);
                ImGui::PopID();
            }

            clipper.End();
        }

        ImGui::PopStyleVar(2);

        m_draw_list=nullptr;
    }

    ImGui::EndChild();

    if(m_offset!=INVALID_OFFSET) {
        if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
            this->SetNewOffset(m_offset,-(int)this->num_columns,false);
        } else if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
            this->SetNewOffset(m_offset,(int)this->num_columns,false);
        } else if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow))) {
            this->SetNewOffset(m_offset,-1,false);
        } else if(ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow))) {
            this->SetNewOffset(m_offset,1,false);
        }
    }

    if(m_set_new_offset) {
        m_offset=m_new_offset;
        m_taken_focus=false;

        printf("set new offset: now 0x%zx\n",m_offset);

        if(m_hex) {
            m_high_nybble=true;
        }

        if(m_offset!=INVALID_OFFSET) {
            m_value=m_data->ReadByte(m_offset);
        }
    }


    m_data=nullptr;
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

    if(data->EventFlag==ImGuiInputTextFlags_CallbackCharFilter) {
        *ch=data->EventChar;
    } else if(data->EventFlag==ImGuiInputTextFlags_CallbackCompletion) {
        *ch='\t';
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

    //if(ImGui::IsMouseClicked(0)) {
    //    printf("click: begin_offset=%zx\n",begin_offset);
    //}

    for(size_t offset=begin_offset;offset!=end_offset;++offset) {
        uint8_t value=m_data->ReadByte(offset);

        bool editing;

        if(offset==m_offset&&m_hex) {
            bool commit=false;
            editing=true;

            ImGui::SameLine(x);

            ImVec2 pos=ImGui::GetCursorScreenPos();
            m_draw_list->AddRectFilled(pos,ImVec2(pos.x+m_metrics.glyph_width*2.f,pos.y+m_metrics.line_height),this->highlight_colour);

            if(!m_high_nybble) {
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
                editing=false;
            } else {
                ImGui::PushID("input");

                bool taken_focus=m_taken_focus;

                if(!m_taken_focus) {
                    printf("focus taken\n");

                    ImGui::SetKeyboardFocusHere();
                    ImGui::CaptureKeyboardFromApp(true);

                    m_taken_focus=true;
                }

                // This makes the InputText wide enough to display the
                // cursor.
                ImGui::PushItemWidth(1.f);

                ImGuiInputTextFlags flags=(//ImGuiInputTextFlags_CharsHexadecimal|
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
                char text[2]="";
                ImWchar event_char=0;
                bool input_text_result=ImGui::InputText("",text,sizeof text,flags,&ReportCharCallback,&event_char);

                if(taken_focus&&!ImGui::IsItemActive()) {
                    this->SetNewOffset(INVALID_OFFSET,0,true);
                    editing=false;
                }

                ImGui::PopItemWidth();
                ImGui::PopID();

                if(editing) {
                    if(event_char!=0) {
                        int nybble=-1;
                        if(event_char>='0'&&event_char<='9') {
                            nybble=event_char-'0';
                        } else if(event_char>='a'&&event_char<='f') {
                            nybble=event_char-'a'+10;
                        } else if(event_char>='A'&&event_char<='F') {
                            nybble=event_char-'A'+10;
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

                        printf("got char: %u, 0x%04X, '%c'\n",event_char,event_char,event_char>=32&&event_char<127?(char)event_char:'?');
                    }
                }
            }

            if(commit) {
                m_data->WriteByte(m_offset,m_value);

                this->SetNewOffset(m_offset,1,true);
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
            };

            ImGui::SameLine(x);

            if(value==0&&this->grey_zeroes) {
                ImGui::TextDisabled(text);
            } else {
                ImGui::TextUnformatted(text,text+sizeof text);
            }

            if(ImGui::IsItemHovered()) {
                //printf("hover item: %zx\n",offset);
                if(ImGui::IsMouseClicked(0)) {
                    this->SetNewOffset(offset,0,false);
                    //new_offset=offset;
                    //got_new_offset=true;
                }
            }
        }

        x+=m_metrics.hex_column_width;
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

    for(size_t offset=begin_offset;offset!=end_offset;++offset) {
        uint8_t value=m_data->ReadByte(offset);

        bool wasprint;
        char display_char=this->GetDisplayChar(value,&wasprint);

        ImGui::SameLine(x);
        ImGui::Text("%c",display_char);

        x+=m_metrics.glyph_width;
    }

    ImGui::PopID();
    ImGui::PopID();
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
