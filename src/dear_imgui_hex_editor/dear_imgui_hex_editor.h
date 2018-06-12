#ifndef HEADER_EF50DD4251384D86B9D1AD17AED87393// -*- mode:c++ -*-
#define HEADER_EF50DD4251384D86B9D1AD17AED87393

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Memory editor for dear imgui. Based in part on the imgui_club
// memory editor[0].
//
// [0]
// https://github.com/ocornut/imgui_club/tree/master/imgui_memory_editor)
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <stdint.h>

struct ImGuiStyle;
struct ImDrawList;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HexEditorHandler {
public:
    HexEditorHandler();
    virtual ~HexEditorHandler()=0;

    HexEditorHandler(const HexEditorHandler &)=delete;
    HexEditorHandler &operator=(const HexEditorHandler &)=delete;

    HexEditorHandler(HexEditorHandler &&)=delete;
    HexEditorHandler &operator=(HexEditorHandler &&)=delete;

    // If result<0, the byte is assumed to be inaccessible.
    // Inaccessible bytes are never writeable.
    //
    // Should support efficient random access to bytes in the current
    // row.
    virtual int ReadByte(size_t offset)=0;
    virtual void WriteByte(size_t offset,uint8_t value)=0;
    virtual bool CanWrite(size_t offset)=0;
    virtual size_t GetSize()=0;

    virtual uintptr_t GetBaseAddress()=0;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HexEditorHandlerWithBufferData:
    public HexEditorHandler
{
public:
    HexEditorHandlerWithBufferData(void *buffer,size_t buffer_size);
    HexEditorHandlerWithBufferData(const void *buffer,size_t buffer_size);

    int ReadByte(size_t offset) override;
    void WriteByte(size_t offset,uint8_t value) override;
    bool CanWrite(size_t offset) override;
    size_t GetSize() override;
    virtual uintptr_t GetBaseAddress() override;
protected:
private:
    const uint8_t *m_read_buffer;
    uint8_t *m_write_buffer;
    size_t m_buffer_size;

    void Construct(const void *read_buffer,void *write_buffer,size_t buffer_size);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HexEditor {
public:
    bool ascii=true;
    bool grey_00s=false;
    bool upper_case=true;
    bool grey_nonprintables=true;
    size_t num_columns=16;

    explicit HexEditor(HexEditorHandler *handler);

    void DoImGui();
protected:
private:
    static const size_t INVALID_OFFSET=~(size_t)0;

    struct Metrics {
        int num_addr_digits=0;
        float line_height=0.f;
        float glyph_width=0.f;
        float hex_left_x=0.f;
        float hex_column_width=0.f;
        float ascii_left_x=0.f;
        uint32_t grey_colour=0;
    };

    HexEditorHandler *const m_handler;

    size_t m_num_calls=0;

    size_t m_offset=INVALID_OFFSET;
    bool m_hex=false;
    bool m_take_focus_next_frame=true;
    float m_next_frame_scroll_y=-1.f;

    int m_value=0;
    bool m_high_nybble=true;

    bool m_set_new_offset=false;
    size_t m_new_offset=INVALID_OFFSET;
    char m_new_offset_input_buffer[100]={};

    size_t m_context_offset=INVALID_OFFSET;

    // Per-frame working data.
    ImDrawList *m_draw_list=nullptr;
    Metrics m_metrics;
    uint32_t m_highlight_colour=0;
    bool m_was_TextInput_visible=false;

    void GetMetrics(Metrics *metrics,const ImGuiStyle &style);
    void DoHexPart(size_t begin_offset,size_t end_offset);
    void DoAsciiPart(size_t begin_offset,size_t end_offset);
    void GetChar(uint16_t *ch,bool *editing,const char *id);
    void UpdateOffsetByKey(int key,int delta,int times);
    void SetNewOffset(size_t base,int delta,bool invalidate_on_failure);
    char GetDisplayChar(int value,bool *wasprint=nullptr) const;
    void OpenContextPopup(size_t offset);
    void DoOptionsPopup();
    void DoContextPopup();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
