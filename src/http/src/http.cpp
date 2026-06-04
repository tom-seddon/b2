#include <shared/system.h>
#include <http/http.h>
#include <shared/strings.h>
#include <shared/log.h>
#include <string.h>
#include <curl/curl.h>
#include <uv.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string HTTP_WWW_FORM_URLENCODED_CONTENT_TYPE = "application/x-www-form-urlencoded";
const std::string HTTP_OCTET_STREAM_CONTENT_TYPE = "application/octet-stream";
const std::string HTTP_TEXT_CONTENT_TYPE = "text/plain";
const std::string HTTP_JSON_CONTENT_TYPE = "application/json";
const std::string HTTP_ISO_8859_1_CHARSET = "ISO-8859-1";
const std::string HTTP_UTF8_CHARSET = "utf-8";
const std::string DEFAULT_CONTENT_TYPE = HTTP_OCTET_STREAM_CONTENT_TYPE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string CONTENT_TYPE = "Content-Type";
const std::string CHARSET_PREFIX = "charset:";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string OK_STATUS_MESSAGE = "200 OK";
const std::string BAD_REQUEST_STATUS_MESSAGE = "400 Bad Request";
const std::string NOT_FOUND_STATUS_MESSAGE = "404 Not Found";
const std::string UNSUPPORTED_MEDIA_TYPE_STATUS_MESSAGE = "415 Unsupported Media Type";
const std::string INTERNAL_SERVER_ERROR_STATUS_MESSAGE = "500 Internal Server Error";
const std::string SERVICE_UNAVAILABLE_STATUS_MESSAGE = "503 Service Unavailable";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char HEX_CHARS[] = "0123456789ABCDEF";

std::string GetPercentEncoded(const std::string &str) {
    std::string encoded;

    for (char c : str) {
        // don't think about this too hard.
        switch (c) {
        default:
            if (c >= 32 && c <= 126) {
                encoded.push_back(c);
            } else {
                [[fallthrough]];
            case ' ':
            case '!':
            case '#':
            case '$':
            case '%':
            case '&':
            case '\'':
            case '(':
            case ')':
            case '*':
            case '+':
            case ',':
            case '/':
            case ':':
            case ';':
            case '=':
            case '?':
            case '@':
            case '[':
            case ']':
                encoded.push_back('%');
                encoded.push_back(HEX_CHARS[(uint8_t)c >> 4]);
                encoded.push_back(HEX_CHARS[c & 0xf]);
            }
        }
    }

    return encoded;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// fix up error text: sort out newlines so they're always \r\n, and append \r\n if required.
static std::string GetErrorResponseBodyText(const std::string &message) {
    std::string result;

    ForEachLine(message, [&result](const std::string_view &line) -> bool {
        result += line;
        result.push_back('\r');
        result.push_back('\n');

        return true;
    });

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void GetContentType(std::string *content_type, std::string *content_type_charset, const char *content_type_header) {
    content_type->clear();
    content_type_charset->clear();

    if (content_type_header) {
        // This is a bit scrappy. I got bored trying to code it up
        // properly.

        const char *p = strchr(content_type_header, ';');
        if (!p) {
            *content_type = content_type_header;
        } else {
            content_type->assign(content_type_header,
                                 (size_t)(p - content_type_header));

            ++p;
            while (*p != 0 && (*p == ' ' || *p == '\t')) {
                ++p;
            }

            if (*p != 0) {
                if (strncmp(p, CHARSET_PREFIX.c_str(), CHARSET_PREFIX.size()) == 0) {
                    content_type_charset->assign(p + CHARSET_PREFIX.size());
                }
            }
        }
    }
}

void GetContentType(std::string *content_type, std::string *content_type_charset, const std::string *content_type_header) {
    if (!content_type_header) {
        GetContentType(content_type, content_type_charset, (const char *)nullptr);
    } else {
        GetContentType(content_type, content_type_charset, content_type_header->c_str());
    }
}

std::string GetContentTypeHeader(const std::string &content_type, const std::string &content_type_charset) {
    if (content_type.empty()) {
        return DEFAULT_CONTENT_TYPE;
    } else if (content_type_charset.empty()) {
        return content_type;
    } else {
        return content_type + " " + CHARSET_PREFIX + content_type_charset;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HTTPHeaderNameComparer::operator()(const std::string &a, const std::string &b) const {
    return strcasecmp(a.c_str(), b.c_str()) < 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPRequest::HTTPRequest(std::string url_)
    : url(std::move(url_)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPRequest::SetHeaderValue(std::string key, std::string value) {
    this->headers.insert(std::make_pair<std::string, std::string>(std::move(key), std::move(value)));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string *HTTPRequest::GetHeaderValue(const std::string &key) const {
    auto &&it = this->headers.find(key);
    if (it == this->headers.end()) {
        return nullptr;
    } else {
        return &it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPRequest::AddQueryParameter(std::string key, std::string value) {
    HTTPQueryParameter p;

    p.key = std::move(key);
    p.value = std::move(value);

    this->query.push_back(std::move(p));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::OK() {
    return HTTPResponse(OK_STATUS_MESSAGE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::BadRequest() {
    return HTTPResponse(BAD_REQUEST_STATUS_MESSAGE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::BadRequest(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    std::string message = strprintfv(fmt, v);
    va_end(v);

    return HTTPResponse(BAD_REQUEST_STATUS_MESSAGE, HTTP_TEXT_CONTENT_TYPE, GetErrorResponseBodyText(message));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::BadRequest(const HTTPRequest &request, const char *fmt, ...) {
    std::string message = "Bad Request: " + request.method + " " + request.url;

    if (fmt) {
        message += "\r\n";

        va_list v;
        va_start(v, fmt);
        message += strprintfv(fmt, v);
        va_end(v);
    }

    return HTTPResponse(BAD_REQUEST_STATUS_MESSAGE, HTTP_TEXT_CONTENT_TYPE, GetErrorResponseBodyText(message));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::NotFound() {
    return HTTPResponse(NOT_FOUND_STATUS_MESSAGE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::NotFound(const HTTPRequest &request, const char *fmt, ...) {
    std::string message = "Not Found: " + request.method + " " + request.url;

    if (fmt) {
        message += "\r\n";

        va_list v;
        va_start(v, fmt);
        message += strprintfv(fmt, v);
        va_end(v);
    }

    return HTTPResponse(NOT_FOUND_STATUS_MESSAGE, HTTP_TEXT_CONTENT_TYPE, GetErrorResponseBodyText(message));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::UnsupportedMediaType(const HTTPRequest &request) {
    return HTTPResponse(UNSUPPORTED_MEDIA_TYPE_STATUS_MESSAGE, HTTP_TEXT_CONTENT_TYPE, request.method + " " + request.url + "\r\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::InternalServerError() {
    return HTTPResponse(INTERNAL_SERVER_ERROR_STATUS_MESSAGE);
}

HTTPResponse HTTPResponse::InternalServerError(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    std::string message = strprintfv(fmt, v);
    va_end(v);

    return HTTPResponse(INTERNAL_SERVER_ERROR_STATUS_MESSAGE, HTTP_TEXT_CONTENT_TYPE, message);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::ServiceUnavailable() {
    return HTTPResponse(SERVICE_UNAVAILABLE_STATUS_MESSAGE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::ServiceUnavailable(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    std::string message = strprintfv(fmt, v);
    va_end(v);

    return HTTPResponse(SERVICE_UNAVAILABLE_STATUS_MESSAGE, HTTP_TEXT_CONTENT_TYPE, message);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse()
    : status(INTERNAL_SERVER_ERROR_STATUS_MESSAGE) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_)
    : status(std::move(status_)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string content_type, std::vector<uint8_t> content)
    : HTTPResponse(OK_STATUS_MESSAGE, std::move(content_type), std::move(content)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string content_type, const std::string &content)
    : HTTPResponse(OK_STATUS_MESSAGE, std::move(content_type), content) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_, std::string content_type_, std::vector<uint8_t> content_)
    : status(std::move(status_))
    , content(std::move(content_))
    , content_type(content_type_.empty() ? DEFAULT_CONTENT_TYPE : std::move(content_type_)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_, std::string content_type_, const std::string &content_)
    : status(std::move(status_))
    , content_type(std::move(content_type_)) {
    this->SetContentString(content_);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsPrintable(const std::vector<uint8_t> &bytes) {
    for (uint8_t byte : bytes) {
        if (byte > 127) {
            return false;
        }
    }

    return true;
}

std::string HTTPResponse::GetContentString() const {
    if (this->content_type == HTTP_JSON_CONTENT_TYPE) {
        return std::string(this->content.begin(), this->content.end());
    } else if (this->content_type == HTTP_TEXT_CONTENT_TYPE) {
        if (content_type_charset == HTTP_UTF8_CHARSET || IsPrintable(this->content)) {
            return std::string(this->content.begin(), this->content.end());
        }
    }

    return "(unprintable Content-Type: " + GetContentTypeHeader(this->content_type, this->content_type_charset) + ")";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPResponse::SetContentString(const std::string &content_) {
    this->content.assign(content_.begin(), content_.end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool InitHTTPDependencies(LogSet *logs) {
    CURLcode r = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (r != 0) {
        logs->e.f("Failed to initialise libcurl: %s\n", curl_easy_strerror(r));
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPDependencyVersions GetHTTPDependencyVersions() {
    HTTPDependencyVersions versions;

    const curl_version_info_data *curl_version = curl_version_info(CURLVERSION_NOW);
    versions.libcurl_version = curl_version->version;

    versions.libuv_version = uv_version_string();

    return versions;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
