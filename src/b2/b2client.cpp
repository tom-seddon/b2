#include <shared/system.h>
#include <nlohmann/json.hpp>

#include <shared/strings.h>
#include <shared/path.h>
#include <shared/CommandLineParser.h>
#include <stdio.h>
#include <shared/system_specific.h>
#include "b2client_http_api.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// yes, this spells disk with a "k".

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    std::string disks[2];
    bool direct_disks[2];
    std::string config;

    std::string b2_path;

    bool help = false;
    bool verbose = false;
    bool maybe_run_new_instance = true;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool ParseCommandLineOptions(Options *options, int argc, char *argv[]) {
    std::vector<std::string> argv_utf8;
    for (int i = 0; i < argc; ++i) {
        argv_utf8.push_back(GetUTF8StringFromThreadACPString(argv[i]));
    }

    CommandLineParser p("b2client");

    for (int i = 0; i < 2; ++i) {
        p.AddOption((char)('0' + i)).Arg(&options->disks[i]).Meta("FILE").Help("load in-memory disk image from FILE into drive " + std::to_string(i));
        p.AddOption(0, strprintf("%d-direct", i)).SetIfPresent(&options->direct_disks[i]).Help(strprintf("if -%d specified as well, use disk image rather than in-memory disk image", i));
    }

    p.AddOption('c', "config").Arg(&options->config).Meta("CONFIG").Help("reset with config CONFIG");

    p.AddOption(0, "b2-path").Arg(&options->b2_path).Meta("PATH").Help("use PATH as b2 executable").ShowDefault();

    p.AddOption('v', "verbose").SetIfPresent(&options->verbose).Help("be more verbose");

    // compared to using curl directly, the b2client command line is slightly
    // more convenient in this case.
    p.AddOption(0, "no-new-instance").ResetIfPresent(&options->maybe_run_new_instance).Help("if there's no b2 running, fail. Don't run a new copy");

    p.AddHelpOption(&options->help);

    if (!p.Parse(argv_utf8)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *const B2_BINARY_NAMES[] = {
#if SYSTEM_WINDOWS

    // If running from the build folder, it'll have to find the adjacent
    // executable, which is called b2.exe; if running from the distribution zip,
    // it'll want to find b2_Debug by preference. So try both.
    "b2_Debug.exe",
    "b2.exe",

#elif SYSTEM_OSX

    // The debug executable is called b2.
    "b2",

#elif SYSTEM_LINUX

    // Similar arrangement as on Windows: one case for running from the build
    // folder, another for running after make install.
    "b2-debug",
    "b2",

#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    Options options;

    // Make a guess as to default b2 exe.
    {
        std::string b2client_folder = PathGetFolder(PathGetEXEFileName());
        for (const char *b2_name : B2_BINARY_NAMES) {
            std::string b2_path = PathJoined(b2client_folder, b2_name);
            if (PathIsFileOnDisk(b2_path, nullptr, nullptr)) {
                options.b2_path = b2_path;
                break;
            }
        }

        // If not found, the options processing will moan if not specified.
    }

    if (!ParseCommandLineOptions(&options, argc, argv)) {
        return 1;
    }

    if (options.help) {
        return 0;
    }

    std::string url = "http://127.0.0.1:" + std::to_string(HTTP_SERVER_PORT) + "/" + CLIENT_PATH;

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
