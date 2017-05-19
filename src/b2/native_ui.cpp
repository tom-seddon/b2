#define _CRT_NONSTDC_NO_DEPRECATE
#include <shared/system.h>
#include "native_ui.h"
#include <shared/debug.h>
#include <shared/path.h>
#include <map>
#include "Messages.h"
#include <shared/system_specific.h>
#include <shared/log.h>

#include "native_ui_noc.h"

#if SYSTEM_OSX
void MessageBox(const std::string &title,const std::string &text);
#elif SYSTEM_LINUX
#include <gtk/gtk.h>
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

#elif SYSTEM_OSX

    MessageBox(title,text);

#elif SYSTEM_LINUX

    // Doesn't seem to be any way to apply a title with GTK.
    (void)title;

    gtk_init_check(nullptr,nullptr);
    
    GtkWidget *dialog=gtk_message_dialog_new(nullptr,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             "%s",
                                             title.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             "%s",
                                             text.c_str());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

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

static const char *GetCStr(const std::string &str) {
    if(str.empty()) {
        return nullptr;
    } else {
        return str.c_str();
    }
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

    const char *r=this->HandleOpen();
    if(r) {
        m_last_path=r;
        *path=m_last_path;
        return true;
    } else {
        m_last_path.clear();
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FileDialog::FileDialog(std::string tag,int noc_flags):
    SelectorDialog(std::move(tag)),
    m_noc_flags(noc_flags)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FileDialog::AddFilter(std::string title,const std::string &patterns) {
    std::string filter=std::move(title);
    filter.push_back(0);
    filter.append(patterns);
    filter.push_back(0);

    m_filters.append(filter);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FileDialog::AddAllFilesFilter() {
#if SYSTEM_OSX

    // the initial "*." is actually ignored - this due to the way noc
    // deals with OS X's file dialog only accepting extensions, rather
    // than file patterns.
    this->AddFilter("All files","*.*");

#else

    this->AddFilter("All files","*");

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *FileDialog::HandleOpen() {
    std::string default_name=PathGetName(m_last_path);
    std::string default_folder=PathGetFolder(m_last_path);

    LOGF(OUTPUT,"%s: ",__func__);
    {
        LOG_EXTERN(OUTPUT);
        LOGI(OUTPUT);
        LOGF(OUTPUT,"Last path: ``%s''\n",m_last_path.c_str());
        LOGF(OUTPUT,"Default folder: ``%s''\n",default_folder.c_str());
        LOGF(OUTPUT,"Default name: ``%s''\n",default_name.c_str());
    }

    const char *r=noc_file_dialog_open(m_noc_flags,m_filters.c_str(),
                                       GetCStr(default_folder),
                                       GetCStr(default_name));
    return r;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

OpenFileDialog::OpenFileDialog(std::string tag):
    FileDialog(std::move(tag),NOC_FILE_DIALOG_OPEN)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SaveFileDialog::SaveFileDialog(std::string tag):
    FileDialog(std::move(tag),NOC_FILE_DIALOG_SAVE|NOC_FILE_DIALOG_OVERWRITE_CONFIRMATION)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FolderDialog::FolderDialog(std::string tag):
    SelectorDialog(std::move(tag))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *FolderDialog::HandleOpen() {
    const char *default_path=GetCStr(m_last_path);
    const char *r=noc_file_dialog_open(NOC_FILE_DIALOG_DIR,nullptr,default_path,nullptr);
    return r;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
