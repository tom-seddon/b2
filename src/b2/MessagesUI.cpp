#include <shared/system.h>
#include "MessagesUI.h"
#include "dear_imgui.h"
#include "Messages.h"
#include "commands.h"
#include "SettingsUI.h"
#include "BeebWindow.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const ImVec4 INFO_COLOUR(1.f, 1.f, 1.f, 1.f);
static const ImVec4 WARNING_COLOUR(1.f, 1.f, 0.f, 1.f);
static const ImVec4 ERROR_COLOUR(1.f, 0.f, 0.f, 1.f);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ImGuiMessageListMessage(const MessageList::Message *m) {
    ImGuiStyleColourPusher pusher;

    switch (m->type) {
    case MessageType_Info:
        pusher.Push(ImGuiCol_Text, INFO_COLOUR);
        break;

    case MessageType_Warning:
        pusher.Push(ImGuiCol_Text, WARNING_COLOUR);
        break;

    case MessageType_Error:
        pusher.Push(ImGuiCol_Text, ERROR_COLOUR);
        break;
    }

    ImGui::TextUnformatted(m->text.c_str(), m->text.c_str() + m->text.size());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MessagesUI : public SettingsUI {
  public:
    MessagesUI(std::shared_ptr<MessageList> message_list);

    void DoImGui() override;

    bool OnClose() override;

  protected:
  private:
    std::shared_ptr<MessageList> m_message_list;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static CommandTable2 g_messages_table("Messages Window");
static Command2 g_copy_command(&g_messages_table, "copy", "Copy");
static Command2 g_clear_command(&g_messages_table, "clear", "Clear");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessagesUI::MessagesUI(std::shared_ptr<MessageList> message_list)
    : SettingsUI(&g_messages_table)
    , m_message_list(std::move(message_list)) {
    this->SetDefaultSize(ImVec2(600, 300));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessagesUI::DoImGui() {
    if (this->cst->WasActioned(g_clear_command)) {
        m_message_list->ClearMessages();
    }

    if (this->cst->WasActioned(g_copy_command)) {
        ImGuiIO &io = ImGui::GetIO();

        std::string text;

        m_message_list->ForEachMessage(
            [&text](const MessageList::Message *m) {
                text += m->text;
            });

        (*io.SetClipboardTextFn)(io.ClipboardUserData, text.c_str());
    }

    //
    this->cst->DoButton(g_clear_command);

    ImGui::SameLine();

    this->cst->DoButton(g_copy_command);

    ImGui::BeginChild("##messages", ImVec2(), true);

    m_message_list->ForEachMessage(&ImGuiMessageListMessage);

    ImGui::EndChild();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool MessagesUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateMessagesUI(BeebWindow *beeb_window) {
    return std::make_unique<MessagesUI>(beeb_window->GetMessageList());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
