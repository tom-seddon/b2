#ifndef HEADER_1D01A764189346A683C7422A889709E4 // -*- mode:c++ -*-
#define HEADER_1D01A764189346A683C7422A889709E4

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#include "conf.h"

#include <memory>
#include <string>
#include <map>
#include <vector>

class Messages;
class HTTPServer;

// stupid X11.h crap.
#ifdef BadRequest
#undef BadRequest
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const std::string HTTP_OCTET_STREAM_CONTENT_TYPE;
extern const std::string HTTP_TEXT_CONTENT_TYPE;

extern const std::string HTTP_ISO_8859_1_CHARSET;
extern const std::string HTTP_UTF8_CHARSET;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct HTTPQueryParameter {
    std::string key, value;
};

// What a crap name... this is the minimum amount of stuff from a
// response necessary to send a response to it.
//
// (This doesn't make much sense inside the server's code. But when
// deferring a reply it means the handler doesn't have to store off
// the entire HTTPRequest (strings, vectors, map...). Just this little
// struct is enough.)
struct HTTPResponseData {
    uint64_t connection_id = 0;
    bool dump = false;
};

// Client ignores response_data.
//
// Client ignores url_path, which must be empty. url is the URL to use.
//
// Client appends query parameters from query array to URL to use. First, if the
// URL contains a '?', a '&' is appended; otherwise, a '?' is appended. Then the
// query parameters, separated by '&', percent-encoded. Caller must ensure the
// result of this will be valid.
class HTTPRequest {
  public:
    HTTPResponseData response_data;
    std::map<std::string, std::string> headers;
    std::string url;
    std::string url_path;
    std::vector<HTTPQueryParameter> query;
    std::string content_type, content_type_charset;
    std::vector<uint8_t> body;
    std::string method; // if empty, method is GET.

    HTTPRequest() = default;
    explicit HTTPRequest(std::string url);

    void SetHeaderValue(std::string key, std::string value);
    const std::string *GetHeaderValue(const std::string &key) const;

    void AddQueryParameter(std::string key,std::string value);

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Client only fills in content_vec.
class HTTPResponse {
  public:
    std::string status;

    std::vector<uint8_t> content_vec;
    std::string content_str;

    // if content is non-empty but content_type is "", the assumed
    // content type is application/octet-stream.
    std::string content_type;

    static HTTPResponse OK();
    static HTTPResponse BadRequest();
    static HTTPResponse BadRequest(const char *fmt, ...) PRINTF_LIKE(1, 2);
    static HTTPResponse BadRequest(const HTTPRequest &request, const char *fmt = nullptr, ...) PRINTF_LIKE(2, 3);
    static HTTPResponse NotFound();
    static HTTPResponse NotFound(const HTTPRequest &request);
    static HTTPResponse UnsupportedMediaType(const HTTPRequest &request);
    static HTTPResponse ServiceUnavailable();

    // A default-constructed HTTPResponse has a status of 500 Internal
    // Server Error.
    HTTPResponse();

    // An HTTPResponse constructed with a status string is assumed to
    // be an error.
    explicit HTTPResponse(std::string status);

    // An HTTPResponse with content and no status implicitly has a
    // status of 200 OK.
    HTTPResponse(std::string content_type, std::vector<uint8_t> content);
    HTTPResponse(std::string content_type, std::string content);

    HTTPResponse(std::string status, std::string content_type, std::vector<uint8_t> content);
    HTTPResponse(std::string status, std::string content_type, std::string content);

  protected:
  private:
};

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

    virtual bool Start(int port, bool listen_on_all_interfaces, Messages *messages) = 0;

    // Fairly useless flag, suitable only for polling, used by the tests.
    virtual bool IsServerListening() const = 0;

    virtual void SetHandler(std::shared_ptr<HTTPHandler> handler) = 0;

    void SendResponse(const HTTPRequest &request, HTTPResponse response);
    virtual void SendResponse(const HTTPResponseData &response_data, HTTPResponse response) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPClient {
  public:
    HTTPClient();
    virtual ~HTTPClient();

    HTTPClient(const HTTPClient &) = delete;
    HTTPClient &operator=(const HTTPClient &) = delete;

    HTTPClient(HTTPClient &&) = delete;
    HTTPClient &operator=(HTTPClient &&) = delete;

    virtual void SetMessages(Messages *messages) = 0;
    virtual void SetVerbose(bool verbose) = 0;

    virtual void AddDefaultHeader(const char *key, const char *value) = 0;

    virtual int SendRequest(const HTTPRequest &request, HTTPResponse *response) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPServer> CreateHTTPServer();
std::unique_ptr<HTTPClient> CreateHTTPClient();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
