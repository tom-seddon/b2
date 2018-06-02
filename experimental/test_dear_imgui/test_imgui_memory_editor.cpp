#include "test_imgui_memory_editor.h"
#include "imgui_no_warnings.h"
#include <imgui_memory_editor.h>
#include <assert.h>
#include <memory>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define ASSERT(X) ((X)?(void)0:__debugbreak(),(void)0)
#else
#define ASSERT(X) assert(X)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestMemoryEditor;

static char g_buffer[1024];
static std::unique_ptr<TestMemoryEditor> g_editor;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static unsigned char ReadBuffer(unsigned char *data,size_t off) {
    (void)data;

    ASSERT(off<sizeof g_buffer);
    return g_buffer[off];
}

static void WriteBuffer(unsigned char *data,size_t off,unsigned char value) {
    (void)data;

    ASSERT(off<sizeof g_buffer);
    g_buffer[off]=value;
}

static bool HighlightValue(unsigned char *data,size_t off) {
    (void)data;
    ASSERT(off<sizeof g_buffer);
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TestMemoryEditor {
public:
    TestMemoryEditor() {
        m_edit.ReadFn=&ReadBuffer;
        m_edit.WriteFn=&WriteBuffer;
        m_edit.HighlightFn=&HighlightValue;
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
    if(!g_editor) {
        g_editor=std::make_unique<TestMemoryEditor>();
    }

    ImGui::SetNextWindowSize(ImVec2(700,600),ImGuiCond_FirstUseEver);
    g_editor->DoImGui();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
