#include <shared/system.h>
#include <shared/debug.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <string>
#include "native_ui.h"
#include "native_ui_osx.h"
#include <vector>
#include "load_save.h"

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

static void SetDefaultPath(NSSavePanel *panel, const std::string &default_path) {
    if (!default_path.empty()) {
        auto default_url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:default_path.c_str()]];
        [panel setDirectoryURL:[default_url URLByDeletingLastPathComponent]];
        [panel setNameFieldStringValue:default_url.lastPathComponent];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string RunModal(NSSavePanel *panel) {
    auto old_key_window = [NSApp keyWindow];

    std::string result;
    if ([panel runModal] == NSModalResponseOK) {
        result.assign([[[panel URL] path] UTF8String]);
    }

    /* For some reason, OS X doesn't seem to do this
     * automatically, even though b2 has an app bundle with an
     * Info.plist and whatnot and otherwise seems to behave normally.
     */
    [old_key_window makeKeyWindow];

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string DoFileDialogOSX(const std::vector<OpenFileDialog::Filter> &filters,
                                   const std::string &default_path,
                                   NSSavePanel *panel) {
    SetDefaultPath(panel, default_path);

    if (!filters.empty()) {
        // Special handling for ".*". The OS X dialog doesn't work quite like
        // the Windows one.
        bool all_files = false;
        for (const OpenFileDialog::Filter &filter : filters) {
            for (const std::string &extension : filter.extensions) {
                if (extension == ".*") {
                    all_files = true;
                    break;
                }
            }
        }

        if (all_files) {
            // This is the default, so do nothing.
        } else {
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

    return RunModal(panel);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OpenFileDialogOSX(const std::vector<OpenFileDialog::Filter> &filters,
                              const std::string &default_path) {
    auto pool = [[NSAutoreleasePool alloc] init];

    std::string result = DoFileDialogOSX(filters, default_path, [NSOpenPanel openPanel]);

    [pool release];
    pool = nil;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialogOSX(const std::vector<OpenFileDialog::Filter> &filters,
                              const std::string &default_path) {
    auto pool = [[NSAutoreleasePool alloc] init];

    std::string result = DoFileDialogOSX(filters, default_path, [NSSavePanel savePanel]);

    [pool release];
    pool = nil;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SelectFolderDialogOSX(const std::string &default_path) {
    auto pool = [[NSAutoreleasePool alloc] init];

    auto panel = [NSOpenPanel openPanel];

    [panel setCanChooseDirectories:YES];
    [panel setCanChooseFiles:NO];

    SetDefaultPath(panel, default_path);

    std::string result = RunModal(panel);

    [pool release];
    pool = nil;

    return result;
}
