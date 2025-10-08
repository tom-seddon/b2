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

// RecentPaths objects are intended to be globals, initialised before main
// begins. This flag is a crude way of checking for this.
static bool g_tables_ever_accessed;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<RecentPaths *> *g_all_recent_paths;

static std::vector<RecentPaths *> *GetMutableAllRecentPaths() {
    static std::vector<RecentPaths *> s_all_recent_paths;

    g_all_recent_paths = &s_all_recent_paths;

    return &s_all_recent_paths;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<RecentPaths *> *GetAllRecentPaths() {
    g_tables_ever_accessed = true;

    return GetMutableAllRecentPaths();
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

#error

#endif
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

RecentPaths::RecentPaths(std::string name_)
    : name(std::move(name_))
    , m_max_num_paths(20) //does this need to be tweakable?
{
    ASSERT(m_max_num_paths > 0);
    ASSERT(!g_tables_ever_accessed);

    std::vector<RecentPaths *> *all_paths = GetMutableAllRecentPaths();
    for (RecentPaths *paths : *all_paths) {
        (void)paths;
        ASSERT(paths->name != this->name);
    }

    all_paths->push_back(this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths::~RecentPaths() {
    std::vector<RecentPaths *> *all_paths = GetMutableAllRecentPaths();

    ASSERT(std::find(all_paths->begin(), all_paths->end(), this) != all_paths->end());
    all_paths->erase(std::remove(all_paths->begin(), all_paths->end(), this), all_paths->end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RecentPaths::Clear() {
    m_paths.clear();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RecentPaths::AddPath(const std::string path) {
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

    m_paths.insert(m_paths.begin(), std::move(path));
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

SelectorDialog::SelectorDialog(const Guid &guid)
    : m_guid(guid) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SelectorDialog::~SelectorDialog() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SelectorDialog::AddLastPathToRecentPaths(RecentPaths *paths) {
    if (!m_last_path.empty()) {
        paths->AddPath(m_last_path);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SelectorDialog::Open(SDL_Window *parent, std::string *path) {
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

FileDialog::FileDialog(const Guid &guid)
    : SelectorDialog(guid) {
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

OpenFileDialog::OpenFileDialog(const Guid &guid)
    : FileDialog(guid) {
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

    return OpenFileDialogWindows(parent, m_guid, m_filters, m_last_path);

#else

    // The window SDL creates doesn't seem to be one that GTK
    // understands.
    (void)parent;
    return OpenFileDialogGTK(m_filters, m_last_path);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SaveFileDialog::SaveFileDialog(const Guid &guid)
    : FileDialog(guid) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SaveFileDialog::SetSuggestedName(const std::string &path) {
    m_suggested_name = PathGetName(path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SaveFileDialog::SetSaveAsPath(std::string save_as_path) {
    m_save_as_path = std::move(save_as_path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialog::HandleOpen(SDL_Window *parent) {

#if SYSTEM_OSX

    (void)parent;
    return SaveFileDialogOSX(m_filters, m_last_path);

#elif SYSTEM_WINDOWS

    return SaveFileDialogWindows(parent, m_guid, m_filters, m_suggested_name, m_save_as_path);

#else

    (void)parent;
    return SaveFileDialogGTK(m_filters, m_last_path);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
