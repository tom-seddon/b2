#ifndef HEADER_1D01A764189346A683C7422A889709E4 // -*- mode:c++ -*-
#define HEADER_1D01A764189346A683C7422A889709E4

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#include "conf.h"

#include <memory>

class Messages;
class HTTPServer;
class HTTPRequest;
class HTTPResponse;
struct HTTPResponseData;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPHandler : public std::enable_shared_from_this<HTTPHandler> {
  public:
    virtual ~HTTPHandler() = 0;

    virtual bool ThreadHandleRequest(HTTPResponse *response, HTTPServer *server, HTTPRequest &&request) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPServer {
  public:
    HTTPServer();

    // blocks until HTTP server definitely stopped.
    virtual ~HTTPServer() = 0;

    HTTPServer(const HTTPServer &) = delete;
    HTTPServer &operator=(const HTTPServer &) = delete;

    HTTPServer(HTTPServer &&) = delete;
    HTTPServer &operator=(HTTPServer &&) = delete;

    virtual bool Start(int port, Messages *messages) = 0;

    virtual void SetHandler(std::shared_ptr<HTTPHandler> handler) = 0;

    void SendResponse(const HTTPRequest &request, HTTPResponse response);
    virtual void SendResponse(const HTTPResponseData &response_data, HTTPResponse response) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPServer> CreateHTTPServer();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
