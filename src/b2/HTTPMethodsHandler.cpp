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

static const std::string PRG_CONTENT_TYPE="application/x-c64-program";

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
    BeebThreadPeekMessage(uint32_t addr,uint64_t count,HTTPServer *server,HTTPRequest &&request):
        m_addr(addr),
        m_count(count),
        m_server(server),
        m_response_data(request.response_data)
    {
        printf("BeebThreadPeekMessage: this=%p\n",(void *)this);
    }

    void ThreadHandleMessage(BBCMicro *beeb) override {
        HTTPResponse m_response;
        uint32_t addr=m_addr;

        std::vector<uint8_t> data;
        data.reserve(m_count);

        for(uint64_t i=0;i<m_count;++i) {
            int value=beeb->DebugGetByte(addr++);

            data.push_back((uint8_t)value);
        }

        HTTPResponse response(HTTP_OCTET_STREAM_CONTENT_TYPE,std::move(data));
        m_server->SendResponse(m_response_data,std::move(response));
    }
protected:
private:
    uint32_t m_addr=0;
    uint64_t m_count=0;
    HTTPServer *m_server=nullptr;
    HTTPResponseData m_response_data;
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

        if(arg_index!=parts.size()) {
            server->SendResponse(request,HTTPResponse::BadRequest(request,"too many arguments"));
            return false;
        }

        return true;
    }

    void HandleRequest(HTTPServer *server,HTTPRequest &&request) {
        //std::vector<std::string> parts=GetPathParts(request.url_path);
        //server->SendResponse(request,HTTPResponse::BadRequest("hello hello"));

        std::vector<std::string> path_parts=GetPathParts(request.url_path);

        if(path_parts.empty()) {
            server->SendResponse(request,HTTPResponse::NotFound(request));
            return;
        }

        if(path_parts.size()>=3&&path_parts[0]=="w") {
            BeebWindow *beeb_window=BeebWindows::FindBeebWindowByName(path_parts[1]);
            if(!beeb_window) {
                server->SendResponse(request,HTTPResponse::NotFound(request));
                return;
            }

            std::shared_ptr<BeebThread> beeb_thread=beeb_window->GetBeebThread();

            if(path_parts[2]=="reset") {
                if(!this->ParseArgsOrSendResponse(server,request,path_parts,2,nullptr)) {
                    return;
                }

                this->SendMessage(beeb_thread,server,request,std::make_unique<BeebThread::HardResetMessage>(false));
                return;
            } else if(path_parts[2]=="paste") {
                if(!this->ParseArgsOrSendResponse(server,request,path_parts,2,nullptr)) {
                    return;
                }

                std::string ascii;
                if(request.content_type==HTTP_TEXT_CONTENT_TYPE&&(request.content_type_charset.empty()||request.content_type_charset==HTTP_ISO_8859_1_CHARSET)) {
                    if(GetBBCASCIIFromISO88511(&ascii,request.body)!=0) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request));
                        return;
                    }
                } else if(request.content_type==HTTP_TEXT_CONTENT_TYPE&&request.content_type_charset==HTTP_UTF8_CHARSET) {
                    if(!GetBBCASCIIFromUTF8(&ascii,request.body,nullptr,nullptr,nullptr)) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request));
                        return;
                    }
                } else {
                    // Maybe support octet-stream?? Like, if you've got
                    // verbatim *SPOOL output from a real BBC or something?
                    server->SendResponse(request,HTTPResponse::BadRequest(request,"Unsupported Content-Type \"%s\", charset \"%s\"\n",request.content_type.c_str(),request.content_type_charset.c_str()));
                    return;
                }

                FixBBCASCIINewlines(&ascii);

                this->SendMessage(beeb_thread,server,request,std::make_unique<BeebThread::StartPasteMessage>(std::move(ascii)));
                return;
            } else if(path_parts[2]=="poke") {
                std::vector<uint8_t> data;
                uint32_t addr;
                if(request.content_type==PRG_CONTENT_TYPE) {
                    if(data.size()<2) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request));
                        return;
                    }

                    addr=data[0]|data[1]<<8|0xffff0000;
                    data=std::move(request.body);
                    data.erase(data.begin(),data.begin()+2);
                } else {
                    if(!this->ParseArgsOrSendResponse(server,request,path_parts,2,
                                                      ":x32",&addr,
                                                      nullptr))
                    {
                        return;
                    }

                    data=std::move(request.body);
                }

                this->SendMessage(beeb_thread,server,request,std::make_unique<BeebThread::DebugSetBytesMessage>(addr,std::move(data)));
                return;
            } else if(path_parts[2]=="peek") {
                uint32_t begin;
                uint64_t end;
                bool end_is_len;
                if(!this->ParseArgsOrSendResponse(server,request,path_parts,2,
                                                  ":x32",&begin,
                                                  ":x64/len",&end,&end_is_len,
                                                  nullptr))
                {
                    return;
                }

                if(end_is_len) {
                    if(end>MAX_PEEK_SIZE) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request,"count is too large: %" PRIu64,end));
                        return;
                    }

                    end+=begin;
                } else {
                    if(end<begin||end>1ull<<32) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request,"bad end address: %" PRIu64,end));
                        return;
                    }
                }

                beeb_thread->Send(std::make_unique<BeebThreadPeekMessage>(begin,end-begin,server,std::move(request)));
                return;
            }
        }

        server->SendResponse(request,HTTPResponse::BadRequest());
    }

    void SendMessage(const std::shared_ptr<BeebThread> &beeb_thread,HTTPServer *server,const HTTPRequest &request,std::unique_ptr<BeebThread::Message> message) {
        message->completion_fun=[server,response_data=request.response_data](bool success) {
            if(success) {
                server->SendResponse(response_data,HTTPResponse::OK());
            } else {
                server->SendResponse(response_data,HTTPResponse::ServiceUnavailable());
            }
        };

        beeb_thread->Send(std::move(message));
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPHandler> CreateHTTPMethodsHandler() {
    return std::make_unique<HTTPMethodsHandler>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
