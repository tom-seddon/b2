#ifndef HEADER_1D01A764189346A683C7422A889709E4// -*- mode:c++ -*-
#define HEADER_1D01A764189346A683C7422A889709E4

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#if HTTP_SERVER

#include <memory>
#include <string>
#include <map>
#include <vector>

class MessageList;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct HTTPQueryParameter {
    std::string key,value;
};

struct HTTPRequest {
    std::map<std::string,std::string> headers;
    std::string url,url_path,url_fragment;
    std::vector<HTTPQueryParameter> query;
    std::vector<uint8_t> body;
    std::string method;
};

struct HTTPResponse {
    std::string status;
    std::map<std::string,std::string> headers;

    // If body_data's length is 0, body_str will be considered
    // instead. Just use whichever is easiest to construct.
    std::vector<uint8_t> body_data;
    std::string body_str;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool StartHTTPServer(int port);

// will block until HTTP server is definitely stopped.
void StopHTTPServer();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
