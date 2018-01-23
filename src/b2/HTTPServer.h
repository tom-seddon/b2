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

class HTTPRequest {
public:
    HTTPRequest();
    virtual ~HTTPRequest()=0;

    virtual const std::string *GetHeaderValue(const std::string &key) const=0;

    virtual const std::vector<uint8_t> &GetBody() const=0;

    virtual const std::string &GetMethod() const=0;

    virtual const std::string &GetURL() const=0;

    virtual const std::string &GetURLPath() const=0;

    bool IsPOST() const;
    bool IsGET() const;

    void Send200();
    void Send400(const std::string &elaboration=std::string());
    void Send404(const std::string &elaboration=std::string());
    void Send500();

    virtual void SendResponse(HTTPResponse response)=0;
protected:
    HTTPRequest(const HTTPRequest &)=default;
    HTTPRequest &operator=(const HTTPRequest &)=default;

    HTTPRequest(HTTPRequest &&)=default;
    HTTPRequest &operator=(HTTPRequest &&)=default;
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
