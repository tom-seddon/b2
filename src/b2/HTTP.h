#ifndef HEADER_3B9A1A2F47DE44A7850AD05E88DE7C86 // -*- mode:c++ -*-
#define HEADER_3B9A1A2F47DE44A7850AD05E88DE7C86

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <map>
#include <vector>

// stupid X11.h crap.
#ifdef BadRequest
#undef BadRequest
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

extern const std::string HTTP_OCTET_STREAM_CONTENT_TYPE;
extern const std::string HTTP_TEXT_CONTENT_TYPE;
extern const std::string HTTP_JSON_CONTENT_TYPE;

extern const std::string HTTP_ISO_8859_1_CHARSET;
extern const std::string HTTP_UTF8_CHARSET;

extern const std::string DEFAULT_CONTENT_TYPE;

extern const std::string CONTENT_TYPE;
extern const std::string CHARSET_PREFIX;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetPercentEncoded(const std::string &str);

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

struct HTTPHeaderNameComparer {
    bool operator()(const std::string &a, const std::string &b) const;
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
    std::map<std::string, std::string, HTTPHeaderNameComparer> headers;
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

    void AddQueryParameter(std::string key, std::string value);

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

#endif
