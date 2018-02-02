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

static void PrintLibUVError(int rc,const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    LOGV(HTTP,fmt,v);
    va_end(v);

    LOGF(HTTP,": %s (%s)\n",uv_strerror(rc),uv_err_name(rc));
}

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

//static std::vector<std::string> GetPathParts(const std::string &path) {
//    std::vector<std::string> parts;
//    std::string part;
//
//    for(char c:path) {
//        if(c=='/') {
//            if(!part.empty()) {
//                parts.push_back(part);
//                part.clear();
//            }
//        } else {
//            part.append(1,c);
//        }
//    }
//
//    if(!part.empty()) {
//        parts.push_back(part);
//    }
//
//    return parts;
//}
//
//static bool FindBeebWindow(BeebWindow **window_ptr,const std::string &name) {
//    *window_ptr=BeebWindows::FindBeebWindowByName(name);
//    return !!*window_ptr;
//}
//
//struct Response {
//    HTTPResponse http_response;
//    std::unique_ptr<BeebThread::Message> message;
//    std::string content_type;
//
//    explicit Response(HTTPResponse http_response);
//    explicit Response(std::unique_ptr<BeebThread::Message> message);
//    Response(std::unique_ptr<BeebThread::Message> message,std::string content_type);
//};
//
//Response::Response(HTTPResponse http_response_):
//    http_response(std::move(http_response_))
//{
//}
//
//Response::Response(std::unique_ptr<BeebThread::Message> message_):
//    message(std::move(message_))
//{
//}
//
//Response::Response(std::unique_ptr<BeebThread::Message> message_,std::string content_type_):
//    message(std::move(message_)),
//    content_type(std::move(content_type_))
//{
//}
//
//static const char POKE[]="poke";
//static const char RESET[]="reset";
//
//static Response GetResponseForHTTPRequest(HTTPRequest *http_request,const std::vector<std::string> &path_parts,size_t index0) {
//    if(path_parts[index0]==POKE) {
//        if(http_request->body.empty()) {
//            return Response(HTTPResponse::BadRequest("no message body"));
//        }
//
//        if(path_parts.size()!=index0+2) {
//            return Response(HTTPResponse::BadRequest("syntax: %s/ADDR",POKE));
//        }
//
//        uint32_t addr;
//        if(!GetUInt32FromHexString(&addr,path_parts[index0+1])) {
//            return Response(HTTPResponse::BadRequest("bad address: %s",path_parts[index0+1].c_str()));
//        }
//
//        return Response(std::make_unique<BeebThread::DebugSetBytesMessage>(addr,std::move(http_request->body)));
//    } else if(path_parts[index0]==RESET) {
//        return Response(std::make_unique<BeebThread::HardResetMessage>(false));
//    } else {
//        return Response(HTTPResponse::BadRequest("unknown request: %s",path_parts[index0].c_str()));
//    }
//}
//
//static void DispatchRequestMainThread(HTTPConnection *conn) {
//    std::vector<std::string> parts=GetPathParts(conn->request.url_path);
//
//    //uint32_t value;
//    BeebWindow *window;
//
//    if(parts.size()==2&&parts[0]=="c") {
//        if(!FindBeebWindow(&window,parts[1])) {
//            SendResponse(conn,HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str()));
//            return;
//        }
//
//        conn->server->main_thread_data.window_queues.erase(parts[1]);
//    } else if(parts.size()>=3&&parts[0]=="q") {
//        if(!FindBeebWindow(&window,parts[1])) {
//            SendResponse(conn,HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str()));
//            return;
//        }
//
//        Response response=GetResponseForHTTPRequest(&conn->request,parts,2);
//
//        if(!response.message) {
//            SendResponse(conn,std::move(response.http_response));
//            return;
//        }
//
//        WindowQueue *window_queue=&conn->server->main_thread_data.window_queues[parts[1]];
//
//        if(!content_type.empty()) {
//            if(window_queue->messages.empty()) {
//                window_queue->content_type=std::move(content_type);
//                window_queue->content_type_request=parts[2];
//            } else {
//                if(window_queue->content_type!=content_type) {
//                    return HTTPResponse::BadRequest("result type conflict: %s type is %s, queued %s type is %s",
//                                                    parts[2],
//                                                    content_type.c_str(),
//                                                    window_queue->content_type_request.c_str(),
//                                                    window_queue->content_type.c_str());
//                }
//            }
//        }
//
//        window_queue->messages.push_back(std::move(message));
//    } else if(parts.size()>=3&&parts[0]=="x") {
//        if(!FindBeebWindow(&window,parts[1])) {
//            return HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str());
//        }
//
//        std::string content_type;
//        HTTPResponse response;
//        std::unique_ptr<BeebThread::Message> message=CreateMessageFromRequest(&content_type,&response,http_request,parts,2);
//        if(!message) {
//            return response;
//        }
//
//        response=HTTPResponse("200 OK",request->GetContentType());
//
//        window->GetBeebThread()->Send(std::move(message));
//
//        return response;
//    } else if(parts.size()==2&&parts[0]=="r") {
//        if(!FindBeebWindow(&window,parts[1])) {
//            return HTTPResponse::BadRequest("unknown window: %s",parts[1].c_str());
//        }
//
//        WindowQueue *window_queue=&main_thread_data->window_queues[parts[1]];
//
//        auto response=HTTPResponse("200 OK",window_queue->content_type);
//
//        for(const std::unique_ptr<Request> &request:window_queue->requests) {
//            request->Do(&response.content_vec,window);
//        }
//
//        window_queue=nullptr;
//        main_thread_data->window_queues.erase(parts[1]);
//
//        return response;
//    } else {
//        return HTTPResponse::BadRequest("unknown operation: %s",parts[0].c_str());
//    }
//}

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
    content_vec(std::move(content)),
    content_type(content_type_.empty()?DEFAULT_CONTENT_TYPE:std::move(content_type_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPResponse::HTTPResponse(std::string status_,std::string content_type_,std::string content):
    status(std::move(status_)),
    content_str(std::move(content)),
    content_type(std::move(content_type_))
{
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

class HTTPServerImpl:
    public HTTPServer
{
public:
    HTTPServerImpl();
    ~HTTPServerImpl();

    bool Start(int port) override;
    void SetHandler(HTTPHandler *handler) override;
protected:
private:
    struct Connection {
        uint64_t id=0;
        HTTPServerImpl *server=nullptr;
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

    struct ThreadData {
        uv_tcp_t listen_tcp{};
        uint64_t next_connection_id=1;
        std::map<uint64_t,Connection *> connection_by_id;
    };

    struct SharedData {
        Mutex mutex;
        uv_loop_t *loop=nullptr;
        HTTPHandler *handler=nullptr;
    };

    SharedData m_sd;
    ThreadData m_td;
    std::thread m_thread;

    void ThreadMain(int port);
    void CloseConnection(Connection *conn);
    void WaitForRequest(Connection *conn);
    void SendResponse(Connection *conn,HTTPResponse &&response);

    static void StopAsyncCallback(uv_async_t *stop_async);
    static int HandleMessageBegin(http_parser *parser);
    static int HandleURL(http_parser *parser,const char *at,size_t length);
    static int HandleHeaderField(http_parser *parser,const char *at,size_t length);
    static int HandleHeaderValue(http_parser *parser,const char *at,size_t length);
    static int HandleHeadersComplete(http_parser *parser);
    static int HandleBody(http_parser *parser,const char *at,size_t length);
    static int HandleMessageComplete(http_parser *parser);
    static void HandleNewConnection(uv_stream_t *server_tcp,int status);
    static void HandleReadAlloc(uv_handle_t *handle,size_t suggested_size,uv_buf_t *buf);
    static void HandleRead(uv_stream_t *stream,ssize_t num_read,const uv_buf_t *buf);
    static void HandleConnectionClose(uv_handle_t *handle);
    static void HandleResponseWritten(uv_write_t *req,int status);
};



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPServerImpl::HTTPServerImpl() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPServerImpl::~HTTPServerImpl() {
    {
        std::lock_guard<Mutex> sd_lock(m_sd.mutex);

        if(!m_sd.loop) {
            return;
        }

        auto stop_async=new uv_async_t{};
        stop_async->data=this;
        uv_async_init(m_sd.loop,stop_async,&StopAsyncCallback);
        uv_async_send(stop_async);
    }

    m_thread.join();

    {
        std::lock_guard<Mutex> sd_lock(m_sd.mutex);

        ASSERT(!m_sd.loop);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HTTPServerImpl::Start(int port) {
    {
        std::lock_guard<Mutex> sd_lock(m_sd.mutex);

        if(m_sd.loop) {
            return false;
        }
    }

    m_thread=std::thread([this,port]() {
        this->ThreadMain(port);
    });

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::SetHandler(HTTPHandler *handler) {
    std::lock_guard<Mutex> sd_lock(m_sd.mutex);

    m_sd.handler=handler;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::ThreadMain(int port) {
    int rc;

    uv_loop_t loop={};

    rc=uv_loop_init(&loop);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_loop_init failed");
        goto done;
    }

    loop.data=this;

    rc=uv_tcp_init(&loop,&m_td.listen_tcp);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_tcp_init failed");
        goto done;
    }

    m_td.listen_tcp.data=this;

    {
        struct sockaddr_in addr;
        uv_ip4_addr("127.0.0.1",port,&addr);
        rc=uv_tcp_bind(&m_td.listen_tcp,(struct sockaddr *)&addr,0);
        if(rc!=0) {
            PrintLibUVError(rc,"uv_tcp_bind failed");
            uv_close((uv_handle_t *)&m_td.listen_tcp,&EmptyCloseCallback);
        } else {
            rc=uv_listen((uv_stream_t *)&m_td.listen_tcp,10,&HandleNewConnection);
            if(rc!=0) {
                PrintLibUVError(rc,"uv_listen failed");
                uv_close((uv_handle_t *)&m_td.listen_tcp,&EmptyCloseCallback);
            }
        }
    }

    {
        std::lock_guard<Mutex> sd_lock(m_sd.mutex);
        m_sd.loop=&loop;
    }

    rc=uv_run(&loop,UV_RUN_DEFAULT);

done:
    {
        std::lock_guard<Mutex> sd_lock(m_sd.mutex);
        m_sd.loop=nullptr;
    }

    if(loop.data) {
        uv_loop_close(&loop);
        loop.data=nullptr;
    }

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::CloseConnection(Connection *conn) {
    ASSERT(m_td.connection_by_id.count(conn->id)==1);
    ASSERT(m_td.connection_by_id[conn->id]==conn);

    m_td.connection_by_id.erase(conn->id);

    uv_close((uv_handle_t *)&conn->tcp,&HandleConnectionClose);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::WaitForRequest(Connection *conn) {
    int rc;

    conn->key.clear();
    conn->value=nullptr;
    conn->request=HTTPRequest();
    conn->response_body_data.clear();
    conn->response_body_str.clear();
    conn->response_prefix.clear();

    rc=uv_read_start((uv_stream_t *)&conn->tcp,&HandleReadAlloc,&HandleRead);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_read_start failed");
        this->CloseConnection(conn);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::SendResponse(Connection *conn,HTTPResponse &&response) {
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

    rc=uv_write(&conn->write_response_req,(uv_stream_t *)&conn->tcp,bufs.data(),(unsigned)bufs.size(),&HandleResponseWritten);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_write failed");
        this->CloseConnection(conn);
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::StopAsyncCallback(uv_async_t *stop_async) {
    auto server=(HTTPServerImpl *)stop_async->loop->data;

    uv_print_all_handles(stop_async->loop,stderr);

    {
        std::lock_guard<Mutex> lock(server->m_sd.mutex);
        ASSERT(stop_async->loop==server->m_sd.loop);
        server->m_sd.loop=nullptr;
    }

    while(!server->m_td.connection_by_id.empty()) {
        server->CloseConnection(server->m_td.connection_by_id.begin()->second);
    }

    uv_close((uv_handle_t *)&server->m_td.listen_tcp,&EmptyCloseCallback);
    uv_close((uv_handle_t *)stop_async,&ScalarDeleteCloseCallback<uv_async_t>);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleMessageBegin(http_parser *parser) {
    (void)parser;
    //auto conn=(HTTPConnection *)parser->data;

    LOGF(HTTP,"%s\n",__func__);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleURL(http_parser *parser,const char *at,size_t length) {
    auto conn=(Connection *)parser->data;

    conn->request.url.append(at,length);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleHeaderField(http_parser *parser,const char *at,size_t length) {
    auto conn=(Connection *)parser->data;

    conn->value=nullptr;
    conn->key.append(at,length);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleHeaderValue(http_parser *parser,const char *at,size_t length) {
    auto conn=(Connection *)parser->data;

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleHeadersComplete(http_parser *parser) {
    auto conn=(Connection *)parser->data;

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleBody(http_parser *parser,const char *at,size_t length) {
    auto conn=(Connection *)parser->data;

    conn->request.body.insert(conn->request.body.end(),at,at+length);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int HTTPServerImpl::HandleMessageComplete(http_parser *parser) {
    int rc;
    auto conn=(Connection *)parser->data;

    conn->keep_alive=!!http_should_keep_alive(parser);

    rc=uv_read_stop((uv_stream_t *)&conn->tcp);
    if(rc!=0) {
        PrintLibUVError(rc,"uv_read_stop failed");
        conn->server->CloseConnection(conn);
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

    HTTPHandler *handler;
    {
        std::lock_guard<Mutex> sd_lock(conn->server->m_sd.mutex);

        handler=conn->server->m_sd.handler;
    }

    HTTPResponse response;
    bool send_response=false;
    if(handler) {
        send_response=handler->ThreadHandleRequest(&response,std::move(conn->request));
    } else {
        response=CreateErrorResponse(conn->request,"404 Not Found");
        send_response=true;
    }

    if(send_response) {
        conn->server->SendResponse(conn,std::move(response));
    }

    //conn->status="404 Not Found";

    //if(!conn->status.empty()) {
        //SendResponse(conn,CreateErrorResponse(conn->request,"404 Not Found"));
    //}

    //PushFunctionMessage([conn]() {
    //    DispatchRequestMainThread(conn);
    //});

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleNewConnection(uv_stream_t *server_tcp,int status) {
    auto server=(HTTPServerImpl *)server_tcp->data;
    int rc;

    if(status!=0) {
        PrintLibUVError(status,"connection callback");
        return;
    }

    auto conn=new Connection;

    conn->id=server->m_td.next_connection_id++;
    conn->parser_settings.on_url=&HandleURL;
    conn->parser_settings.on_header_field=&HandleHeaderField;
    conn->parser_settings.on_header_value=&HandleHeaderValue;
    conn->parser_settings.on_headers_complete=&HandleHeadersComplete;
    conn->parser_settings.on_body=&HandleBody;
    conn->parser_settings.on_message_complete=&HandleMessageComplete;
    conn->parser_settings.on_message_begin=&HandleMessageBegin;

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

    ASSERT(conn->server->m_td.connection_by_id.count(conn->id)==0);
    conn->server->m_td.connection_by_id[conn->id]=conn;

    conn->server->WaitForRequest(conn);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleReadAlloc(uv_handle_t *handle,size_t suggested_size,uv_buf_t *buf) {
    auto conn=(Connection *)handle->data;
    (void)suggested_size;

    ASSERT(handle==(uv_handle_t *)&conn->tcp);
    ASSERT(conn->num_read<sizeof conn->read_buf);
    static_assert(sizeof conn->read_buf<=UINT_MAX,"");
    *buf=uv_buf_init(conn->read_buf+conn->num_read,(unsigned)(sizeof conn->read_buf-conn->num_read));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleRead(uv_stream_t *stream,ssize_t num_read,const uv_buf_t *buf) {
    (void)buf;

    auto conn=(Connection *)stream->data;
    ASSERT(stream==(uv_stream_t *)&conn->tcp);

    if(num_read==UV_EOF) {
        conn->server->CloseConnection(conn);
    } else if(num_read<0) {
        PrintLibUVError((int)num_read,"connection read callback");
        conn->server->CloseConnection(conn);
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
            conn->server->SendResponse(conn,CreateErrorResponse(conn->request,"400 Bad Request"));
            //CloseConnection(conn);
        } else {
            ASSERT(num_consumed<=total_num_read);
            memmove(conn->read_buf,conn->read_buf+num_consumed,total_num_read-num_consumed);
            conn->num_read=total_num_read-num_consumed;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleConnectionClose(uv_handle_t *handle) {
    auto conn=(Connection *)handle->data;
    ASSERT(handle==(uv_handle_t *)&conn->tcp);
    //HTTPServer *server=conn->server;

    //ASSERT(conn->server->connections.count(conn)==1);
    //conn->server->connections.erase(conn);

    delete conn;
    conn=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void HTTPServerImpl::HandleResponseWritten(uv_write_t *req,int status) {
    auto conn=(Connection *)req->data;

    ASSERT(req==&conn->write_response_req);
    req->data=nullptr;

    if(status!=0) {
        PrintLibUVError(status,"%s status",__func__);
        conn->server->CloseConnection(conn);
        return;
    }

    if(conn->keep_alive&&conn->parser.http_errno==0) {
        conn->server->WaitForRequest(conn);
    } else {
        conn->server->CloseConnection(conn);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPServer> CreateHTTPServer() {
    return std::make_unique<HTTPServerImpl>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
