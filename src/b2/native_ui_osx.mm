#include <shared/system.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <string>
#include "native_ui_osx.h"
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageBox(const std::string &title,const std::string &text) {
    NSString *nstitle=[[NSString alloc] initWithUTF8String:title.c_str()];

    NSString *nstext=[[NSString alloc] initWithUTF8String:text.c_str()];

    NSAlert *alert=[[NSAlert alloc] init];

    [alert setInformativeText:nstext];

    [alert setMessageText:nstitle];

    [alert setAlertStyle:NSCriticalAlertStyle];

    [alert runModal];

    [alert release];
    alert=nil;

    [nstext release];
    nstext=nil;

    [nstitle release];
    nstitle=nil;
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

static void SetDefaultPath(NSSavePanel *panel,const std::string &default_path) {
    if(!default_path.empty()) {
        auto default_url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:default_path.c_str()]];
        [panel setDirectoryURL:default_url];
        [panel setNameFieldStringValue:default_url.lastPathComponent];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string RunModal(NSSavePanel *panel) {
    auto old_key_window=[NSApp keyWindow];

    std::string result;
    if([panel runModal]==NSModalResponseOK) {
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

static std::string DoFileDialogOSX(const std::vector<std::string> &types,
                                   const std::string &default_path,
                                   NSSavePanel *panel)
{
    SetDefaultPath(panel,default_path);

    if(!types.empty()) {
        NSMutableArray<NSString *> *types_array=[NSMutableArray array];
        for(const std::string &type:types) {
            [types_array addObject:[NSString stringWithUTF8String:type.c_str()]];
        }

        [panel setAllowedFileTypes:types_array];
    }

    return RunModal(panel);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string OpenFileDialogOSX(const std::vector<std::string> &types,
                              const std::string &default_path)
{
    auto pool=[[NSAutoreleasePool alloc] init];

    std::string result=DoFileDialogOSX(types,default_path,[NSOpenPanel openPanel]);

    [pool release];
    pool=nil;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SaveFileDialogOSX(const std::vector<std::string> &types,
                              const std::string &default_path)
{
    auto pool=[[NSAutoreleasePool alloc] init];

    std::string result=DoFileDialogOSX(types,default_path,[NSSavePanel savePanel]);

    [pool release];
    pool=nil;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SelectFolderDialogOSX(const std::string &default_path) {
    auto pool=[[NSAutoreleasePool alloc] init];

    auto panel=[NSOpenPanel openPanel];

    [panel setCanChooseDirectories:YES];
    [panel setCanChooseFiles:NO];

    SetDefaultPath(panel,default_path);

    std::string result=RunModal(panel);

    [pool release];
    pool=nil;

    return result;
}
