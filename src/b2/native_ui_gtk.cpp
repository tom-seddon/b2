#include <shared/system.h>
#include "native_ui.h"
#include "native_ui_gtk.h"
#include <gtk/gtk.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageBox(const std::string &title,const std::string &text) {
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

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static GtkWidget *CreateFileDialog(const char *title,
                                   GtkFileChooserAction action) {
    gtk_init_check(nullptr,nullptr);
    GtkWidget *gdialog=gtk_file_chooser_dialog_new(title,
                                                   nullptr,
                                                   action,
                                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                                   "_Open", GTK_RESPONSE_ACCEPT,
                                                   nullptr);
    return gdialog;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string RunFileDialog(GtkWidget *gdialog) {
    gint gresult=gtk_dialog_run(GTK_DIALOG(gdialog));

    std::string result;
    if(gresult==GTK_RESPONSE_ACCEPT) {
        if(const char *name=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(gdialog))) {
            result.assign(name);
        }
    }

    gtk_widget_destroy(gdialog);
    gdialog=nullptr;

    while(gtk_events_pending()) {
        gtk_main_iteration();
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void SetDefaultPath(GtkWidget *gdialog,
                           const std::string &default_path)
{
    if(!default_path.empty()) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(gdialog),default_path.c_str());
    }
}
    
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
    
static void AddFilters(GtkWidget *gdialog,
                       const std::vector<OpenFileDialog::Filter> &filters)
{
    for(const OpenFileDialog::Filter &filter:filters) {
        GtkFileFilter *gfilter=gtk_file_filter_new();

        std::string name=filter.title+" (";
        for(size_t i=0;i<filter.extensions.size();++i) {
            if(i>0) {
                name+="; ";
            }
            name+="*"+filter.extensions[i];
        }
        name+=")";

        gtk_file_filter_set_name(gfilter,name.c_str());

        for(const std::string &extension:filter.extensions) {
            gtk_file_filter_add_pattern(gfilter,("*"+extension).c_str());
        }

        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(gdialog),gfilter);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OpenFileDialogGTK(const std::vector<OpenFileDialog::Filter> &filters,
                              const std::string &default_path)
{
    GtkWidget *gdialog=CreateFileDialog("Open File",
                                        GTK_FILE_CHOOSER_ACTION_OPEN);

    AddFilters(gdialog,filters);
    SetDefaultPath(gdialog,default_path);
    
    return RunFileDialog(gdialog);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialogGTK(const std::vector<OpenFileDialog::Filter> &filters,
                              const std::string &default_path)
{
    GtkWidget *gdialog=CreateFileDialog("Open File",
                                        GTK_FILE_CHOOSER_ACTION_OPEN);

    AddFilters(gdialog,filters);
    SetDefaultPath(gdialog,default_path);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(gdialog),TRUE);
    
    return RunFileDialog(gdialog);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SelectFolderDialogGTK(const std::string &default_path) {
    GtkWidget *gdialog=CreateFileDialog("Select Folder",
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

    SetDefaultPath(gdialog,default_path);

    return RunFileDialog(gdialog);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
