#include "test_imgui_memory_editor.h"

#include "pushwarn_imgui_whatever.h"
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_memory_editor.h>
#include "popwarn.h"

#include <assert.h>
#include <memory>
#include <dear_imgui_hex_editor.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define ASSERT(X) ((X) ? (void)0 : __debugbreak(), (void)0)
#else
#define ASSERT(X) assert(X)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestHandler;
class TestMemoryEditor;

static uint8_t g_buffer[711];
static std::unique_ptr<TestMemoryEditor> g_memory_editor;
static bool g_hex_editor_open;
static std::unique_ptr<TestHandler> g_test_handler;
static std::unique_ptr<HexEditor> g_hex_editor;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestHandler : public HexEditorHandlerWithBufferData {
  public:
    TestHandler(void *buffer, size_t buffer_size)
        : HexEditorHandlerWithBufferData(buffer, buffer_size) {
    }

    void ReadByte(HexEditorByte *byte, size_t offset) override {
        if (offset >= 32 && offset < 64) {
            byte->got_value = false;
        } else if (offset >= 64 && offset < 80) {
            this->HexEditorHandlerWithBufferData::ReadByte(byte, offset);
            byte->can_write = false;
            byte->colour = IM_COL32(255, 0, 0, 255);
        } else {
            this->HexEditorHandlerWithBufferData::ReadByte(byte, offset);
        }
    }

    void DebugPrint(const char *fmt, ...) override {
        va_list v;

        va_start(v, fmt);
        vprintf(fmt, v);
        va_end(v);
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static unsigned char ReadBuffer(const ImU8 *data, size_t off) {
    (void)data;

    ASSERT(off < sizeof g_buffer);
    return g_buffer[off];
}

static void WriteBuffer(ImU8 *data, size_t off, unsigned char value) {
    (void)data;

    ASSERT(off < sizeof g_buffer);
    g_buffer[off] = value;
}

static bool HighlightValue(const ImU8 *data, size_t off) {
    (void)data, (void)off;
    ASSERT(off < sizeof g_buffer);
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestMemoryEditor {
  public:
    TestMemoryEditor() {
        m_edit.ReadFn = &ReadBuffer;
        m_edit.WriteFn = &WriteBuffer;
        m_edit.HighlightFn = &HighlightValue;
    }

    void DoImGui() {
        m_edit.DrawWindow("Example: imgui_club memory editor",
                          (unsigned char *)g_buffer,
                          sizeof g_buffer);
    }

  protected:
  private:
    MemoryEditor m_edit;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestImguiMemoryEditor() {
    // yum
    if (!g_memory_editor) {
        g_test_handler = std::make_unique<TestHandler>(g_buffer, sizeof g_buffer);
        g_hex_editor = std::make_unique<HexEditor>(g_test_handler.get());

        g_memory_editor = std::make_unique<TestMemoryEditor>();

        for (size_t i = 0; i < sizeof g_buffer; ++i) {
            g_buffer[i] = i < 256 ? (uint8_t)i : (uint8_t)rand();
        }
    }

    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
    g_memory_editor->DoImGui();

    {
        if (ImGui::Begin("Example: tom memory editor",
                         &g_hex_editor_open,
                         ImGuiWindowFlags_NoScrollbar)) {
            g_hex_editor->DoImGui();
        }
        ImGui::End();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
