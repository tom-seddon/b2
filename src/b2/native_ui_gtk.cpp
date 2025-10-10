#include <shared/system.h>
#include "nlohmann_json_wrapper.h"
#include <shared/json.h>
#include "native_ui.h"
#include "native_ui_gtk.h"
#include <glib-2.0/glib.h>
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#include <gtk/gtk.h>
G_GNUC_END_IGNORE_DEPRECATIONS
#include "misc.h"
#include "Messages.h"
#include <SDL.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "load_save.h"
#include "b2.h"
#include <shared/debug.h>
#include "native_ui_private.h"
#include <map>
#include <shared/path.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct PersistentFileDialogData {
    std::string last_folder;
};

JSON_SERIALIZE(PersistentFileDialogData, last_folder);

static std::map<Guid, PersistentFileDialogData> g_persistent_file_dialog_data_by_guid;

bool LoadSelectorDialogPersistentDataGTK(const JSON &j, std::string *error) {
    return j.Load(&g_persistent_file_dialog_data_by_guid, error);
}

void SaveSelectorDialogPersistentDataGTK(JSON *j) {
    j->Save(g_persistent_file_dialog_data_by_guid);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// I couldn't get gtk_clipboard_set_image to work, and it's a pain
// having to faff about with the GdxPixbuf stuff anyway. So this
// shells out to xclip.
//
// As a bonus, xclip does a sort of auto-daemonize kind of thing so
// the clipped image data can live on after b2 quits.

static void RunXClip(const std::string &temp_file_path,
                     Messages *messages) {

    // Ugh
    char *argv[] = {
        (char *)"xclip",
        (char *)"-selection",
        (char *)"clipboard",
        (char *)"-target",
        (char *)"image/png",
        (char *)"-in",
        (char *)temp_file_path.c_str(),
        nullptr,
    };
    pid_t xclip_pid;
    int rc = posix_spawnp(&xclip_pid, "xclip", nullptr, nullptr, argv, environ);
    if (rc != 0) {
        messages->e.f("Failed to run xclip: %s\n", strerror(rc));
        return;
    }

    int status;
    if (waitpid(xclip_pid, &status, 0) != xclip_pid) {
        messages->e.f("xclip failed: %s\n", strerror(errno));
        return;
    }

    if (!WIFEXITED(status)) {
        messages->e.f("xclip didn't exit\n");
        return;
    }

    if (WEXITSTATUS(status) != 0) {
        messages->e.f("xclip failed with exit code %d\n", WEXITSTATUS(status));
        return;
    }
}

void SetClipboardImage(SDL_Surface *surface, Messages *messages) {
    char temp_file_path[] = "/tmp/b2_png_XXXXXX";
    int fd = mkstemp(temp_file_path);
    if (fd == -1) {
        messages->e.f("Failed to open temp file: %s\n", strerror(errno));
        return;
    }

    close(fd);
    fd = -1;

    if (SaveSDLSurface(surface, temp_file_path, messages)) {
        RunXClip(temp_file_path, messages);
    }

    unlink(temp_file_path);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static GCancellable *g_current_gcancellable = nullptr;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CloseModalDialogLocked() {
    ASSERT(!IsMainThread());
    ASSERT(g_native_ui_modal_state == NativeUiModalState_Open);

    if (!g_current_gcancellable) {
        // can't cancel! Stuck!
        return false;
    }

    g_cancellable_cancel(g_current_gcancellable);
    g_current_gcancellable = nullptr;

    // Should close in due course.
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct RunDialogData {
    // Set to true to stop the Gtk4 loop.
    bool stop = false;

    GCancellable *gcancellable = nullptr;
};

static gboolean HandleRunDialogIdle(gpointer user_data) {
    auto rdd = (RunDialogData *)user_data;

    if (!TickNoopMessageLoop()) {
        // Cancel the dialog, whichever it was. This will count as a
        // cancel of some kind, and execution will continue.
        g_cancellable_cancel(rdd->gcancellable);

        // Post another quit message so the main message loop sees it.
        SDL_Event event = {};
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
    }

    return G_SOURCE_CONTINUE;
}

static void RunDialog(RunDialogData *rdd) {
    GMainContext *gmain_context = g_main_context_default();

    {
        LockGuard lock(g_native_ui_globals_mutex);

        g_native_ui_modal_state = NativeUiModalState_Open;
        g_current_gcancellable = rdd->gcancellable;
    }

    guint id = g_idle_add(&HandleRunDialogIdle, rdd);

    while (g_main_context_pending(gmain_context)) {
        g_main_context_iteration(gmain_context, TRUE);
        if (rdd->stop) {
            break;
        }
    }

    {
        LockGuard lock(g_native_ui_globals_mutex);

        g_native_ui_modal_state = NativeUiModalState_NotOpen;
        g_current_gcancellable = nullptr;
    }

    g_source_remove(id);

    g_object_unref(rdd->gcancellable), rdd->gcancellable = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void HandleMessageBoxComplete(GObject *source_object,
                                     GAsyncResult *res,
                                     gpointer data) {
    (void)source_object;

    auto rdd = (RunDialogData *)data;

    gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source_object),
                                   res,
                                   nullptr);

    rdd->stop = true;
}

void MessageBox(const std::string &title, const std::string &text) {
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", title.c_str());

    gtk_alert_dialog_set_detail(dialog, text.c_str());

    gtk_alert_dialog_set_modal(dialog, 1);

    RunDialogData rdd;
    rdd.gcancellable = g_cancellable_new();

    gtk_alert_dialog_choose(dialog,
                            nullptr,
                            rdd.gcancellable,
                            &HandleMessageBoxComplete,
                            &rdd);

    RunDialog(&rdd);

    g_object_unref(dialog), dialog = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct RunFileDialogData : public RunDialogData {
    PersistentFileDialogData *persistent_data = nullptr;
    GtkFileDialog *gdialog = nullptr;
    GFile *gfile = nullptr;
    GError *gerror = nullptr;
};

static std::string RunFileDialog(RunFileDialogData *rfdd) {
    RunDialog(rfdd);

    if (rfdd->gerror) {
        g_error_free(rfdd->gerror), rfdd->gerror = nullptr;
    }

    std::string path;
    if (rfdd->gfile) {
        if (char *path_tmp = g_file_get_path(rfdd->gfile)) {
            path = path_tmp;
            g_free(path_tmp), path_tmp = nullptr;

            rfdd->persistent_data->last_folder = PathGetFolder(path);
        } else {
            rfdd->persistent_data->last_folder.clear();
        }

        printf("%s: last folder now: \"%s\"\n", __func__, rfdd->persistent_data->last_folder.c_str());

        g_object_unref(rfdd->gfile), rfdd->gfile = nullptr;
    }

    return path;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CreateFileDialog(RunFileDialogData *rfdd,
                             const Guid &guid,
                             const char *title,
                             const std::vector<OpenFileDialog::Filter> &filters,
                             const std::string &initial_name) {
    rfdd->persistent_data = &g_persistent_file_dialog_data_by_guid[guid];

    {
        char guid_str[GUID_STR_SIZE];
        GetStringFromGuid(guid_str, guid);

        printf("%s: %s: last folder: \"%s\"\n", __func__, guid_str, rfdd->persistent_data->last_folder.c_str());
    }

    rfdd->gdialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(rfdd->gdialog, title);

    GListStore *gfilters = g_list_store_new(GTK_TYPE_FILE_FILTER);

    for (const OpenFileDialog::Filter &filter : filters) {
        GtkFileFilter *gfilter = gtk_file_filter_new();

        std::string name = filter.title + " (";
        for (size_t i = 0; i < filter.extensions.size(); ++i) {
            if (i > 0) {
                name += "; ";
            }
            name += "*" + filter.extensions[i];
        }
        name += ")";

        gtk_file_filter_set_name(gfilter, name.c_str());

        for (const std::string &extension : filter.extensions) {
            gtk_file_filter_add_pattern(gfilter, ("*" + extension).c_str());
        }

        g_list_store_append(gfilters, gfilter);

        g_object_unref(gfilter), gfilter = nullptr;
    }

    gtk_file_dialog_set_filters(rfdd->gdialog, G_LIST_MODEL(gfilters));
    g_object_unref(gfilters), gfilters = nullptr;

    if (!rfdd->persistent_data->last_folder.empty()) {
        GFile *gdefault_folder = g_file_new_for_path(rfdd->persistent_data->last_folder.c_str());
        gtk_file_dialog_set_initial_folder(rfdd->gdialog, gdefault_folder);
        g_object_unref(gdefault_folder), gdefault_folder = nullptr;
    }

    if (!initial_name.empty()) {
        gtk_file_dialog_set_initial_name(rfdd->gdialog, initial_name.c_str());
    }

    rfdd->gcancellable = g_cancellable_new();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void HandleOpenFileComplete(GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer data) {
    (void)source_object;

    auto rfdd = (RunFileDialogData *)data;

    rfdd->gfile = gtk_file_dialog_open_finish((GtkFileDialog *)rfdd->gdialog,
                                              res,
                                              &rfdd->gerror);

    rfdd->stop = true;
}

std::string OpenFileDialogGTK(const Guid &guid,
                              const std::vector<OpenFileDialog::Filter> &filters,
                              const std::string &default_path) {
    RunFileDialogData rfdd;
    CreateFileDialog(&rfdd,
                     guid,
                     "Open File",
                     filters,
                     default_path);

    gtk_file_dialog_open(rfdd.gdialog,
                         nullptr,
                         rfdd.gcancellable,
                         &HandleOpenFileComplete,
                         &rfdd);

    std::string path = RunFileDialog(&rfdd);
    return path;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void HandleSaveFileComplete(GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer data) {
    (void)source_object;

    auto rfdd = (RunFileDialogData *)data;

    rfdd->gfile = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(rfdd->gdialog),
                                              res,
                                              &rfdd->gerror);
    rfdd->stop = true;
}

std::string SaveFileDialogGTK(const Guid &guid,
                              const std::vector<OpenFileDialog::Filter> &filters,
                              const std::string &suggested_name) {
    RunFileDialogData rfdd;
    CreateFileDialog(&rfdd,
                     guid,
                     "Save File",
                     filters,
                     suggested_name);

    gtk_file_dialog_save(rfdd.gdialog,
                         nullptr,
                         rfdd.gcancellable,
                         &HandleSaveFileComplete,
                         &rfdd);

    std::string path = RunFileDialog(&rfdd);
    return path;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
