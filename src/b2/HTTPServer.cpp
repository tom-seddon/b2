#include <shared/system.h>
#include "HTTPServer.h"

#if HTTP_SERVER

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <uv.h>
#include <http_parser.h>
#include <shared/mutex.h>
#include "misc.h"
#include <set>
#include <shared/debug.h>
#include <map>
#include <string>
#include "b2.h"
#include "BeebWindow.h"
#include "BeebWindows.h"
#include "BeebThread.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(HTTP,"http","HTTP  ",&log_printer_stdout_and_debugger,true)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string CONTENT_TYPE="Content-Type";
static const std::string CONTENT_LENGTH="Content-Length";
static const std::string OCTET_STREAM_CONTENT_TYPE="application/octet-stream";
static const std::string TEXT_CONTENT_TYPE="text/plain";
static const std::string DEFAULT_CONTENT_TYPE=OCTET_STREAM_CONTENT_TYPE;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// this is all a little bit understructured... there needs to be a
// split between the HTTP server part and the actual emulator-specific
// bits, and ideally support for multiple servers (with one thread per
// server).

static std::thread g_http_server_thread;
static Mutex g_http_mutex;
static uv_loop_t * g_uv_loop=nullptr;
static uv_async_t g_stop_async={};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void PrintLibUVError(int rc,const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    LOGV(HTTP,fmt,v);
    va_end(v);

    LOGF(HTTP,": %s (%s)\n",uv_strerror(rc),uv_err_name(rc));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct HTTPServer;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct HTTPConnection {
    HTTPServer *server=nullptr;
    uv_tcp_t tcp={};
    http_parser parser={};
    http_parser_settings parser_settings={};

    std::string key;
    std::string *value=nullptr;
    HTTPRequest request;

    bool keep_alive=false;

    std::string response_status;
    std::string response_prefix;
    std::vector<uint8_t> response_body_data;
    std::string response_body_str;
    uv_write_t write_response_req={};

    size_t num_read=0;
    char read_buf[100];
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Request {
public:
    Request()=default;
    virtual ~Request();

    // default impl returns "", indicating no result.
    virtual std::string GetContentType() const;

    virtual const char *GetCommandName() const=0;

    void Do(std::vector<uint8_t> *result_body,BeebWindow *beeb_window);
protected:
    virtual void HandleDo(std::vector<uint8_t> *result_body,BeebWindow *beeb_window)=0;
private:
    bool m_done=false;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Request::~Request() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string Request::GetContentType() const {
    return "";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Request::Do(std::vector<uint8_t> *result_body,BeebWindow *beeb_window) {
    ASSERT(!m_done);

    size_t n=result_body->size();
    (void)n;
    this->HandleDo(result_body,beeb_window);
    ASSERT(!this->GetContentType().empty()||result_body->size()==n);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct WindowQueue {
    std::string content_type;
    const char *content_type_request=nullptr;
    std::vector<std::unique_ptr<Request>> requests;
};

struct MainThreadData {
    std::map<std::string,WindowQueue> window_queues;
};

struct HTTPServer {
    int port=-1;

    std::set<HTTPConnection *> connections;
    uv_loop_t loop={};
    bool closed=false;
    uv_tcp_t listen_tcp={};

    MainThreadData main_thread_data;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetEscaped(std::string str) {
    std::string result;
    result.reserve(str.size());

    for(char c:str) {
        if(c=='<') {
            result+="&lt;";
        } else if(c=='>') {
            result+="&gt;";
        } else if(c=='&') {
            result+="&amp;";
        } else if(c=='"') {
            result+="&quot;";
        } else {
            result.append(1,c);
        }
    }

    return result;
}

static HTTPResponse CreateErrorResponse(const HTTPRequest &request,
                                        std::string status)
{
    std::string body;


    //HTTPResponse r;

    //r.co
    //r.headers["Content-Type"]="text/html";

    //r.body_str.clear();

    body+="<html>";
    body+="<head><title>"+status+"</title></head>";
    body+="<body>";
    body+="<h1>"+status+"</h1>";
    body+="<p><b>URL:</b> "+GetEscaped(request.url)+"</p>";
    body+="<p><b>Method:</b> "+request.method+"</p>";
    //if(!info.empty()) {
    //    r.body_str+="<p><b>Extra info:</b> "+GetEscaped(info)+"</p>";
    //}
    body+="</body>";
    body+="</html>";

    return HTTPResponse(std::move(status),"text/html",std::move(body));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class ContType>
static std::vector<uv_buf_t> GetBufs(ContType *cont) {
    size_t num_left=cont->size()*sizeof *cont->data();
    auto p=const_cast<char *>(reinterpret_cast<const char *>(cont->data()));

    std::vector<uv_buf_t> bufs;

    while(num_left>0) {
        size_t n=num_left;
        if(n>UINT_MAX) {
            n=UINT_MAX;
        }

        bufs.push_back(uv_buf_init(p,(unsigned)n));

        p+=n;
        num_left-=n;
    }

    return bufs;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
static void ScalarDeleteCloseCallback(uv_handle_t *handle) {
    delete (T *)handle;
}

static void EmptyCloseCallback(uv_handle_t *handle) {
    (void)handle;
}

static int HandleHTTPMessageBegin(http_parser *parser) {
    (void)parser;
    //auto conn=(HTTPConnection *)parser->data;

    LOGF(HTTP,"%s\n",__func__);

    return 0;
}

static int HandleHTTPURL(http_parser *parser,const char *at,size_t length) {
    auto conn=(HTTPConnection *)parser->data;

    conn->request.url.append(at,length);

    return 0;
}

static int HandleHTTPHeaderField(http_parser *parser,const char *at,size_t length) {
    auto conn=(HTTPConnection *)parser->data;

    conn->value=nullptr;
    conn->key.append(at,length);

    return 0;
}

static int HandleHTTPHeaderValue(http_parser *parser,const char *at,size_t length) {
    auto conn=(HTTPConnection *)parser->data;

    if(!conn->value) {
        conn->value=&conn->request.headers[conn->key];
        conn->key.clear();

        // https://tools.ietf.org/html/rfc2616#section-4.2
        if(!conn->value->empty()) {
            conn->value->append(1,',');
        }
    }

    conn->value->append(at,length);

    return 0;
}

static int GetHexCharValue(char c) {
    if(c>='0'&&c<='9') {
        return c-'0';
    } else if(c>='a'&&c<='f') {
        return c-'a'+10;
    } else if(c>='A'&&c<='F') {
        return c-'A'+10;
    } else {
        return -1;
    }
}

static bool GetPercentDecoded(std::string *result,const std::string &str,size_t offset,size_t n) {
    ASSERT(offset+n<=str.size());

    result->clear();
    result->reserve(n);

    size_t i=offset;
    size_t end=offset+n;
    while(i<end) {
        if(str[i]=='%') {
            if(i+3>end) {
                return false;
            }

            int h=GetHexCharValue(str[i+1]);
            if(h<0) {
                return false;
            }

            int l=GetHexCharValue(str[i+2]);
            if(l<0) {
                return false;
            }

            result->append(1,(char)(h<<4|l));
            i+=3;
        } else {
            result->append(1,str[i]);
            ++i;
        }
    }

    return true;
}

static bool GetPercentDecodedURLPart(std::string *result,const http_parser_url &url,http_parser_url_fields field,const std::string &url_str,const char *part_name) {
    if(url.field_set&1<<field) {
        if(!GetPercentDecoded(result,url_str,url.field_data[field].off,url.field_data[field].len)) {
            LOGF(HTTP,"invalid percent-encoded %s in URL: %s\n",part_name,url_str.c_str());
            return false;
        }
    }

    return true;
}

static int HandleHTTPHeadersComplete(http_parser *parser) {
    auto conn=(HTTPConnection *)parser->data;

    LOGF(HTTP,"%s\n",__func__);

    conn->request.method=http_method_str((http_method)parser->method);
    //conn->status=200;
    //conn->status=parser->status_code;

    http_parser_url url={};
    // 0 = not connect
    if(http_parser_parse_url(conn->request.url.data(),conn->request.url.size(),0,&url)!=0) {
        LOGF(HTTP,"invalid URL: %s\n",conn->request.url.c_str());
        return -1;
    }

    if(!GetPercentDecodedURLPart(&conn->request.url_path,url,UF_PATH,conn->request.url,"path")) {
        return -1;
    }

    if(!GetPercentDecodedURLPart(&conn->request.url_fragment,url,UF_FRAGMENT,conn->request.url,"fragment")) {
        return -1;
    }

    if(url.field_set&1<<UF_QUERY) {
        size_t begin=url.field_data[UF_QUERY].off;
        size_t end=begin+url.field_data[UF_QUERY].len;

        size_t a=begin;
        while(a<end) {
            HTTPQueryParameter kv;

            size_t b=a;
            while(b<end&&conn->request.url[b]!='=') {
                ++b;
            }

            if(b>=end) {
                LOGF(HTTP,"invalid URL query (missing '='): %s\n",conn->request.url.c_str());
                return -1;
            }

            if(!GetPercentDecoded(&kv.key,conn->request.url,a,b-a)) {
                LOGF(HTTP,"invalid URL query (bad key percent encoding): %s\n",conn->request.url.c_str());
                return -1;
            }

            a=b+1;
            while(b<end&&conn->request.url[b]!='&') {
                ++b;
            }

            if(!GetPercentDecoded(&kv.value,conn->request.url,a,b-a)) {
                LOGF(HTTP,"invalid URL query (bad value percent encoding): %s\n",conn->request.url.c_str());
                return -1;
            }

            conn->request.query.push_back(std::move(kv));

            a=b+1;
        }
    }

    return 0;
}

static int HandleHTTPBody(http_parser *parser,const char *at,size_t length) {
    auto conn=(HTTPConnection *)parser->data;

    conn->request.body.insert(conn->request.body.end(),at,at+length);

    return 0;
}

static void HandleConnectionClose(uv_handle_t *handle) {
    auto conn=(HTTPConnection *)handle->data;
    ASSERT(handle==(uv_handle_t *)&conn->tcp);
    //HTTPServer *server=conn->server;

    ASSERT(conn->server->connections.count(conn)==1);
    conn->server->connections.erase(conn);

    delete conn;
    conn=nullptr;
}

static void CloseConnection(HTTPConnection *conn) {
    uv_close((uv_handle_t *)&conn->tcp,&HandleConnectionClose);
}

static void ConnectionAlloc(uv_handle_t *handle,size_t suggested_size,uv_buf_t *buf) {
    auto conn=(HTTPConnection *)handle->data;
    (void)suggested_size;

    ASSERT(handle==(uv_handle_t *)&conn->tcp);
    ASSERT(conn->num_read<sizeof conn->read_buf);
    static_assert(sizeof conn->read_buf<=UINT_MAX,"");
    *buf=uv_buf_init(conn->read_buf+conn->num_read,(unsigned)(sizeof conn->read_buf-conn->num_read));
}

static void SendResponse(HTTPConnection *conn,HTTPResponse &&response);

static void ConnectionRead(uv_stream_t *stream,ssize_t num_read,const uv_buf_t *buf) {
    (void)buf;

    auto conn=(HTTPConnection *)stream->data;
    ASSERT(stream==(uv_stream_t *)&conn->tcp);

    if(num_read==UV_EOF) {
        CloseConnection(conn);
    } else if(num_read<0) {
        PrintLibUVError((int)num_read,"connection read callback");
        CloseConnection(conn);
    } else if(num_read==0) {
        // ignore...
    } else if(num_read>0) {
        ASSERT((size_t)num_read<=sizeof conn->read_buf);
        size_t total_num_read=conn->num_read+(size_t)num_read;
        ASSERT(total_num_read<=sizeof conn->read_buf);
        size_t num_consumed=http_parser_execute(&conn->parser,&conn->parser_settings,conn->read_buf,total_num_read);
        if(num_consumed==0) {
            if(conn->parser.http_errno==0) {
                // Is this even possible?
                conn->parser.http_errno=HPE_UNKNOWN;
            }
        }

        if(conn->parser.http_errno!=0) {
            LOGF(HTTP,"HTTP error: %s\n",http_errno_description((http_errno)conn->parser.http_errno));
            SendResponse(conn,CreateErrorResponse(conn->request,"400 Bad Request"));
            //CloseConnection(conn);
        } else {
            ASSERT(num_consumed<=total_num_read);
            memmove(conn->read_buf,conn->read_buf+num_consumed,total_num_read-num_consumed);
            conn->num_read=total_num_read-num_consumed;
        }
    }
}

static void WaitForRequest(HTTPConnection *conn) {
    int rc;

    conn->key.clear();
    conn->value=nullptr;
    conn->request=HTTPRequest();
    conn->response_body_data.clear();
    conn->response_body_str.clear();
    conn->response_prefix.clear();

    rc=uv_read_start((uv_stream_t *)&conn->tcp,&ConnectionAlloc,&ConnectionRead);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_read_start failed");
        CloseConnection(conn);
        return;
    }
}

static void WriteResponseCallback(uv_write_t *req,int status) {
    auto conn=(HTTPConnection *)req->data;

    ASSERT(req==&conn->write_response_req);
    req->data=nullptr;

    if(status!=0) {
        PrintLibUVError(status,"%s status",__func__);
        CloseConnection(conn);
        return;
    }

    if(conn->keep_alive&&conn->parser.http_errno==0) {
        WaitForRequest(conn);
    } else {
        CloseConnection(conn);
    }
}

static void SendResponse(HTTPConnection *conn,HTTPResponse &&response) {
    int rc;

    ASSERT(!conn->write_response_req.data);
    conn->write_response_req.data=conn;

    std::map<std::string,std::string> headers;

    if(response.content_type.empty()) {
        headers[CONTENT_TYPE]=DEFAULT_CONTENT_TYPE;
    } else {
        headers[CONTENT_TYPE]=response.content_type;
    }

    std::vector<uv_buf_t> body_bufs;
    if(!response.content_vec.empty()) {
        conn->response_body_data=std::move(response.content_vec);
        headers[CONTENT_LENGTH]=std::to_string(conn->response_body_data.size());
        body_bufs=GetBufs(&conn->response_body_data);
    } else if(!response.content_str.empty()) {
        conn->response_body_str=std::move(response.content_str);
        headers[CONTENT_LENGTH]=std::to_string(conn->response_body_str.size());
        body_bufs=GetBufs(&conn->response_body_str);
    } else {
        headers[CONTENT_LENGTH]="0";
    }

    conn->response_prefix="HTTP/1.1 ";
    if(response.status.empty()) {
        conn->response_prefix+="200 OK";
    } else {
        conn->response_prefix+=response.status;
    }
    conn->response_prefix+="\r\n";

    for(auto &&kv:headers) {
        conn->response_prefix+=kv.first+":"+kv.second+"\r\n";
    }
    conn->response_prefix+="\r\n";

    std::vector<uv_buf_t> bufs=GetBufs(&conn->response_prefix);
    bufs.insert(bufs.end(),body_bufs.begin(),body_bufs.end());

    for(size_t i=0;i<bufs.size();++i) {
        LOGF(HTTP,"Buf %zu: ",i);
        LogIndenter indent(&LOG(HTTP));
        LogDumpBytes(&LOG(HTTP),bufs[i].base,bufs[i].len);
    }

    rc=uv_write(&conn->write_response_req,(uv_stream_t *)&conn->tcp,bufs.data(),(unsigned)bufs.size(),&WriteResponseCallback);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_write failed");
        CloseConnection(conn);
        return;
    }
}

static std::vector<std::string> GetPathParts(const std::string &path) {
    std::vector<std::string> parts;
    std::string part;

    for(char c:path) {
        if(c=='/') {
            if(!part.empty()) {
                parts.push_back(part);
                part.clear();
            }
        } else {
            part.append(1,c);
        }
    }

    if(!part.empty()) {
        parts.push_back(part);
    }

    return parts;
}

class PokeRequest:
    public Request
{
public:
    static const char COMMAND_NAME[];

    PokeRequest(uint32_t addr,std::vector<uint8_t> data):
        m_addr(addr),
        m_data(std::move(data))
    {
    }

    const char *GetCommandName() const override {
        return COMMAND_NAME;
    }
protected:
    void HandleDo(std::vector<uint8_t> *result_body,BeebWindow *beeb_window) override {
        (void)result_body;

        std::shared_ptr<BeebThread> beeb_thread=beeb_window->GetBeebThread();

        beeb_thread->SendDebugSetBytesMessage(m_addr,std::move(m_data));
    }
private:
    uint32_t m_addr=0;
    std::vector<uint8_t> m_data;
};

const char PokeRequest::COMMAND_NAME[]="poke";

class PeekRequest:
    public Request
{
public:
    static const char COMMAND_NAME[];

    PeekRequest(uint32_t addr,uint32_t num_bytes):
        m_addr(addr),
        m_num_bytes(num_bytes)
    {
    }

    std::string GetContentType() const override {
        return OCTET_STREAM_CONTENT_TYPE;
    }

    const char *GetCommandName() const override {
        return COMMAND_NAME;
    }
protected:
    void HandleDo(std::vector<uint8_t> *result_body,BeebWindow *beeb_window) override {
        std::shared_ptr<BeebThread> beeb_thread=beeb_window->GetBeebThread();

        std::unique_lock<Mutex> lock;
        const BBCMicro *beeb=beeb_thread->LockBeeb(&lock);

        for(uint32_t i=0;i<m_num_bytes;++i) {
            // when DebugGetByte returns -1, it'll produce 0xff. Not
            // much point trying to be any cleverer than that.
            result_body->push_back((uint8_t)beeb->DebugGetByte(m_addr+i));
        }
    }
private:
    uint32_t m_addr=0;
    uint32_t m_num_bytes=0;
};

class ResetRequest:
    public Request
{
public:
    static const char COMMAND_NAME[];

    ResetRequest(bool boot):
        m_boot(boot)
    {
    }

    const char *GetCommandName() const override {
        return COMMAND_NAME;
    }
protected:
    void HandleDo(std::vector<uint8_t> *result_body,BeebWindow *beeb_window) override {
        (void)result_body;

        std::shared_ptr<BeebThread> beeb_thread=beeb_window->GetBeebThread();

        beeb_thread->SendHardResetMessage(m_boot);
    }
private:
    bool m_boot=false;
};

const char ResetRequest::COMMAND_NAME[]="reset";

static bool FindBeebWindow(BeebWindow **window_ptr,const std::string &name) {
    *window_ptr=BeebWindows::FindBeebWindowByName(name);
    return !!*window_ptr;
}

static std::unique_ptr<Request> CreateRequest(HTTPResponse *response,HTTPRequest *http_request,const std::vector<std::string> &path_parts,size_t index0) {
    if(path_parts[index0]==PokeRequest::COMMAND_NAME) {
        if(http_request->body.empty()) {
            *response=HTTPResponse::BadRequest("no message body");
            return nullptr;
        }

        if(path_parts.size()!=index0+2) {
            *response=HTTPResponse::BadRequest("syntax: %s/ADDR",PokeRequest::COMMAND_NAME);
            return nullptr;
        }

        uint32_t addr;
        if(!GetUInt32FromHexString(&addr,path_parts[index0+1])) {
            *response=HTTPResponse::BadRequest("bad address: %s",path_parts[index0+1].c_str());
            return nullptr;
        }

        return std::make_unique<PokeRequest>(addr,std::move(http_request->body));
    } else if(path_parts[index0]==ResetRequest::COMMAND_NAME) {
        return std::make_unique<ResetRequest>(false);
    } else {
        *response=HTTPResponse::BadRequest("unknown request: %s",path_parts[index0].c_str());
        return nullptr;
    }
}

static HTTPResponse DispatchRequestMainThread2(MainThreadData *main_thread_data,HTTPRequest *http_request) {

    std::vector<std::string> parts=GetPathParts(http_request->url_path);

    //uint32_t value;
    BeebWindow *window;

    if(parts.size()==2&&parts[0]=="c") {
        if(!FindBeebWindow(&window,parts[1])) {
            return HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str());
        }

        main_thread_data->window_queues.erase(parts[1]);
        return HTTPResponse::OK();
    } else if(parts.size()>=3&&parts[0]=="q") {
        if(!FindBeebWindow(&window,parts[1])) {
            return HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str());
        }

        HTTPResponse response;
        std::unique_ptr<Request> request=CreateRequest(&response,http_request,parts,2);
        if(!request) {
            return response;
        }

        WindowQueue *window_queue=&main_thread_data->window_queues[parts[1]];

        std::string content_type=request->GetContentType();
        if(!content_type.empty()) {
            if(window_queue->requests.empty()) {
                window_queue->content_type=std::move(content_type);
                window_queue->content_type_request=request->GetCommandName();
            } else {
                if(window_queue->content_type!=content_type) {
                    return HTTPResponse::BadRequest("result type conflict: %s type is %s, queued %s type is %s",
                                                    request->GetCommandName(),
                                                    content_type.c_str(),
                                                    window_queue->content_type_request,
                                                    window_queue->content_type.c_str());
                }
            }
        }

        window_queue->requests.push_back(std::move(request));

        return HTTPResponse::OK();
    } else if(parts.size()>=3&&parts[0]=="x") {
        if(!FindBeebWindow(&window,parts[1])) {
            return HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str());
        }

        HTTPResponse response;
        std::unique_ptr<Request> request=CreateRequest(&response,http_request,parts,2);
        if(!request) {
            return response;
        }

        response=HTTPResponse("200 OK",request->GetContentType());

        request->Do(&response.content_vec,window);

        return response;
    } else if(parts.size()==2&&parts[0]=="r") {
        if(!FindBeebWindow(&window,parts[1])) {
            return HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str());
        }

        WindowQueue *window_queue=&main_thread_data->window_queues[parts[1]];

        auto response=HTTPResponse("200 OK",window_queue->content_type);

        for(const std::unique_ptr<Request> &request:window_queue->requests) {
            request->Do(&response.content_vec,window);
        }

        window_queue=nullptr;
        main_thread_data->window_queues.erase(parts[1]);

        return response;
    } else {
        return HTTPResponse::BadRequest("unknown operation: %s",parts[0].c_str());
    }
}

static void DispatchRequestMainThread(HTTPConnection *conn) {
    HTTPResponse response=DispatchRequestMainThread2(&conn->server->main_thread_data,&conn->request);

    SendResponse(conn,std::move(response));
}

static int HandleHTTPMessageComplete(http_parser *parser) {
    int rc;
    auto conn=(HTTPConnection *)parser->data;

    conn->keep_alive=!!http_should_keep_alive(parser);

    rc=uv_read_stop((uv_stream_t *)&conn->tcp);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_read_stop failed");
        CloseConnection(conn);
        return -1;
    }

    LOGF(HTTP,"Headers: ");
    {
        LogIndenter indent(&LOG(HTTP));
        //LOGF(HTTP,"Status: %u\n",conn->status);
        LOGF(HTTP,"Method: %s\n",conn->request.method.c_str());

        LOGF(HTTP,"URL: ");
        {
            LogIndenter indent2(&LOG(HTTP));
            LOGF(HTTP,"%s\n",conn->request.url.c_str());
            LOGF(HTTP,"Path: %s\n",conn->request.url_path.c_str());
            //LOGF(HTTP,"Query: %s\n",conn->request.url_query.c_str());

            if(!conn->request.query.empty()) {
                LOGF(HTTP,"Query: ");
                {
                    LogIndenter indent3(&LOG(HTTP));

                    for(const HTTPQueryParameter &kv:conn->request.query) {
                        LOGF(HTTP,"%s: %s\n",kv.key.c_str(),kv.value.c_str());
                    }
                }
            }

            LOGF(HTTP,"Fragment: %s\n",conn->request.url_fragment.c_str());
        }

        if(!conn->request.headers.empty()) {
            LOGF(HTTP,"Fields: ");
            LogIndenter indent2(&LOG(HTTP));
            for(auto &&kv:conn->request.headers) {
                LOGF(HTTP,"%s: %s\n",kv.first.c_str(),kv.second.c_str());
            }
        }
    }

    if(conn->request.body.empty()) {
        LOGF(HTTP,"Body: ");
        {
            LogIndenter indent(&LOG(HTTP));
            LogDumpBytes(&LOG(HTTP),conn->request.body.data(),conn->request.body.size());
        }
    }

    //conn->status="404 Not Found";

    //if(!conn->status.empty()) {
    //SendResponse(conn,CreateErrorResponse(conn->request,"404 Not Found"));
    //}

    PushFunctionMessage([conn]() {
        DispatchRequestMainThread(conn);
    });

    return 0;
}

static void StopHTTPServerAsyncCallback(uv_async_t *async) {
    auto server=(HTTPServer *)async->loop->data;

    uv_print_all_handles(async->loop,stderr);

    {
        std::lock_guard<Mutex> lock(g_http_mutex);
        ASSERT(async->loop==g_uv_loop);
        g_uv_loop=nullptr;
    }

    uv_close((uv_handle_t *)async,&EmptyCloseCallback);

    for(HTTPConnection *conn:server->connections) {
        CloseConnection(conn);
    }

    uv_close((uv_handle_t *)&server->listen_tcp,&EmptyCloseCallback);
}

static void HandleConnection(uv_stream_t *server_tcp,int status) {
    auto server=(HTTPServer *)server_tcp->data;
    int rc;

    if(status!=0) {
        PrintLibUVError(status,"connection callback");
        return;
    }

    auto conn=new HTTPConnection;

    conn->parser_settings.on_url=&HandleHTTPURL;
    conn->parser_settings.on_header_field=&HandleHTTPHeaderField;
    conn->parser_settings.on_header_value=&HandleHTTPHeaderValue;
    conn->parser_settings.on_headers_complete=&HandleHTTPHeadersComplete;
    conn->parser_settings.on_body=&HandleHTTPBody;
    conn->parser_settings.on_message_complete=&HandleHTTPMessageComplete;
    conn->parser_settings.on_message_begin=&HandleHTTPMessageBegin;

    http_parser_init(&conn->parser,HTTP_REQUEST);
    conn->parser.data=conn;

    rc=uv_tcp_init(server_tcp->loop,&conn->tcp);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_tcp_init failed");
        delete conn;
        return;
    }

    rc=uv_accept(server_tcp,(uv_stream_t *)&conn->tcp);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_accept failed");
        delete conn;
        return;
    }

    conn->tcp.data=conn;
    conn->server=server;
    conn->server->connections.insert(conn);

    WaitForRequest(conn);
}

static void Run(int port) {
    int rc;
    uv_loop_t loop={};

    HTTPServer server;

    rc=uv_loop_init(&loop);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_loop_init failed");
        goto done;
    }

    loop.data=&server;

    rc=uv_tcp_init(&loop,&server.listen_tcp);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_tcp_init failed");
        goto done;
    }

    server.listen_tcp.data=&server;

    {
        struct sockaddr_in addr;
        uv_ip4_addr("127.0.0.1",port,&addr);
        rc=uv_tcp_bind(&server.listen_tcp,(struct sockaddr *)&addr,0);
        if(rc!=0) {
            PrintLibUVError(rc,"uv_tcp_bind failed");
            uv_close((uv_handle_t *)&server.listen_tcp,&EmptyCloseCallback);
        } else {
            rc=uv_listen((uv_stream_t *)&server.listen_tcp,10,&HandleConnection);
            if(rc!=0) {
                PrintLibUVError(rc,"uv_listen failed");
                uv_close((uv_handle_t *)&server.listen_tcp,&EmptyCloseCallback);
            }
        }
    }

    {
        std::lock_guard<Mutex> lock(g_http_mutex);
        g_uv_loop=&loop;
    }

    LOGF(HTTP,"Running libuv loop...\n");
    rc=uv_run(&loop,UV_RUN_DEFAULT);
    LOGF(HTTP,"uv_run result: %d\n",rc);

done:;
    if(loop.data) {
        uv_loop_close(&loop);
        loop.data=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool StartHTTPServer(int port) {
    StopHTTPServer();

    g_http_server_thread=std::thread([port]() {
        Run(port);
    });

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void StopHTTPServer() {
    {
        std::lock_guard<Mutex> lock(g_http_mutex);

        if(!g_uv_loop) {
            return;
        }

        uv_async_init(g_uv_loop,&g_stop_async,&StopHTTPServerAsyncCallback);
        uv_async_send(&g_stop_async);
    }

    g_http_server_thread.join();

    {
        std::lock_guard<Mutex> lock(g_http_mutex);

        ASSERT(!g_uv_loop);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string *HTTPRequest::GetHeaderValue(const std::string &key) const {
    auto &&it=this->headers.find(key);
    if(it==this->headers.end()) {
        return nullptr;
    } else {
        return &it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HTTPRequest::IsPOST() const {
    return this->method=="POST";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HTTPRequest::IsGET() const {
    return this->method=="GET";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::OK() {
    return HTTPResponse("200 OK");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse HTTPResponse::BadRequest(const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    std::string message=strprintfv(fmt,v);
    va_end(v);

    return HTTPResponse("400 Bad Request",TEXT_CONTENT_TYPE,std::move(message));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse():
    status("500 Internal Server Error")
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_):
    status(std::move(status_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string content_type,std::vector<uint8_t> content):
    HTTPResponse("200 OK",std::move(content_type),std::move(content))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string content_type,std::string content):
    HTTPResponse("200 OK",std::move(content_type),std::move(content))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_,std::string content_type_,std::vector<uint8_t> content):
    status(std::move(status_)),
    content_type(content_type_.empty()?DEFAULT_CONTENT_TYPE:std::move(content_type_)),
    content_vec(std::move(content))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_,std::string content_type_,std::string content):
    status(std::move(status_)),
    content_type(std::move(content_type_)),
    content_str(std::move(content))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
