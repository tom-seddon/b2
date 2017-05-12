#include <shared/system.h>
#include "misc.h"
#include "load_save.h"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <shared/log.h>
#include <shared/path.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SaveCocoaFrameUsingName(void *nswindow,const std::string &name) {
    if(!nswindow) {
        return;
    }

    if(name.empty()) {
        return;
    }

    NSString *nsname=[[NSString alloc] initWithUTF8String:name.c_str()];
    
    [(NSWindow *)nswindow saveFrameUsingName:nsname];
    
    [nsname release];
    nsname=nil;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SetCocoaFrameUsingName(void *nswindow,const std::string &name) {
    if(!nswindow) {
        return false;
    }

    if(name.empty()) {
        return false;
    }

    NSString *nsname=[[NSString alloc] initWithUTF8String:name.c_str()];
    
    bool result=!![(NSWindow *)nswindow setFrameUsingName:nsname];
    
    [nsname release];
    nsname=nil;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetApplicationSupportFileName(const std::string &suffix) {
    std::string fname;
    
    @autoreleasepool {
        NSFileManager *defaultManager=[NSFileManager defaultManager];
        NSError *error=nil;

        NSURL *url=[defaultManager URLForDirectory:NSApplicationSupportDirectory
                                          inDomain:NSUserDomainMask
                                 appropriateForURL:nil
                                            create:TRUE
                                             error:&error];
        if(!url) {
            return "";
        }

        const char *fileSystemRepresentation=[url fileSystemRepresentation];
        if(!fileSystemRepresentation) {
            return "";
        }

        fname=fileSystemRepresentation;

        NSBundle *mainBundle=[NSBundle mainBundle];

        NSString *bundleIdentifier=[mainBundle bundleIdentifier];

        if([bundleIdentifier length]>0) {
            fname=PathJoined(fname,[bundleIdentifier UTF8String]);
        }
    }

    fname=PathJoined(fname,suffix);

    if(!PathCreateFolder(PathGetFolder(fname))) {
        LOGF(OUTPUT,"%s: PathCreateFolder failed: %s\n",__func__,strerror(errno));
        return "";
    }

    return fname;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
