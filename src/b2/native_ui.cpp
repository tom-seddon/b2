#define _CRT_NONSTDC_NO_DEPRECATE
#include <shared/system.h>
#include "native_ui.h"
#include <shared/debug.h>
#include <shared/path.h>
#include <map>
#include "Messages.h"
#include <shared/system_specific.h>
#include <shared/log.h>
#include "native_ui_private.h"
#include "b2.h"
#include <string.h>

#if SYSTEM_OSX
#include "native_ui_osx.h"
#elif SYSTEM_WINDOWS
#include "native_ui_windows.h"
#elif SYSTEM_LINUX
#include "native_ui_gtk.h"
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<const SelectorDialogTag *, RecentPaths> g_recent_paths_by_tag;

// SelectorDialogTag objects are intended to be globals, initialised
// before main begins. This flag is a crude way of checking for this.
static bool g_selector_dialog_tags_ever_accessed;

static std::vector<const SelectorDialogTag *> *g_selector_dialog_tags;

static std::vector<const SelectorDialogTag *> *GetSelectorDialogTagsArray() {
    static std::vector<const SelectorDialogTag *> s_selector_dialog_tags;

    g_selector_dialog_tags = &s_selector_dialog_tags;

    return &s_selector_dialog_tags;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SelectorDialogTag::SelectorDialogTag(uint8_t guid0, uint8_t guid1, uint8_t guid2, uint8_t guid3, uint8_t guid4, uint8_t guid5, uint8_t guid6, uint8_t guid7, uint8_t guid8, uint8_t guid9, uint8_t guid10, uint8_t guid11, uint8_t guid12, uint8_t guid13, uint8_t guid14, uint8_t guid15, std::string name_)
    : guid{guid0, guid1, guid2, guid3, guid4, guid5, guid6, guid7, guid8, guid9, guid10, guid11, guid12, guid13, guid14, guid15}
    , name(std::move(name_)) {
    ASSERT(!g_selector_dialog_tags_ever_accessed);

    std::vector<const SelectorDialogTag *> *tags = GetSelectorDialogTagsArray();

    for (const SelectorDialogTag *tag : *tags) {
        (void)tag;
        ASSERT(memcmp(tag->guid, this->guid, 16) != 0);
        ASSERT(tag->name != this->name);
    }

    tags->push_back(this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SelectorDialogTag::~SelectorDialogTag() {
    std::vector<const SelectorDialogTag *> *tags = GetSelectorDialogTagsArray();

    tags->erase(std::remove(tags->begin(), tags->end(), this), tags->end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<const SelectorDialogTag *> *GetAllSelectorDialogTags() {
    g_selector_dialog_tags_ever_accessed = true;

    return GetSelectorDialogTagsArray();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FailureMessageBox(const std::string &title, const std::shared_ptr<MessageList> &message_list, size_t num_messages) {
    std::vector<MessageList::Message> messages;

    message_list->ForEachMessage(num_messages, [&](MessageList::Message *m) {
        messages.push_back(*m);
    });

    std::string last_error;

    if (messages.empty()) {
        last_error = "No further information available.";
    } else {
        ASSERT(messages.size() <= PTRDIFF_MAX);
        for (size_t i = 0; i < messages.size(); ++i) {
            size_t index = messages.size() - 1 - i;
            MessageList::Message *m = &messages[index];

            if (m->type == MessageType_Error || m->type == MessageType_Warning) {
                last_error = m->text;
                messages.erase(messages.begin() + (ptrdiff_t)index);
                break;
            }
        }
    }

    std::string text;

    if (!last_error.empty()) {
        text = last_error;
    }

    if (!messages.empty()) {
        text += "\n";

        for (auto &&message : messages) {
            text += message.text;
        }
    }

#if SYSTEM_WINDOWS

    MessageBox(nullptr, text.c_str(), title.c_str(), MB_ICONERROR);

#elif SYSTEM_OSX || SYSTEM_LINUX

    MessageBox(title, text);

#else

    // ????

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths *GetRecentPathsByTag(const SelectorDialogTag *tag) {
    return &g_recent_paths_by_tag[tag];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetRecentPathsByTag(const SelectorDialogTag *tag, RecentPaths recents) {
    g_recent_paths_by_tag[tag] = std::move(recents);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CloseModalDialog() {
    ASSERT(!IsMainThread()); //TODO: fix if there's ever a need for it...

    // If the modal is open, initiate the close.
    {
        LockGuard<Mutex> lock(g_native_ui_globals_mutex);

        if (g_native_ui_modal_state == NativeUiModalState_Open) {
            CloseModalDialogLocked();
            g_native_ui_modal_state = NativeUiModalState_Closing;
        }
    }

    bool closed = WaitForModalNotOpen(1.0);
    return closed;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths::RecentPaths()
    : m_max_num_paths(20) //does this need to be tweakable?
{
    ASSERT(m_max_num_paths > 0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RecentPaths::AddPath(const char *path) {
    {
        auto it = m_paths.begin();

        while (it != m_paths.end()) {
            if (PathCompare(*it, path) == 0) {
                it = m_paths.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (m_paths.size() > m_max_num_paths) {
        m_paths.resize(m_max_num_paths - 1);
    }

    m_paths.insert(m_paths.begin(), path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t RecentPaths::GetNumPaths() const {
    return m_paths.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &RecentPaths::GetPathByIndex(size_t index) const {
    ASSERT(index < m_paths.size());

    return m_paths[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RecentPaths::RemovePathByIndex(size_t index) {
    ASSERT(index < m_paths.size());

    m_paths.erase(m_paths.begin() + (ptrdiff_t)index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SelectorDialog::SelectorDialog(const SelectorDialogTag *tag)
    : m_tag(tag) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SelectorDialog::~SelectorDialog() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths *SelectorDialog::GetRecentPaths() const {
    RecentPaths *recent = GetRecentPathsByTag(m_tag);
    return recent;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SelectorDialog::AddLastPathToRecentPaths() {
    if (RecentPaths *recent = GetRecentPathsByTag(m_tag)) {
        if (!m_last_path.empty()) {
            recent->AddPath(m_last_path.c_str());
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SelectorDialog::Open(SDL_Window *parent, std::string *path) {
    if (m_last_path.empty()) {
        if (RecentPaths *recent = GetRecentPathsByTag(m_tag)) {
            if (recent->GetNumPaths() > 0) {
                m_last_path = recent->GetPathByIndex(0);
            }
        }
    }

    std::string result = this->HandleOpen(parent);
    if (result.empty()) {
        m_last_path.clear();
        return false;
    } else {
        m_last_path = result;
        *path = m_last_path;
        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FileDialog::FileDialog(const SelectorDialogTag *tag)
    : SelectorDialog(tag) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FileDialog::AddFilter(std::string title, std::vector<std::string> patterns) {
#if ASSERT_ENABLED
    ASSERT(!patterns.empty());
    for (const std::string &pattern : patterns) {
        ASSERT(!pattern.empty());
        ASSERT(pattern[0] == '.');
    }
#endif

    m_filters.push_back({std::move(title), std::move(patterns)});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FileDialog::AddAllFilesFilter() {
    this->AddFilter("All files", {".*"});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

OpenFileDialog::OpenFileDialog(const SelectorDialogTag *tag)
    : FileDialog(tag) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OpenFileDialog::HandleOpen(SDL_Window *parent) {
    LOGF(OUTPUT, "%s: ", __func__);
    {
        LOG_EXTERN(OUTPUT);
        LOGI(OUTPUT);
        LOGF(OUTPUT, "Last path: ``%s''\n", m_last_path.c_str());
        //        LOGF(OUTPUT,"Default folder: ``%s''\n",default_folder.c_str());
        //        LOGF(OUTPUT,"Default name: ``%s''\n",default_name.c_str());
    }

#if SYSTEM_OSX

    // macOS modal dialogs are app-modal.
    (void)parent;
    return OpenFileDialogOSX(m_filters, m_last_path);

#elif SYSTEM_WINDOWS

    return OpenFileDialogWindows(parent, m_tag->guid, m_filters, m_last_path);

#else

    // The window SDL creates doesn't seem to be one that GTK
    // understands.
    (void)parent;
    return OpenFileDialogGTK(m_filters, m_last_path);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SaveFileDialog::SaveFileDialog(const SelectorDialogTag *tag)
    : FileDialog(tag) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialog::HandleOpen(SDL_Window *parent) {

#if SYSTEM_OSX

    (void)parent;
    return SaveFileDialogOSX(m_filters, m_last_path);

#elif SYSTEM_WINDOWS

    return SaveFileDialogWindows(parent, m_tag->guid, m_filters, m_last_path);

#else

    (void)parent;
    return SaveFileDialogGTK(m_filters, m_last_path);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// FolderDialog::FolderDialog(std::string tag)
//     : SelectorDialog(std::move(tag)) {
// }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// std::string FolderDialog::HandleOpen(SDL_Window *parent) {

// #if SYSTEM_OSX

//     (void)parent;
//     std::string r = SelectFolderDialogOSX(m_last_path);
//     return r;

// #elif SYSTEM_WINDOWS

//     std::string r = SelectFolderDialogWindows(parent, m_last_path);
//     return r;

// #else

//     (void)parent;
//     std::string r = SelectFolderDialogGTK(m_last_path);
//     return r;

// #endif
// }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
