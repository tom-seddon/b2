#include <shared/system.h>
#include <http/http.h>
#include <shared/strings.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string HTTP_OCTET_STREAM_CONTENT_TYPE = "application/octet-stream";
const std::string HTTP_TEXT_CONTENT_TYPE = "text/plain";
const std::string HTTP_JSON_CONTENT_TYPE = "application/json";
const std::string HTTP_ISO_8859_1_CHARSET = "ISO-8859-1";
const std::string HTTP_UTF8_CHARSET = "utf-8";
const std::string DEFAULT_CONTENT_TYPE = HTTP_OCTET_STREAM_CONTENT_TYPE;

const std::string CONTENT_TYPE = "Content-Type";
const std::string CHARSET_PREFIX = "charset:";

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
            content_type->assign(content_type_header, p - content_type_header);

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
    return HTTPResponse("200 OK");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::BadRequest() {
    return HTTPResponse("400 Bad Request");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::BadRequest(const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    std::string message = strprintfv(fmt, v);
    va_end(v);

    return HTTPResponse("400 Bad Request", HTTP_TEXT_CONTENT_TYPE, std::move(message));
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

    return HTTPResponse("400 Bad Request", HTTP_TEXT_CONTENT_TYPE, std::move(message));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::NotFound() {
    return HTTPResponse("404 Not Found");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::NotFound(const HTTPRequest &request) {
    return HTTPResponse("404 Not Found", HTTP_TEXT_CONTENT_TYPE, request.method + " " + request.url);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::UnsupportedMediaType(const HTTPRequest &request) {
    return HTTPResponse("415 Unsupported Media Type", HTTP_TEXT_CONTENT_TYPE, request.method + " " + request.url);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::ServiceUnavailable() {
    return HTTPResponse("503 Service Unavailable");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse()
    : status("500 Internal Server Error") {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_)
    : status(std::move(status_)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string content_type, std::vector<uint8_t> content)
    : HTTPResponse("200 OK", std::move(content_type), std::move(content)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string content_type, const std::string &content)
    : HTTPResponse("200 OK", std::move(content_type), content) {
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
        if (content_type_charset == HTTP_UTF8_CHARSET || IsPrintable(this->content))
            return std::string(this->content.begin(), this->content.end());
    }

    return "(unprintable Content-Type: " + GetContentTypeHeader(this->content_type, this->content_type_charset) + ")";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPResponse::SetContentString(const std::string &content_) {
    this->content.assign(content_.begin(), content_.end());
}
