#define _CRT_NONSTDC_NO_DEPRECATE
#include <shared/system.h>
#include "native_ui.h"
#include <shared/debug.h>
#include <shared/path.h>
#include <map>
#include "Messages.h"
#include <shared/system_specific.h>
#include <shared/log.h>

#if SYSTEM_OSX
#include "native_ui_osx.h"
#elif SYSTEM_WINDOWS
#include "native_ui_windows.h"
#elif SYSTEM_LINUX
#include "native_ui_gtk.h"
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<std::string,RecentPaths> g_recent_paths_by_tag;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FailureMessageBox(const std::string &title,const std::shared_ptr<MessageList> &message_list,size_t num_messages) {
    std::vector<MessageList::Message> messages;

    message_list->ForEachMessage(num_messages,[&](MessageList::Message *m) {
        messages.push_back(*m);
    });

    std::string last_error;

    if(messages.empty()) {
        last_error="No further information available.";
    } else {
        ASSERT(messages.size()<=PTRDIFF_MAX);
        for(size_t i=0;i<messages.size();++i) {
            size_t index=messages.size()-1-i;
            MessageList::Message *m=&messages[index];

            if(m->type==MessageType_Error||m->type==MessageType_Warning) {
                last_error=m->text;
                messages.erase(messages.begin()+(ptrdiff_t)index);
                break;
            }
        }
    }

    std::string text;

    if(!last_error.empty()) {
        text=last_error;
    }

    if(!messages.empty()) {
        text+="\n";

        for(auto &&message:messages) {
            text+=message.text;
        }
    }

#if SYSTEM_WINDOWS

    MessageBox(nullptr,text.c_str(),title.c_str(),MB_ICONERROR);

#elif SYSTEM_OSX||SYSTEM_LINUX

    MessageBox(title,text);

#else

    // ????

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ForEachRecentPaths(std::function<void(const std::string &,const RecentPaths &)> fun) {
    for(auto &&it:g_recent_paths_by_tag) {
        fun(it.first,it.second);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths *GetRecentPathsByTag(const std::string &tag) {
    if(tag.empty()) {
        return nullptr;
    }

    return &g_recent_paths_by_tag[tag];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetRecentPathsByTag(std::string tag,RecentPaths recents) {
    g_recent_paths_by_tag[std::move(tag)]=std::move(recents);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths::RecentPaths():
    m_max_num_paths(20)//does this need to be tweakable?
{
    ASSERT(m_max_num_paths>0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RecentPaths::AddPath(const char *path) {
    {
        auto it=m_paths.begin();

        while(it!=m_paths.end()) {
            if(PathCompare(*it,path)==0) {
                it=m_paths.erase(it);
            } else {
                ++it;
            }
        }
    }

    if(m_paths.size()>m_max_num_paths) {
        m_paths.resize(m_max_num_paths-1);
    }

    m_paths.insert(m_paths.begin(),path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t RecentPaths::GetNumPaths() const {
    return m_paths.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &RecentPaths::GetPathByIndex(size_t index) const {
    ASSERT(index<m_paths.size());

    return m_paths[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RecentPaths::RemovePathByIndex(size_t index) {
    ASSERT(index<m_paths.size());

    m_paths.erase(m_paths.begin()+(ptrdiff_t)index);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SelectorDialog::SelectorDialog(std::string tag):
    m_recent_paths_tag(std::move(tag))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SelectorDialog::~SelectorDialog() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths *SelectorDialog::GetRecentPaths() const {
    RecentPaths *recent=GetRecentPathsByTag(m_recent_paths_tag);
    return recent;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SelectorDialog::AddLastPathToRecentPaths() {
    if(RecentPaths *recent=GetRecentPathsByTag(m_recent_paths_tag)) {
        if(!m_last_path.empty()) {
            recent->AddPath(m_last_path.c_str());
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SelectorDialog::Open(std::string *path) {
    if(m_last_path.empty()) {
        if(RecentPaths *recent=GetRecentPathsByTag(m_recent_paths_tag)) {
            if(recent->GetNumPaths()>0) {
                m_last_path=recent->GetPathByIndex(0);
            }
        }
    }

    std::string result=this->HandleOpen();
    if(result.empty()) {
        m_last_path.clear();
        return false;
    } else {
        m_last_path=result;
        *path=m_last_path;
        return true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FileDialog::FileDialog(std::string tag):
    SelectorDialog(std::move(tag))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FileDialog::AddFilter(std::string title,std::vector<std::string> patterns) {
    ASSERT(!patterns.empty());
    for(const std::string &pattern:patterns) {
        ASSERT(!pattern.empty());
        ASSERT(pattern[0]=='.');
    }
    m_filters.push_back({std::move(title),std::move(patterns)});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FileDialog::AddAllFilesFilter() {
    this->AddFilter("All files",{".*"});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

OpenFileDialog::OpenFileDialog(std::string tag):
    FileDialog(std::move(tag))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OpenFileDialog::HandleOpen() {
    LOGF(OUTPUT,"%s: ",__func__);
    {
        LOG_EXTERN(OUTPUT);
        LOGI(OUTPUT);
        LOGF(OUTPUT,"Last path: ``%s''\n",m_last_path.c_str());
        //        LOGF(OUTPUT,"Default folder: ``%s''\n",default_folder.c_str());
        //        LOGF(OUTPUT,"Default name: ``%s''\n",default_name.c_str());
    }

#if SYSTEM_OSX

    return OpenFileDialogOSX(m_filters,m_last_path);

#elif SYSTEM_WINDOWS

    return OpenFileDialogWindows(m_filters,m_last_path);

#else

    return OpenFileDialogGTK(m_filters,m_last_path);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SaveFileDialog::SaveFileDialog(std::string tag):
    FileDialog(std::move(tag))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialog::HandleOpen() {

#if SYSTEM_OSX

    return SaveFileDialogOSX(m_filters,m_last_path);

#elif SYSTEM_WINDOWS

    return SaveFileDialogWindows(m_filters,m_last_path);

#else

    return SaveFileDialogGTK(m_filters,m_last_path);

#endif

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FolderDialog::FolderDialog(std::string tag):
    SelectorDialog(std::move(tag))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string FolderDialog::HandleOpen() {

#if SYSTEM_OSX

    std::string r=SelectFolderDialogOSX(m_last_path);
    return r;

#elif SYSTEM_WINDOWS

    std::string r=SelectFolderDialogWindows(m_last_path);
    return r;

#else

    std::string r=SelectFolderDialogGTK(m_last_path);
    return r;

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
