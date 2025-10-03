#include <shared/system.h>
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

void MessageBox(const std::string &title, const std::string &text) {
    // Get the main application window as parent
    GtkWindow *parent = nullptr;
    GList *toplevels = gtk_window_list_toplevels();
    if (toplevels) {
        for (GList *iter = toplevels; iter; iter = iter->next) {
            GtkWidget *window = GTK_WIDGET(iter->data);
            if (gtk_widget_get_visible(window) && GTK_IS_WINDOW(window)) {
                parent = GTK_WINDOW(window);
                break;
            }
        }
        g_list_free(toplevels);
    }

    GtkAlertDialog *alert = gtk_alert_dialog_new(title.c_str());
    gtk_alert_dialog_set_detail(alert, text.c_str());
    gtk_alert_dialog_set_modal(alert, TRUE);

    // Show the alert dialog
    gtk_alert_dialog_show(alert, parent);

    // Clean up
    g_object_unref(alert);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void (*g_trace_save_callback)(const std::string& path) = nullptr;

// For std::function callbacks
static std::function<void(const std::string&)> g_std_function_callback;

// Dialog operation types
enum class DialogOperation {
    Save,
    Open,
    SelectFolder
};

static DialogOperation g_current_dialog_operation = DialogOperation::Save;

// Function to process GTK events (to be called from main SDL loop)
void ProcessGTKEvents() {
    // Process any pending GTK events without blocking
    while (g_main_context_pending(g_main_context_default())) {
        g_main_context_iteration(g_main_context_default(), FALSE);
    }
}

// Helper function to create GTK4 file filters from our filter format
static GListModel* CreateGTK4Filters(const std::vector<OpenFileDialog::Filter> &filters) {
    if (filters.empty()) {
        return nullptr;
    }

    GListStore *store = g_list_store_new(GTK_TYPE_FILE_FILTER);

    for (const OpenFileDialog::Filter &filter : filters) {
        GtkFileFilter *gfilter = gtk_file_filter_new();

        // Set the filter name
        std::string name = filter.title + " (";
        for (size_t i = 0; i < filter.extensions.size(); ++i) {
            if (i > 0) {
                name += "; ";
            }
            name += "*" + filter.extensions[i];
        }
        name += ")";
        gtk_file_filter_set_name(gfilter, name.c_str());

        // Add patterns for each extension
        for (const std::string &extension : filter.extensions) {
            if (extension == ".*") {
                // Special case for "all files" filter
                gtk_file_filter_add_pattern(gfilter, "*");
            } else {
                gtk_file_filter_add_pattern(gfilter, ("*" + extension).c_str());
            }
        }

        g_list_store_append(store, gfilter);
        g_object_unref(gfilter); // The store takes ownership
    }

    return G_LIST_MODEL(store);
}


// Callback for GTK4 async file dialog
static void async_file_dialog_response_callback(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)user_data; // Suppress unused parameter warning
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = nullptr;

    // Use the correct finish function based on the operation type
    GFile *file = nullptr;
    switch (g_current_dialog_operation) {
        case DialogOperation::Save:
            file = gtk_file_dialog_save_finish(dialog, res, &error);
            break;
        case DialogOperation::Open:
            file = gtk_file_dialog_open_finish(dialog, res, &error);
            break;
        case DialogOperation::SelectFolder:
            file = gtk_file_dialog_select_folder_finish(dialog, res, &error);
            break;
    }

    if (error) {
        g_error_free(error);
        if (g_trace_save_callback) {
            g_trace_save_callback(""); // Empty path indicates error/cancellation
        }
        if (g_std_function_callback) {
            g_std_function_callback(""); // Empty path indicates error/cancellation
        }
    } else if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            if (g_trace_save_callback) {
                g_trace_save_callback(std::string(path));
            }
            if (g_std_function_callback) {
                g_std_function_callback(std::string(path));
            }
            g_free(path);
        }
        g_object_unref(file);
    } else {
        if (g_trace_save_callback) {
            g_trace_save_callback(""); // Empty path indicates cancellation
        }
        if (g_std_function_callback) {
            g_std_function_callback(""); // Empty path indicates cancellation
        }
    }

    // Clean up the dialog now that the async operation is complete
    g_object_unref(dialog);
}


// Async function for Open File Dialog
void OpenFileDialogGTKAsync(const std::vector<OpenFileDialog::Filter> &filters,
                           const std::string &default_path,
                           std::function<void(const std::string&)> callback) {

    // Get the main application window as parent
    GtkWindow *parent = nullptr;
    GList *toplevels = gtk_window_list_toplevels();
    if (toplevels) {
        for (GList *iter = toplevels; iter; iter = iter->next) {
            GtkWidget *window = GTK_WIDGET(iter->data);
            if (gtk_widget_get_visible(window) && GTK_IS_WINDOW(window)) {
                parent = GTK_WINDOW(window);
                break;
            }
        }
        g_list_free(toplevels);
    }

    // Create GTK4 native file dialog
    GtkFileDialog *file_dialog = gtk_file_dialog_new();

    // Set dialog properties
    gtk_file_dialog_set_title(file_dialog, "Open File");

    // Apply file filters if provided
    GListModel *gtk_filters = CreateGTK4Filters(filters);
    if (gtk_filters) {
        gtk_file_dialog_set_filters(file_dialog, gtk_filters);
        g_object_unref(gtk_filters); // Clean up the list model
    }

    // Set initial folder if provided (extract directory from file path)
    if (!default_path.empty()) {
        std::string initial_folder_path = default_path;
        size_t last_slash = default_path.find_last_of('/');
        if (last_slash != std::string::npos && last_slash > 0) {
            initial_folder_path = default_path.substr(0, last_slash);
        }

        GFile *initial_folder = g_file_new_for_path(initial_folder_path.c_str());
        gtk_file_dialog_set_initial_folder(file_dialog, initial_folder);
        g_object_unref(initial_folder);
    }

    // Store the callback for this operation
    g_std_function_callback = callback;
    g_current_dialog_operation = DialogOperation::Open;

    // Start the async file dialog (non-blocking)
    gtk_file_dialog_open(file_dialog, parent, nullptr, async_file_dialog_response_callback, nullptr);

    // Don't unref the dialog - it needs to stay alive for the async operation
    // The dialog will be cleaned up in the callback
}

// Async function for Select Folder Dialog
void SelectFolderDialogGTKAsync(const std::string &default_path,
                               void (*callback)(const std::string& path)) {

    // Get the main application window as parent
    GtkWindow *parent = nullptr;
    GList *toplevels = gtk_window_list_toplevels();
    if (toplevels) {
        for (GList *iter = toplevels; iter; iter = iter->next) {
            GtkWidget *window = GTK_WIDGET(iter->data);
            if (gtk_widget_get_visible(window) && GTK_IS_WINDOW(window)) {
                parent = GTK_WINDOW(window);
                break;
            }
        }
        g_list_free(toplevels);
    }

    // Create GTK4 native file dialog
    GtkFileDialog *file_dialog = gtk_file_dialog_new();

    // Set dialog properties
    gtk_file_dialog_set_title(file_dialog, "Select Folder");

    // Set initial folder if provided
    if (!default_path.empty()) {
        GFile *initial_folder = g_file_new_for_path(default_path.c_str());
        gtk_file_dialog_set_initial_folder(file_dialog, initial_folder);
        g_object_unref(initial_folder);
    }

    // Set the callback and operation type for this operation
    g_trace_save_callback = callback;
    g_current_dialog_operation = DialogOperation::SelectFolder;

    // Start the async folder selection dialog (non-blocking)
    gtk_file_dialog_select_folder(file_dialog, parent, nullptr, async_file_dialog_response_callback, nullptr);

    // Don't unref the dialog - it needs to stay alive for the async operation
    // The dialog will be cleaned up in the callback
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// GTK-specific async implementation for SaveFileDialog
void SaveFileDialogGTKAsync(const std::vector<OpenFileDialog::Filter> &filters,
                           const std::string &default_path,
                           std::function<void(const std::string&)> callback) {

    // Get the main application window as parent
    GtkWindow *parent = nullptr;
    GList *toplevels = gtk_window_list_toplevels();
    if (toplevels) {
        for (GList *iter = toplevels; iter; iter = iter->next) {
            GtkWidget *window = GTK_WIDGET(iter->data);
            if (gtk_widget_get_visible(window) && GTK_IS_WINDOW(window)) {
                parent = GTK_WINDOW(window);
                break;
            }
        }
        g_list_free(toplevels);
    }

    // Create GTK4 native file dialog
    GtkFileDialog *file_dialog = gtk_file_dialog_new();

    // Set dialog properties
    gtk_file_dialog_set_title(file_dialog, "Save File");

    // Apply file filters if provided
    GListModel *gtk_filters = CreateGTK4Filters(filters);
    if (gtk_filters) {
        gtk_file_dialog_set_filters(file_dialog, gtk_filters);
        g_object_unref(gtk_filters); // Clean up the list model
    }

    // Set initial folder if provided (extract directory from file path)
    if (!default_path.empty()) {
        std::string initial_folder_path = default_path;
        size_t last_slash = default_path.find_last_of('/');
        if (last_slash != std::string::npos && last_slash > 0) {
            initial_folder_path = default_path.substr(0, last_slash);
        }

        GFile *initial_folder = g_file_new_for_path(initial_folder_path.c_str());
        gtk_file_dialog_set_initial_folder(file_dialog, initial_folder);
        g_object_unref(initial_folder);
    }

    // Store the callback for this operation
    g_std_function_callback = callback;
    g_current_dialog_operation = DialogOperation::Save;

    // Start the async file dialog (non-blocking)
    gtk_file_dialog_save(file_dialog, parent, nullptr, async_file_dialog_response_callback, nullptr);

}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
