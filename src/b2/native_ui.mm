#include <shared/system.h>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <string>

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
