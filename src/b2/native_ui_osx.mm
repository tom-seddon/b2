#include <shared/system.h>
#include <nlohmann/json.hpp>
#include <shared/debug.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <string>
#include "native_ui.h"
#include "native_ui_osx.h"
#include <vector>
#include "load_save.h"
#include "b2.h"
#include "native_ui_private.h"
#include <set>
#include <map>
#include <shared/guid.h>
#include <shared/json.h>
#include <shared/path.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct PersistentFileDialogData {
    std::string last_folder;
};

JSON_SERIALIZE(PersistentFileDialogData, last_folder);

static std::map<Guid, PersistentFileDialogData> g_persistent_file_dialog_data_by_guid;

bool LoadSelectorDialogPersistentDataOSX(const JSON &j, std::string *error) {
    return j.Load(&g_persistent_file_dialog_data_by_guid, error);
}

void SaveSelectorDialogPersistentDataOSX(JSON *j) {
    j->Save(g_persistent_file_dialog_data_by_guid);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetClipboardImage(SDL_Surface *surface, Messages *messages) {
    size_t png_size;
    unsigned char *png = SaveSDLSurfaceToPNGData(surface, &png_size, messages);
    if (!png) {
        return;
    }

    auto data = [NSData dataWithBytesNoCopy:png
                                     length:png_size
                               freeWhenDone:YES];
    auto pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setData:data
                forType:NSPasteboardTypePNG];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CloseModalDialogLocked() {
    ASSERT(!IsMainThread());
    NSApplication *application = [NSApplication sharedApplication];

    [application abortModal];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageBox(const std::string &title, const std::string &text) {
    NSString *nstitle = [[NSString alloc] initWithUTF8String:title.c_str()];

    NSString *nstext = [[NSString alloc] initWithUTF8String:text.c_str()];

    NSAlert *alert = [[NSAlert alloc] init];

    [alert setInformativeText:nstext];

    [alert setMessageText:nstitle];

    [alert setAlertStyle:NSAlertStyleCritical];

    [alert runModal];

    [alert release];
    alert = nil;

    [nstext release];
    nstext = nil;

    [nstitle release];
    nstitle = nil;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *GetKeyWindow() {
    return [NSApp keyWindow];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetDoubleClickIntervalSeconds() {
    return [NSEvent doubleClickInterval];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string DoFileDialogOSX(const Guid &guid,
                                   const std::vector<OpenFileDialog::Filter> &filters,
                                   const std::string &name_field_value,
                                   NSSavePanel *panel) {
    PersistentFileDialogData *persistent_data = &g_persistent_file_dialog_data_by_guid[guid];

    if (!persistent_data->last_folder.empty()) {
        auto url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:persistent_data->last_folder.c_str()] isDirectory:YES];
        [panel setDirectoryURL:url];
    }

    if (!name_field_value.empty()) {
        [panel setNameFieldStringValue:[NSString stringWithUTF8String:name_field_value.c_str()]];
    }

    if (!filters.empty()) {
        std::set<std::string> extensions;
        for (const OpenFileDialog::Filter &filter : filters) {
            for (const std::string &extension : filter.extensions) {
                if (extension == ".*") {
                    // Wildcard extensions don't seem to work.
                    //
                    // Skip this one. If it was the only one, the panel will
                    // end up handling all extensions. If it wasn't, the
                    // catch-all will just get lost - which isn't ideal, but
                    // I don't know what else you can do?
                } else {
                    extensions.insert(extension);
                }
            }
        }

        // if no file types are set, the dialog allows anything.
        if (!extensions.empty()) {
            NSMutableArray<NSString *> *types = [NSMutableArray array];

            for (const OpenFileDialog::Filter &filter : filters) {
                for (const std::string &extension : filter.extensions) {
                    ASSERT(!extension.empty());
                    ASSERT(extension[0] == '.');
                    [types addObject:[NSString stringWithUTF8String:extension.substr(1).c_str()]];
                }
            }

            // This was deprecated after 10.9, so just hide the warning.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            [panel setAllowedFileTypes:types];
#pragma GCC diagnostic pop
        }
    }

    NSWindow *old_key_window = [NSApp keyWindow];

    {
        LockGuard<Mutex> lock(g_native_ui_globals_mutex);

        g_native_ui_modal_state = NativeUiModalState_Open;
    }

    NSModalResponse response = [panel runModal];

    {
        LockGuard<Mutex> lock(g_native_ui_globals_mutex);

        g_native_ui_modal_state = NativeUiModalState_NotOpen;
    }

    if (response == NSModalResponseOK) {
    }

    /* For some reason, OS X doesn't seem to do this
     * automatically, even though b2 has an app bundle with an
     * Info.plist and whatnot and otherwise seems to behave normally.
     */
    [old_key_window makeKeyWindow];

    if (response == NSModalResponseOK) {
        persistent_data->last_folder = [[[panel directoryURL] path] UTF8String];
        std::string result = [[[panel URL] path] UTF8String];
        return result;
    } else {
        return "";
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OpenFileDialogOSX(const Guid &guid,
                              const std::vector<OpenFileDialog::Filter> &filters) {
    auto pool = [[NSAutoreleasePool alloc] init];

    std::string result = DoFileDialogOSX(guid, filters, "", [NSOpenPanel openPanel]);

    [pool release], pool = nil;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialogOSX(const Guid &guid,
                              const std::vector<OpenFileDialog::Filter> &filters,
                              const std::string &suggested_name) {
    auto pool = [[NSAutoreleasePool alloc] init];

    // macOS assumes the name doesn't have an extension.
    //
    // TODO: could/should enforce this on all platforms? Looks like macOS uses
    // the first extension from the first filter?
    std::string name=PathWithoutExtension(PathGetName(suggested_name));
    
    std::string result = DoFileDialogOSX(guid, filters, name, [NSSavePanel savePanel]);

    [pool release], pool = nil;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
