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

class HTTPRequest {
public:
    std::map<std::string,std::string> headers;
    std::string url;
    std::string url_path;
    std::string url_fragment;
    std::vector<HTTPQueryParameter> query;
    std::vector<uint8_t> body;
    std::string method;

    HTTPRequest()=default;

    const std::string *GetHeaderValue(const std::string &key) const;

    bool IsPOST() const;
    bool IsGET() const;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPResponse {
public:
    std::string status;

    std::vector<uint8_t> content_vec;
    std::string content_str;

    // if content is non-empty but content_type is "", the assumed
    // content type is application/octet-stream.
    std::string content_type;

    // if content is empty and elaboration is not "", the elaboration
    // is included in the error message
    //std::string elaboration;

    static HTTPResponse OK();
    static HTTPResponse BadRequest(const char *fmt,...) PRINTF_LIKE(1,2);

    // A default-constructed HTTPResponse has a status of 500 Internal
    // Server Error.
    HTTPResponse();

    // An HTTPResponse constructed with a status string is assumed to
    // be an error.
    explicit HTTPResponse(std::string status);

    // An HTTPResponse with content and no status implicitly has a
    // status of 200 OK.
    HTTPResponse(std::string content_type,std::vector<uint8_t> content);
    HTTPResponse(std::string content_type,std::string content);

    HTTPResponse(std::string status,std::string content_type,std::vector<uint8_t> content);
    HTTPResponse(std::string status,std::string content_type,std::string content);
protected:
private:
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
