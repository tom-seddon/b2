#include <shared/system.h>
#include <nlohmann/json.hpp>

#include <shared/strings.h>
#include <shared/path.h>
#include <shared/CommandLineParser.h>
#include <stdio.h>
#include <shared/system_specific.h>
#include "http.h"
#include "HTTPClient.h"
#include <shared/log.h>
#include "Messages.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// yes, this spells disk with a "k".

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(VERBOSE, "", &log_printer_stderr, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const int HTTP_SERVER_PORT = 0xbbcb;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Options {
    // b2 client request data
    std::string disks[2];
    bool in_memory_disks[2] = {};
    std::string config;
    std::string window_name;
    bool boot = false;

    // b2 path stuff
    std::string b2_path;

    // b2client stuff
    bool help = false;
    bool verbose = false;
    bool launch = false;
};

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

static bool HandleCommandLineOptions(Options *options, int argc, char *argv[]) {
    std::vector<std::string> argv_utf8;
    for (int i = 0; i < argc; ++i) {
        argv_utf8.push_back(GetUTF8StringFromThreadACPString(argv[i]));
    }

    // Make a guess as to default b2 exe.
    {
        std::string b2client_folder = PathGetFolder(PathGetEXEFileName());
        for (const char *b2_name : B2_BINARY_NAMES) {
            std::string b2_path = PathJoined(b2client_folder, b2_name);
            if (PathIsFileOnDisk(b2_path, nullptr, nullptr)) {
                options->b2_path = b2_path;
                break;
            }
        }

        // If not found, there'll be an error if it turns out later to be
        // required.
    }

    CommandLineParser p("b2client");

    for (int i = 0; i < 2; ++i) {
        p.AddOption(0, strprintf("--%d", i)).Arg(&options->disks[i]).Meta("FILE").Help("load disk image from FILE into drive " + std::to_string(i));
        p.AddOption(0, strprintf("%d-in-memory", i)).SetIfPresent(&options->in_memory_disks[i]).Help(strprintf("if -%d specified as well, use in-memory disk image", i));
    }

    p.AddOption(0, "config").Arg(&options->config).Meta("CONFIG").Help("power-on reset with config CONFIG");

    p.AddOption(0, "boot").SetIfPresent(&options->boot).Help("auto-boot BBC");

    p.AddOption(0, "verbose").SetIfPresent(&options->verbose).Help("be more verbose");

    // compared to using curl directly, the b2client command line is slightly
    // more convenient in this case.
    //p.AddOption(0, "launch").ResetIfPresent(&options->launch).Help("if there's no b2 running, launch a new copy");
    //p.AddOption(0, "b2-path").Arg(&options->b2_path).Meta("PATH").Help("use PATH as b2 executable").ShowDefault();

    p.AddOption(0, "window").Meta("TITLE").Arg(&options->window_name).Help("use b2 window TITLE, or the most recently used if not specified");

    p.AddHelpOption(&options->help);

    if (!p.Parse(argv_utf8)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static ClientRequest GetClientRequest(const Options &options) {
//    ClientRequest crequest;
//
//    crequest.window_name = options.window_name;
//    crequest.config_name = options.config;
//    for (int i = 0; i < 2; ++i) {
//        crequest.drive_files.push_back(options.disks[i]);
//        crequest.drive_in_memory.push_back(options.in_memory_disks[i]);
//    }
//
//    return crequest;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    Options options;
    if (!HandleCommandLineOptions(&options, argc, argv)) {
        return 1;
    } else if (options.help) {
        return 0;
    }

    LOG(VERBOSE).enabled = options.verbose;
    Messages messages(Messages::STDIO);

    //ClientRequest crequest = GetClientRequest(options);

    //std::string crequest_json = nlohmann::json(crequest).dump(4);

    //std::unique_ptr<HTTPClient> client = CreateHTTPClient();
    //client->SetVerbose(options.verbose);
    //client->SetMessages(&messages);

    //HTTPRequest request;
    //request.url = "http://127.0.0.1:" + std::to_string(HTTP_SERVER_PORT) + "/" + CLIENT_PATH;
    //request.content_type = HTTP_JSON_CONTENT_TYPE;
    //request.content_type_charset = HTTP_UTF8_CHARSET;
    //request.body.assign(crequest_json.begin(), crequest_json.end());

    //HTTPResponse response;
    //int status = client->SendRequest(request, &response);
    //if (status == 200) {
    //    return 0;
    //} else if (status >= 0) {
    //    fprintf(stderr, "b2client: got error status %d from running copy of b2: %s\n", status, response.GetContentString().c_str());
    //    return 1;
    //} else if (status < 0) {
    //    fprintf(stderr, "b2client: HTTP request failed\n");
    //    return 1;
    //}

    return 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
