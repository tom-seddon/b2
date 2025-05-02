#include <shared/system.h>
#include "HTTPServer.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/path.h>
#include <uv.h>
#include <llhttp.h>
#include <shared/mutex.h>
//#include "misc.h"
#include <set>
#include <shared/debug.h>
#include <map>
#include <string>
#include "Messages.h"
#include <inttypes.h>
#include <curl/curl.h>
#include <string.h>
#include <thread>
#include "HTTP.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(HTTPSV, "http", "HTTPSV", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string CONTENT_LENGTH = "Content-Length";
static const std::string HOST = "Host";
static const std::string UNKNOWN_HOST = "unknown-host";
static const std::string DUMP = "b2Dump";
static const std::string EXPECT = "Expect";
static const std::string EXPECT_CONTINUE = "100-continue";
static const std::string CONTINUE_RESPONSE = "100 Continue\r\n";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CURLUDeleter {
    void operator()(CURLU *curlu) const {
        curl_url_cleanup(curlu);
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void PRINTF_LIKE(3, 4) PrintLibUVError(Log *log, int rc, const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    log->v(fmt, v);
    va_end(v);

    log->f(": %s (%s)\n", uv_strerror(rc), uv_err_name(rc));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetEscaped(std::string str) {
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        if (c == '<') {
            result += "&lt;";
        } else if (c == '>') {
            result += "&gt;";
        } else if (c == '&') {
            result += "&amp;";
        } else if (c == '"') {
            result += "&quot;";
        } else {
            result.append(1, c);
        }
    }

    return result;
}

static HTTPResponse
CreateErrorResponse(const HTTPRequest &request,
                    std::string status) {
    std::string body;

    //HTTPResponse r;

    //r.co
    //r.headers["Content-Type"]="text/html";

    //r.body_str.clear();

    body += "<html>";
    body += "<head><title>" + status + "</title></head>";
    body += "<body>";
    body += "<h1>" + status + "</h1>";
    body += "<p><b>URL:</b> " + GetEscaped(request.url) + "</p>";
    body += "<p><b>Method:</b> " + request.method + "</p>";
    //if(!info.empty()) {
    //    r.body_str+="<p><b>Extra info:</b> "+GetEscaped(info)+"</p>";
    //}
    body += "</body>";
    body += "</html>";

    return HTTPResponse(std::move(status), "text/html", std::move(body));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class ContType>
static std::vector<uv_buf_t> GetBufs(ContType *cont) {
    size_t num_left = cont->size() * sizeof *cont->data();
    auto p = const_cast<char *>(reinterpret_cast<const char *>(cont->data()));

    std::vector<uv_buf_t> bufs;

    while (num_left > 0) {
        size_t n = num_left;
        if (n > UINT_MAX) {
            n = UINT_MAX;
        }

        bufs.push_back(uv_buf_init(p, (unsigned)n));

        p += n;
        num_left -= n;
    }

    return bufs;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
static void ScalarDeleteCloseCallback(uv_handle_t *handle) {
    delete (T *)handle;
}

//static bool GetPercentDecodedURLPart(std::string *result, const http_parser_url &url, http_parser_url_fields field, const std::string &url_str, const char *part_name) {
//    if (url.field_set & 1 << field) {
//        if (!GetPercentDecoded(result, url_str, url.field_data[field].off, url.field_data[field].len)) {
//            LOGF(HTTPSV, "invalid percent-encoded %s in URL: %s\n", part_name, url_str.c_str());
//            return false;
//        }
//    }
//
//    return true;
//}

static bool GetPercentDecodedURLPart(std::string *result, CURLU *url, CURLUPart part, const std::string &url_str, const char *part_name) {
    char *result_tmp;
    int rc = curl_url_get(url, part, &result_tmp, CURLU_URLDECODE);
    if (rc != 0) {
        LOGF(HTTPSV, "invalid percent-encoded %s in URL: %s\n", part_name, url_str.c_str());
        curl_free(result_tmp);
        return false;
    }

    result->assign(result_tmp);
    curl_free(result_tmp);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPHandler::~HTTPHandler() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPServer::HTTPServer() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPServer::~HTTPServer() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServer::SendResponse(const HTTPRequest &request, HTTPResponse response) {
    this->SendResponse(request.response_data, std::move(response));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPServerImpl : public HTTPServer {
  public:
    HTTPServerImpl();
    ~HTTPServerImpl();

    bool Start(int port, Messages *messages) override;
    void SetHandler(std::shared_ptr<HTTPHandler> handler) override;
    void SendResponse(const HTTPResponseData &response_data, HTTPResponse response) override;

  protected:
  private:
    struct Connection {
        uint64_t id = 0;
        HTTPServerImpl *server = nullptr;
        uv_tcp_t tcp = {};
        llhttp_t parser = {};
        llhttp_settings_t parser_settings = {};

        std::string key;
        std::string *value = nullptr;

        // Should this be std::vector<HTTPRequest>, or some similar equivalent?
        // My notes say yes, but I'm not so sure any more - pipelining ain't so
        // good for non-idempotent requests, and the Wikipedia article sounds a
        // not-very-positive note...
        // (https://en.wikipedia.org/wiki/HTTP_pipelining)
        HTTPRequest request;

        bool keep_alive = false;

        bool interim_response = false;
        std::string response_status;
        std::string response_prefix;
        std::vector<uint8_t> response_body_data;
        std::string response_body_str;
        uv_write_t write_response_req = {};

        size_t num_read = 0;
        char read_buf[100];
    };

    struct ThreadData {
        uv_tcp_t listen_tcp{};
        uint64_t next_connection_id = 1;
        std::map<uint64_t, Connection *> connection_by_id;
    };

    struct SharedData {
        uv_loop_t loop{};
        std::shared_ptr<HTTPHandler> handler;
    };

    SharedData m_sd;
    ThreadData m_td;
    std::thread m_thread;
    const uint64_t m_create_tick_count = GetCurrentTickCount();

    void ThreadMain();
    void CloseConnection(Connection *conn);
    void ResetRequest(Connection *conn);
    void StartReading(Connection *conn);
    bool StopReading(Connection *conn);
    void SendResponse(Connection *conn, bool dump, HTTPResponse &&response, bool interim);

    static void SendResponseAsyncCallback(uv_async_t *send_response_async);
    static void StopAsyncCallback(uv_async_t *stop_async);
    static int HandleMessageBegin(llhttp_t *parser);
    static int HandleURL(llhttp_t *parser, const char *at, size_t length);
    static int HandleHeaderField(llhttp_t *parser, const char *at, size_t length);
    static int HandleHeaderValue(llhttp_t *parser, const char *at, size_t length);
    static int HandleHeadersComplete(llhttp_t *parser);
    static int HandleBody(llhttp_t *parser, const char *at, size_t length);
    static int HandleMessageComplete(llhttp_t *parser);
    static void HandleNewConnection(uv_stream_t *server_tcp, int status);
    static void HandleReadAlloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
    static void HandleRead(uv_stream_t *stream, ssize_t num_read, const uv_buf_t *buf);
    static void HandleConnectionClose(uv_handle_t *handle);
    static void HandleResponseWritten(uv_write_t *req, int status);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPServerImpl::HTTPServerImpl() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPServerImpl::~HTTPServerImpl() {
    if (m_thread.joinable()) {
        ASSERT(m_sd.loop.data);

        auto stop_async = new uv_async_t{};
        stop_async->data = this;
        uv_async_init(&m_sd.loop, stop_async, &StopAsyncCallback);
        uv_async_send(stop_async);

        m_thread.join();
    }

    // This normally gets closed by the thread, but if an error prevented the
    // thread from starting it could still need closing.
    if (m_td.listen_tcp.data) {
        uv_close((uv_handle_t *)&m_td.listen_tcp, nullptr);
    }

    if (m_sd.loop.data) {
        // Handle any remaining callbacks.
        uv_run(&m_sd.loop, UV_RUN_DEFAULT);

        uv_loop_close(&m_sd.loop);
        m_sd.loop.data = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HTTPServerImpl::Start(int port, Messages *messages) {
    ASSERT(!m_sd.loop.data);

    int rc = uv_loop_init(&m_sd.loop);
    if (rc != 0) {
        PrintLibUVError(&messages->e, rc, "uv_loop_init failed");
        return false;
    }

    m_sd.loop.data = this;

    rc = uv_tcp_init(&m_sd.loop, &m_td.listen_tcp);
    if (rc != 0) {
        PrintLibUVError(&messages->e, rc, "uv_tcp_init failed");
        return false;
    }

    m_td.listen_tcp.data = this;

    // The uv_close calls will leave some pending operations in the uv_loop. But
    // that's ok; the destructor does a uv_run, so they'll get handled.
    {
        struct sockaddr_in addr;

        uv_ip4_addr("127.0.0.1", port, &addr);
        rc = uv_tcp_bind(&m_td.listen_tcp, (struct sockaddr *)&addr, 0);
        if (rc != 0) {
            PrintLibUVError(&messages->e, rc, "uv_tcp_bind failed");
            uv_close((uv_handle_t *)&m_td.listen_tcp, nullptr);
            m_td.listen_tcp.data = nullptr;
            return false;
        }

        rc = uv_listen((uv_stream_t *)&m_td.listen_tcp, 10, &HandleNewConnection);
        if (rc != 0) {
            PrintLibUVError(&messages->e, rc, "uv_listen failed");
            uv_close((uv_handle_t *)&m_td.listen_tcp, nullptr);
            m_td.listen_tcp.data = nullptr;
            return false;
        }
    }

    m_thread = std::thread([this]() {
        this->ThreadMain();
    });

    messages->i.f("HTTP server listening on port %d (0x%x)\n", port, port);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::SetHandler(std::shared_ptr<HTTPHandler> handler) {
    m_sd.handler = std::move(handler);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SendResponseData {
    uint64_t tick_count = 0;
    uint64_t connection_id = 0;
    bool dump = false;
    HTTPResponse response;
};

void HTTPServerImpl::SendResponse(const HTTPResponseData &response_data, HTTPResponse response) {
    if (!m_sd.loop.data) {
        return;
    }

    auto data = new SendResponseData{};

    data->tick_count = GetCurrentTickCount();
    data->connection_id = response_data.connection_id;
    data->dump = response_data.dump;
    data->response = std::move(response);

    auto send_response_async = new uv_async_t{};
    send_response_async->data = data;

    uv_async_init(&m_sd.loop, send_response_async, &SendResponseAsyncCallback);
    uv_async_send(send_response_async);
    send_response_async = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::ThreadMain() {
    int rc;

    SetCurrentThreadNamef("HTTP Server");

    rc = uv_run(&m_sd.loop, UV_RUN_DEFAULT);
    if (rc != 0) {
        PrintLibUVError(&LOG(HTTPSV), rc, "uv_run failed");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::CloseConnection(Connection *conn) {
    ASSERT(m_td.connection_by_id.count(conn->id) == 1);
    ASSERT(m_td.connection_by_id[conn->id] == conn);

    m_td.connection_by_id.erase(conn->id);

    uv_close((uv_handle_t *)&conn->tcp, &HandleConnectionClose);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::ResetRequest(Connection *conn) {
    conn->key.clear();
    conn->value = nullptr;
    conn->request = HTTPRequest();
    conn->request.response_data.connection_id = conn->id;
    conn->response_body_data.clear();
    conn->response_body_str.clear();
    conn->response_prefix.clear();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::StartReading(Connection *conn) {
    int rc = uv_read_start((uv_stream_t *)&conn->tcp, &HandleReadAlloc, &HandleRead);
    if (rc != 0) {
        PrintLibUVError(&LOG(HTTPSV), rc, "uv_read_start failed");
        this->CloseConnection(conn);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HTTPServerImpl::StopReading(Connection *conn) {
    int rc = uv_read_stop((uv_stream_t *)&conn->tcp);
    if (rc != 0) {
        PrintLibUVError(&LOG(HTTPSV), rc, "uv_read_stop failed");
        conn->server->CloseConnection(conn);
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::SendResponse(Connection *conn, bool dump, HTTPResponse &&response, bool interim) {
    int rc;

    if (!this->StopReading(conn)) {
        return;
    }

    ASSERT(!conn->write_response_req.data);
    conn->write_response_req.data = conn;

    std::map<std::string, std::string> headers;

    conn->interim_response = interim;

    if (response.content_type.empty()) {
        headers[CONTENT_TYPE] = DEFAULT_CONTENT_TYPE;
    } else {
        headers[CONTENT_TYPE] = response.content_type;
    }

    std::vector<uv_buf_t> body_bufs;
    if (!response.content_vec.empty()) {
        conn->response_body_data = std::move(response.content_vec);
        headers[CONTENT_LENGTH] = std::to_string(conn->response_body_data.size());
        body_bufs = GetBufs(&conn->response_body_data);
    } else if (!response.content_str.empty()) {
        conn->response_body_str = std::move(response.content_str);
        headers[CONTENT_LENGTH] = std::to_string(conn->response_body_str.size());
        body_bufs = GetBufs(&conn->response_body_str);
    } else {
        headers[CONTENT_LENGTH] = "0";
    }

    conn->response_prefix = "HTTP/1.1 ";
    if (response.status.empty()) {
        conn->response_prefix += "200 OK";
    } else {
        conn->response_prefix += response.status;
    }
    conn->response_prefix += "\r\n";

    for (auto &&kv : headers) {
        conn->response_prefix += kv.first + ":" + kv.second + "\r\n";
    }
    conn->response_prefix += "\r\n";

    std::vector<uv_buf_t> bufs = GetBufs(&conn->response_prefix);
    bufs.insert(bufs.end(), body_bufs.begin(), body_bufs.end());

    if (dump) {
        for (size_t i = 0; i < bufs.size(); ++i) {
            LOGF(HTTPSV, "Buf %zu: ", i);
            LogIndenter indent(&LOG(HTTPSV));
            LogDumpBytes(&LOG(HTTPSV), bufs[i].base, bufs[i].len);
        }
    }

    rc = uv_write(&conn->write_response_req, (uv_stream_t *)&conn->tcp, bufs.data(), (unsigned)bufs.size(), &HandleResponseWritten);
    if (rc != 0) {
        PrintLibUVError(&LOG(HTTPSV), rc, "uv_write failed");
        this->CloseConnection(conn);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::SendResponseAsyncCallback(uv_async_t *send_response_async) {
    auto data = (SendResponseData *)send_response_async->data;
    auto server = (HTTPServerImpl *)send_response_async->loop->data;

    LOGF(HTTPSV, "%s: latency = %.3f sec\n", __func__, GetSecondsFromTicks(GetCurrentTickCount() - data->tick_count));

    auto &&it = server->m_td.connection_by_id.find(data->connection_id);
    if (it != server->m_td.connection_by_id.end()) {
        Connection *conn = it->second;

        server->SendResponse(conn, data->dump, std::move(data->response), false);
    }

    delete data;
    data = nullptr;

    uv_close((uv_handle_t *)send_response_async, &ScalarDeleteCloseCallback<uv_async_t>);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::StopAsyncCallback(uv_async_t *stop_async) {
    auto server = (HTTPServerImpl *)stop_async->loop->data;
    ASSERT(stop_async->loop == &server->m_sd.loop);

    uv_print_all_handles(stop_async->loop, stderr);

    while (!server->m_td.connection_by_id.empty()) {
        server->CloseConnection(server->m_td.connection_by_id.begin()->second);
    }

    uv_close((uv_handle_t *)&server->m_td.listen_tcp, nullptr);
    server->m_td.listen_tcp.data = nullptr;

    uv_close((uv_handle_t *)stop_async, &ScalarDeleteCloseCallback<uv_async_t>);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleMessageBegin(llhttp_t *parser) {
    (void)parser;
    //auto conn=(HTTPConnection *)parser->data;

    LOGF(HTTPSV, "%s\n", __func__);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleURL(llhttp_t *parser, const char *at, size_t length) {
    auto conn = (Connection *)parser->data;

    conn->request.url.append(at, length);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleHeaderField(llhttp_t *parser, const char *at, size_t length) {
    auto conn = (Connection *)parser->data;

    conn->value = nullptr;
    conn->key.append(at, length);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleHeaderValue(llhttp_t *parser, const char *at, size_t length) {
    auto conn = (Connection *)parser->data;

    if (!conn->value) {
        conn->value = &conn->request.headers[conn->key];
        conn->key.clear();

        // https://tools.ietf.org/html/rfc2616#section-4.2
        if (!conn->value->empty()) {
            conn->value->append(1, ',');
        }
    }

    conn->value->append(at, length);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleHeadersComplete(llhttp_t *parser) {
    auto conn = (Connection *)parser->data;
    int rc;

    LOGF(HTTPSV, "%s: start.\n", __func__);

    // libcurl request a client-style URL, so form something that's enough like
    // that to keep it happy.

    std::string url_str;
    if (!conn->request.url.empty()) {
        url_str = "x" + conn->request.url;
    }

    conn->request.method = llhttp_method_name((llhttp_method_t)parser->method);
    //conn->status=200;
    //conn->status=parser->status_code;

    std::unique_ptr<CURLU, CURLUDeleter> url(curl_url());

    rc = curl_url_set(url.get(), CURLUPART_URL, url_str.c_str(), CURLU_DEFAULT_SCHEME);
    if (rc != 0) {
        LOGF(HTTPSV, "invalid URL: %s\n", conn->request.url.c_str());
        return -1;
    }

    if (!GetPercentDecodedURLPart(&conn->request.url_path, url.get(), CURLUPART_PATH, conn->request.url, "path")) {
        return -1;
    }

    //if (!GetPercentDecodedURLPart(&conn->request.url_path, url, UF_PATH, conn->request.url, "path")) {
    //    return -1;
    //}

    char *query;
    if (curl_url_get(url.get(), CURLUPART_QUERY, &query, CURLU_URLDECODE) == 0) {
        const char *begin = query;
        while (*begin != 0) {

            const char *k_begin = begin;
            const char *k_end = begin;
            while (*k_end != 0 && *k_end != '=') {
                ++k_end;
            }

            if (*k_end == 0) {
                LOGF(HTTPSV, "invalid URL query (missing '='): %s\n", conn->request.url.c_str());
                return -1;
            }

            const char *v_begin = k_end + 1;
            const char *v_end = v_begin;
            while (*v_end != 0 && *v_end != '&') {
                ++v_end;
            }

            HTTPQueryParameter kv;
            kv.key.assign(k_begin, k_end);
            kv.value.assign(v_begin, v_end);

            conn->request.query.push_back(std::move(kv));

            begin = v_end;
            if (*begin != 0) {
                ++begin;
            }
        }

        curl_free(query);
        query = nullptr;
    }

    if (const std::string *content_type = conn->request.GetHeaderValue(CONTENT_TYPE)) {
        // This is a bit scrappy. I got bored trying to code it up
        // properly.
        std::string::size_type index = content_type->find_first_of(";");
        if (index == std::string::npos) {
            conn->request.content_type = *content_type;
        } else {
            conn->request.content_type = content_type->substr(0, index);

            std::string parameters = content_type->substr(index + 1);
            index = parameters.find_first_not_of(" \t");
            if (index != std::string::npos) {
                if (parameters.substr(index, CHARSET_PREFIX.size()) == CHARSET_PREFIX) {
                    conn->request.content_type_charset = parameters.substr(index + CHARSET_PREFIX.size());
                }
            }
        }
    }

    if (const std::string *dump = conn->request.GetHeaderValue(DUMP)) {
        conn->request.response_data.dump = *dump == "1";
    }

    {
        auto &&it = conn->request.headers.find(EXPECT);
        if (it != conn->request.headers.end()) {
            if (it->second == EXPECT_CONTINUE) {
                conn->server->SendResponse(conn, conn->request.response_data.dump, HTTPResponse("100 Continue"), true);
            }
        }
    }

    LOGF(HTTPSV, "%s: finish.\n", __func__);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleBody(llhttp_t *parser, const char *at, size_t length) {
    auto conn = (Connection *)parser->data;

    LOGF(HTTPSV, "%s: start.\n", __func__);

    conn->request.body.insert(conn->request.body.end(), at, at + length);

    LOGF(HTTPSV, "%s: finish.\n", __func__);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleMessageComplete(llhttp_t *parser) {
    auto conn = (Connection *)parser->data;

    LOGF(HTTPSV, "%s: start.\n", __func__);

    conn->keep_alive = !!llhttp_should_keep_alive(parser);

    if (!conn->server->StopReading(conn)) {
        return -1;
    }

    LOGF(HTTPSV, "Headers: ");
    {
        LogIndenter indent(&LOG(HTTPSV));
        //LOGF(HTTPSV,"Status: %u\n",conn->status);
        LOGF(HTTPSV, "Method: %s\n", conn->request.method.c_str());

        LOGF(HTTPSV, "URL: ");
        {
            LogIndenter indent2(&LOG(HTTPSV));
            LOGF(HTTPSV, "%s\n", conn->request.url.c_str());
            LOGF(HTTPSV, "Path: %s\n", conn->request.url_path.c_str());
            //LOGF(HTTPSV,"Query: %s\n",conn->request.url_query.c_str());

            if (!conn->request.query.empty()) {
                LOGF(HTTPSV, "Query: ");
                {
                    LogIndenter indent3(&LOG(HTTPSV));

                    for (const HTTPQueryParameter &kv : conn->request.query) {
                        LOGF(HTTPSV, "%s: %s\n", kv.key.c_str(), kv.value.c_str());
                    }
                }
            }
        }

        if (!conn->request.headers.empty()) {
            LOGF(HTTPSV, "Fields: ");
            LogIndenter indent2(&LOG(HTTPSV));
            for (auto &&kv : conn->request.headers) {
                LOGF(HTTPSV, "%s: %s\n", kv.first.c_str(), kv.second.c_str());
            }
        }
    }

    if (!conn->request.body.empty()) {
        if (conn->request.response_data.dump) {
            LOGF(HTTPSV, "Body: ");
            {
                LogIndenter indent(&LOG(HTTPSV));
                LogDumpBytes(&LOG(HTTPSV), conn->request.body.data(), conn->request.body.size());
            }
        }
    }

    std::shared_ptr<HTTPHandler> handler = conn->server->m_sd.handler;

    HTTPResponse response;
    bool send_response = false;
    if (!!handler) {
        send_response = handler->ThreadHandleRequest(&response, conn->server, std::move(conn->request));
    } else {
        response = CreateErrorResponse(conn->request, "404 Not Found");
        send_response = true;
    }

    if (send_response) {
        conn->server->SendResponse(conn, conn->request.response_data.dump, std::move(response), false);
    }

    //conn->status="404 Not Found";

    //if(!conn->status.empty()) {
    //SendResponse(conn,CreateErrorResponse(conn->request,"404 Not Found"));
    //}

    //PushFunctionMessage([conn]() {
    //    DispatchRequestMainThread(conn);
    //});

    LOGF(HTTPSV, "%s: finish.\n", __func__);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleNewConnection(uv_stream_t *server_tcp, int status) {
    auto server = (HTTPServerImpl *)server_tcp->data;
    int rc;

    LOGF(HTTPSV, "%s: start: (ticks=+%" PRIu64 ")\n", __func__, GetCurrentTickCount() - server->m_create_tick_count);

    if (status != 0) {
        PrintLibUVError(&LOG(HTTPSV), status, "connection callback");
        return;
    }

    auto conn = new Connection;

    conn->id = server->m_td.next_connection_id++;

    llhttp_settings_init(&conn->parser_settings);
    conn->parser_settings.on_url = &HandleURL;
    conn->parser_settings.on_header_field = &HandleHeaderField;
    conn->parser_settings.on_header_value = &HandleHeaderValue;
    conn->parser_settings.on_headers_complete = &HandleHeadersComplete;
    conn->parser_settings.on_body = &HandleBody;
    conn->parser_settings.on_message_complete = &HandleMessageComplete;
    conn->parser_settings.on_message_begin = &HandleMessageBegin;

    llhttp_init(&conn->parser, HTTP_REQUEST, &conn->parser_settings);
    conn->parser.data = conn;

    rc = uv_tcp_init(server_tcp->loop, &conn->tcp);
    if (rc != 0) {
        PrintLibUVError(&LOG(HTTPSV), rc, "uv_tcp_init failed");
        delete conn;
        return;
    }

    rc = uv_accept(server_tcp, (uv_stream_t *)&conn->tcp);
    if (rc != 0) {
        PrintLibUVError(&LOG(HTTPSV), rc, "uv_accept failed");
        delete conn;
        return;
    }

    conn->tcp.data = conn;
    conn->server = server;

    ASSERT(conn->server->m_td.connection_by_id.count(conn->id) == 0);
    conn->server->m_td.connection_by_id[conn->id] = conn;

    conn->server->ResetRequest(conn);
    conn->server->StartReading(conn);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleReadAlloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    auto conn = (Connection *)handle->data;
    (void)suggested_size;

    LOGF(HTTPSV, "%s: start: (ticks=+%" PRIu64 ")\n", __func__, GetCurrentTickCount() - conn->server->m_create_tick_count);

    ASSERT(handle == (uv_handle_t *)&conn->tcp);
    ASSERT(conn->num_read < sizeof conn->read_buf);
    static_assert(sizeof conn->read_buf <= UINT_MAX, "");
    *buf = uv_buf_init(conn->read_buf + conn->num_read, (unsigned)(sizeof conn->read_buf - conn->num_read));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleRead(uv_stream_t *stream, ssize_t num_read, const uv_buf_t *buf) {
    (void)buf;

    auto conn = (Connection *)stream->data;
    ASSERT(stream == (uv_stream_t *)&conn->tcp);

    LOGF(HTTPSV, "%s: start: num_read=%zd (ticks=+%" PRIu64 ")\n", __func__, num_read, GetCurrentTickCount() - conn->server->m_create_tick_count);

    if (num_read == UV_EOF) {
        conn->server->CloseConnection(conn);
    } else if (num_read < 0) {
        PrintLibUVError(&LOG(HTTPSV), (int)num_read, "connection read callback");
        conn->server->CloseConnection(conn);
    } else if (num_read == 0) {
        // ignore...
    } else if (num_read > 0) {
        ASSERT((size_t)num_read <= sizeof conn->read_buf);
        size_t total_num_read = conn->num_read + (size_t)num_read;
        ASSERT(total_num_read <= sizeof conn->read_buf);
        llhttp_errno execute_err = llhttp_execute(&conn->parser, conn->read_buf, total_num_read);

        if (execute_err != HPE_OK) {
            LOGF(HTTPSV, "Parse error: %s %s\n", llhttp_errno_name(execute_err), conn->parser.reason);
            conn->server->SendResponse(conn, conn->request.response_data.dump, CreateErrorResponse(conn->request, "400 Bad Request"), false);
        } else {
            //memmove(conn->read_buf, conn->read_buf + num_consumed, total_num_read - num_consumed);
            //conn->num_read = total_num_read - num_consumed;
        }
    }

    LOGF(HTTPSV, "%s: finish.\n", __func__);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleConnectionClose(uv_handle_t *handle) {
    auto conn = (Connection *)handle->data;
    ASSERT(handle == (uv_handle_t *)&conn->tcp);
    //HTTPServer *server=conn->server;

    //ASSERT(conn->server->connections.count(conn)==1);
    //conn->server->connections.erase(conn);

    delete conn;
    conn = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleResponseWritten(uv_write_t *req, int status) {
    auto conn = (Connection *)req->data;

    ASSERT(req == &conn->write_response_req);
    req->data = nullptr;

    if (status != 0) {
        PrintLibUVError(&LOG(HTTPSV), status, "%s status", __func__);
        conn->server->CloseConnection(conn);
        return;
    }

    if (conn->interim_response) {
        conn->server->StartReading(conn);
    } else {
        if (conn->keep_alive && llhttp_get_errno(&conn->parser) == 0) {
            conn->server->ResetRequest(conn);
            conn->server->StartReading(conn);
        } else {
            conn->server->CloseConnection(conn);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPServer> CreateHTTPServer() {
    return std::make_unique<HTTPServerImpl>();
}
