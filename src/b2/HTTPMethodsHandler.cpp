#include <shared/system.h>
#include "HTTPMethodsHandler.h"
#include "b2.h"
#include <utility>
#include <regex>
#include "HTTPServer.h"
#include "misc.h"
#include "BeebWindows.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <inttypes.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// somewhat arbitrary limit here...
static const uint64_t MAX_PEEK_SIZE=4*1024*1024;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadPeekMessage:
    public BeebThread::CustomMessage
{
public:
    BeebThreadPeekMessage(uint32_t addr,uint64_t count,HTTPResponse *response):
        m_addr(addr),
        m_count(count),
        m_response(response)
    {
        printf("BeebThreadPeekMessage: this=%p\n",(void *)this);
    }

    void ThreadHandleMessage(BBCMicro *beeb) override {
        uint32_t addr=m_addr;

        for(uint64_t i=0;i<m_count;++i) {
            int value=beeb->DebugGetByte(addr++);

            m_response->content_vec.push_back((uint8_t)value);
        }
    }
protected:
private:
    uint32_t m_addr=0;
    uint64_t m_count=0;
    HTTPResponse *m_response=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebThreadSendResponseMessage:
    public BeebThread::CustomMessage
{
public:
    BeebThreadSendResponseMessage(HTTPServer *server,uint64_t connection_id,std::unique_ptr<HTTPResponse> response):
        m_server(server),
        m_connection_id(connection_id),
        m_response(std::move(response))
    {
        printf("BeebThreadSendResponseMessage: this=%p\n",(void *)this);
    }

    void ThreadHandleMessage(BBCMicro *beeb) override {
        (void)beeb;

        m_server->SendResponse(m_connection_id,std::move(*m_response));
    }
protected:
private:
    HTTPServer *m_server=nullptr;
    uint64_t m_connection_id=0;
    std::unique_ptr<HTTPResponse> m_response;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPMethodsHandler:
    public HTTPHandler
{
    struct HandleRequestData {
        HTTPServer *server;
        HTTPRequest request;
    };
public:
    bool ThreadHandleRequest(HTTPResponse *response,HTTPServer *server,HTTPRequest &&request) {
        (void)response;

        auto data=new HandleRequestData{};

        data->server=server;
        data->request=std::move(request);

        PushFunctionMessage([this,data]() {
            HTTPServer *server=data->server;
            HTTPRequest request=std::move(data->request);

            delete data;

            this->HandleRequest(server,std::move(request));
        });

        return false;
    }
protected:
private:
    struct WindowRequests {
        // The request that assigned the Content-Type to the response
        // (for error reporting when there's a mismatch).
        std::string content_type_request;

        // The list of BeebThread messages that will be sent to the
        // BeebThread when the queue is flushed.
        //
        // Automatically topped off with a
        // BeebThreadSendResponseMessage that sends the response,
        // which is possibly just the 200 OK that it's initialised
        // with.
        std::vector<std::unique_ptr<BeebThread::Message>> messages;

        // The response to send. Initially a 200 OK, but filled in by
        // BeebThread messages that produce data.
        //
        // There's no check that messages that produce the same MIME
        // type are consistently filling out content_vec or
        // content_str, so don't mix and match.
        std::unique_ptr<HTTPResponse> response=std::make_unique<HTTPResponse>(HTTPResponse::OK());
    };

    std::map<std::string,WindowRequests> m_window_requests_by_name;

    // Get WindowRequests object and optionally BeebWindow pointer for
    // the given window.
    //
    // Send a Not Found and return nullptr if the window isn't found.
    WindowRequests *GetWindowRequestsOrSendResponse(BeebWindow **beeb_window_ptr,HTTPServer *server,const HTTPRequest &request,const std::string &window_name) {
        if(BeebWindow *beeb_window=BeebWindows::FindBeebWindowByName(window_name)) {
            if(beeb_window_ptr) {
                *beeb_window_ptr=beeb_window;
            }

            return &m_window_requests_by_name[window_name];
        } else {
            server->SendResponse(request,HTTPResponse::NotFound(request));
            return nullptr;
        }
    }

    // Set the Content-Type for this set of window requests. If no
    // Content-Type is set, it is set from this request; otherwise,
    // this request's Content-Type must match the previous requests'.
    //
    // Send an explanatory Bad Request and return false if there's a
    // mismatch.
    bool SetContentTypeOrSendResponse(HTTPServer *server,const HTTPRequest &request,WindowRequests *requests,const std::string &content_type,const std::string &content_type_request) {
        if(!content_type.empty()) {
            if(requests->response->content_type.empty()) {
                requests->response->content_type=content_type;
                requests->content_type_request=content_type_request;
            } else {
                if(requests->response->content_type!=content_type) {
                    server->SendResponse(request,HTTPResponse::BadRequest(request,"Content-Type mismatch\r\nContent-Type of previous %s request is %s\r\nContent-Type of new %s request is %s\r\n",
                                                                          requests->content_type_request.c_str(),requests->response->content_type.c_str(),
                                                                          content_type_request.c_str(),content_type.c_str()));
                    return false;
                }
            }
        }

        return true;
    }

    // Parse path parts.
    //
    // Send some kind of error and return false if there's a problem.
    bool ParseArgsOrSendResponse(HTTPServer *server,const HTTPRequest &request,const std::vector<std::string> &parts,size_t command_index,const char *partspec0,...) {
        va_list v;
        va_start(v,partspec0);

        const char *partspec=partspec0;

        size_t arg_index=command_index+1;
        while(arg_index<parts.size()) {
            if(!partspec) {
                server->SendResponse(request,HTTPResponse::BadRequest(request,"too few args for command: %s",parts[command_index].c_str()));
                return false;
            }

            if(partspec[0]==':') {
                if(strcmp(partspec+1,"x32")==0) {
                    auto u32=va_arg(v,uint32_t *);
                    if(!GetUInt32FromString(u32,parts[arg_index],16)) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request,"bad 32-bit hex value: %s",parts[arg_index].c_str()));
                        return false;
                    }
                } else if(strcmp(partspec+1,"x64")==0) {
                    auto u64=va_arg(v,uint64_t *);
                    if(!GetUInt64FromString(u64,parts[arg_index],16)) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request,"bad 64-bit hex value: %s",parts[arg_index].c_str()));
                        return false;
                    }
                } else if(strcmp(partspec+1,"x64/len")==0) {
                    auto u64=va_arg(v,uint64_t *);
                    auto is_len=va_arg(v,bool *);
                    size_t index=0;
                    if(parts[arg_index][index]=='+') {
                        *is_len=true;

                        if(!GetUInt64FromString(u64,parts[arg_index].c_str()+1)) {
                            server->SendResponse(request,HTTPResponse::BadRequest(request,"bad length: %s",parts[arg_index].c_str()));
                            return false;
                        }
                    } else {
                        *is_len=false;

                        if(!GetUInt64FromString(u64,parts[arg_index],16)) {
                            server->SendResponse(request,HTTPResponse::BadRequest(request,"bad address: %s",parts[arg_index].c_str()));
                            return false;
                        }
                    }
                } else {
                    ASSERT(false);
                    server->SendResponse(request,HTTPResponse());
                    return false;
                }
            } else {
                ASSERT(false);
                server->SendResponse(request,HTTPResponse());
                return false;
            }

            ++arg_index;
            partspec=va_arg(v,const char *);
        }

        va_end(v);

        return true;
    }

    // Create a suitable BeebThread message from the suffix of the URL
    // path. PATH_PARTS[COMMAND_INDEX] is the command name; subsequent
    // entries in PATH_PARTS are the args.
    //
    // Send some kind of error and return nullptr if there's a
    // problem.
    bool AddBeebThreadMessageOrSendResponse(HTTPServer *server,HTTPRequest *request,WindowRequests *requests,const std::vector<std::string> &path_parts,size_t command_index) {
        if(path_parts[command_index]=="peek") {
            if(!this->SetContentTypeOrSendResponse(server,*request,requests,HTTPResponse::OCTET_STREAM_CONTENT_TYPE,path_parts[command_index])) {
                return false;
            }

            uint32_t begin;
            uint64_t end;
            bool end_is_len;
            if(!this->ParseArgsOrSendResponse(server,*request,path_parts,command_index,
                                              ":x32",&begin,
                                              ":x64/len",&end,&end_is_len,
                                              nullptr))
            {
                return false;
            }

            if(end_is_len) {
                if(end>MAX_PEEK_SIZE) {
                    server->SendResponse(*request,HTTPResponse::BadRequest(*request,"count is too large: %" PRIu64,end));
                    return false;
                }

                end+=begin;
            } else {
                if(end<begin||end>1ull<<32) {
                    server->SendResponse(*request,HTTPResponse::BadRequest(*request,"bad end address: %" PRIu64,end));
                    return false;
                }
            }

            requests->messages.push_back(std::make_unique<BeebThreadPeekMessage>(begin,end-begin,requests->response.get()));
            return true;
        } else if(path_parts[command_index]=="poke") {
            uint32_t addr;
            if(!this->ParseArgsOrSendResponse(server,*request,path_parts,command_index,
                                              ":x32",&addr,
                                              nullptr))
            {
                return false;
            }

            requests->messages.push_back(std::make_unique<BeebThread::DebugSetBytesMessage>(addr,std::move(request->body)));
            return true;
        } else if(path_parts[command_index]=="reset") {
            requests->messages.push_back(std::make_unique<BeebThread::HardResetMessage>(false));//false = no autoboot
            return true;
        } else {
            server->SendResponse(*request,HTTPResponse::BadRequest(*request));
            return false;
        }
    }

    // Finish off the request list with an appropriate
    // BeebThreadSendResponseMessage, then submit the list to the
    // window's thread.
    //
    // After this call, *REQUESTS is a bit useless, as everything has
    // been moved out of it...
    void RunRequests(BeebWindow *beeb_window,HTTPServer *server,const HTTPRequest &request,WindowRequests *requests) {
        requests->messages.push_back(std::make_unique<BeebThreadSendResponseMessage>(server,request.connection_id,std::move(requests->response)));

        std::shared_ptr<BeebThread> beeb_thread=beeb_window->GetBeebThread();

        beeb_thread->Send(requests->messages.begin(),requests->messages.end());
    }

    void HandleRequest(HTTPServer *server,HTTPRequest &&request) {
        //std::vector<std::string> parts=GetPathParts(request.url_path);
        //server->SendResponse(request,HTTPResponse::BadRequest("hello hello"));

        std::vector<std::string> path_parts=GetPathParts(request.url_path);

        if(path_parts.empty()) {
            server->SendResponse(request,HTTPResponse::NotFound(request));
            return;
        }

        if(path_parts.size()==2&&path_parts[0]=="c") {
            if(!this->GetWindowRequestsOrSendResponse(nullptr,server,request,path_parts[1])) {
                return;
            }

            m_window_requests_by_name.erase(path_parts[1]);

            server->SendResponse(request,HTTPResponse::OK());
            return;
        } else if(path_parts.size()==2&&path_parts[0]=="r") {
            BeebWindow *beeb_window;
            WindowRequests *requests=this->GetWindowRequestsOrSendResponse(&beeb_window,server,request,path_parts[1]);
            if(!requests) {
                return;
            }

            this->RunRequests(beeb_window,server,request,requests);
            m_window_requests_by_name.erase(path_parts[1]);
            return;
        } else if(path_parts.size()>=3&&path_parts[0]=="q") {
            WindowRequests *requests=this->GetWindowRequestsOrSendResponse(nullptr,server,request,path_parts[1]);
            if(!requests) {
                return;
            }

            if(!this->AddBeebThreadMessageOrSendResponse(server,&request,requests,path_parts,2)) {
                return;
            }

            server->SendResponse(request,HTTPResponse::OK());
            return;
        } else if(path_parts.size()>=3&&path_parts[0]=="x") {
            WindowRequests tmp_requests;

            BeebWindow *beeb_window=BeebWindows::FindBeebWindowByName(path_parts[1]);
            if(!beeb_window) {
                server->SendResponse(request,HTTPResponse::NotFound(request));
                return;
            }

            if(!this->AddBeebThreadMessageOrSendResponse(server,&request,&tmp_requests,path_parts,2)) {
                return;
            }

            this->RunRequests(beeb_window,server,request,&tmp_requests);
            return;
        }

        server->SendResponse(request,HTTPResponse::BadRequest());
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPHandler> CreateHTTPMethodsHandler() {
    return std::make_unique<HTTPMethodsHandler>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
