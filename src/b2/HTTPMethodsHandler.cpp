#include <shared/system.h>
#include "HTTPMethodsHandler.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if HTTP_SERVER

#include "b2.h"
#include <utility>
#include <regex>
#include "HTTPServer.h"
#include "misc.h"
#include "BeebWindows.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <inttypes.h>
#include "MemoryDiscImage.h"
#include "Messages.h"
#include <shared/path.h>
#include "DiscGeometry.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// somewhat arbitrary limit here...
static const uint64_t MAX_PEEK_SIZE=4*1024*1024;

static const std::string PRG_CONTENT_TYPE="application/x-c64-program";
static const std::string PRG_EXTENSION=".prg";

static const std::string HTTP_DISC_IMAGE_LOAD_METHOD="http";

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
        LOGF(OUTPUT,"BeebThreadPeekMessage: this=%p\n",(void *)this);
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
    const std::map<std::string,void (HTTPMethodsHandler::*)(HTTPServer *,HTTPRequest &&,const std::vector<std::string> &,size_t)> m_request_handlers={
        {"reset",&HTTPMethodsHandler::HandleResetRequest},
        {"paste",&HTTPMethodsHandler::HandlePasteRequest},
        {"poke",&HTTPMethodsHandler::HandlePokeRequest},
        {"peek",&HTTPMethodsHandler::HandlePeekRequest},
        {"call",&HTTPMethodsHandler::HandleCallRequest},
        {"mount",&HTTPMethodsHandler::HandleMountRequest},
        {"run",&HTTPMethodsHandler::HandleRunRequest},
    };

    // Parse path parts.
    //
    // Send some kind of error and return false if there's a problem.
    bool ParseArgsOrSendResponse(HTTPServer *server,const HTTPRequest &request,const std::vector<std::string> &parts,size_t command_index,const char *partspec0,...) {
        va_list v;
        va_start(v,partspec0);

        bool result=this->ParseArgsOrSendResponse2(server,request,parts,command_index,partspec0,v);

        va_end(v);

        return result;
    }

    template<class T>
    bool HandleArgOrSendResponse(T *result,
                                 const std::string *value,
                                 bool (*f)(T *,const std::string &,int),
                                 int radix,
                                 HTTPServer *server,
                                 const HTTPRequest &request,
                                 const char *what)
    {
        if(value) {
            if(!(*f)(result,*value,radix)) {
                server->SendResponse(request,HTTPResponse::BadRequest(request,"bad %s: %s",what,value->c_str()));
                return false;
            }
        }

        return true;
    }

    bool ParseArgsOrSendResponse2(HTTPServer *server,const HTTPRequest &request,const std::vector<std::string> &parts,size_t command_index,const char *fmt0,va_list v) {
        size_t arg_index=command_index+1;

        for(const char *fmt=fmt0;fmt;fmt=va_arg(v,const char *)) {
            const char *name=va_arg(v,const char *);

            const std::string *value;
            if(name) {
                value=nullptr;

                for(const HTTPQueryParameter &q:request.query) {
                    if(q.key==name) {
                        value=&q.value;
                        break;
                    }
                }
            } else {
                value=&parts[arg_index++];
            }

            if(strcmp(fmt,"u8")==0) {
                if(!this->HandleArgOrSendResponse(va_arg(v,uint8_t *),value,&GetUInt8FromString,0,server,request,"8-bit value")) {
                    return false;
                }
            } else if(strcmp(fmt,"x16")==0) {
                if(!this->HandleArgOrSendResponse(va_arg(v,uint16_t *),value,&GetUInt16FromString,16,server,request,"16-bit hex value")) {
                    return false;
                }
            } else if(strcmp(fmt,"x32")==0) {
                if(!this->HandleArgOrSendResponse(va_arg(v,uint32_t *),value,&GetUInt32FromString,16,server,request,"32-bit hex value")) {
                    return false;
                }
            } else if(strcmp(fmt,"u32")==0) {
                if(!this->HandleArgOrSendResponse(va_arg(v,uint32_t *),value,&GetUInt32FromString,0,server,request,"32-bit value")) {
                    return false;
                }
            } else if(strcmp(fmt,"x64")==0) {
                if(!this->HandleArgOrSendResponse(va_arg(v,uint64_t *),value,&GetUInt64FromString,16,server,request,"64-bit hex value")) {
                    return false;
                }
            } else if(strcmp(fmt,"x64/len")==0) {
                auto u64=va_arg(v,uint64_t *);
                auto is_len=va_arg(v,bool *);
                size_t index=0;
                if(value) {
                    if((*value)[index]=='+') {
                        *is_len=true;

                        if(!GetUInt64FromString(u64,value->c_str()+1)) {
                            server->SendResponse(request,HTTPResponse::BadRequest(request,"bad length: %s",value->c_str()));
                            return false;
                        }
                    } else {
                        *is_len=false;

                        if(!GetUInt64FromString(u64,*value,16)) {
                            server->SendResponse(request,HTTPResponse::BadRequest(request,"bad address: %s",value->c_str()));
                            return false;
                        }
                    }
                }
            } else if(strcmp(fmt,"bool")==0) {
                auto b=va_arg(v,bool *);
                if(value) {
                    if(!GetBoolFromString(b,*value)) {
                        server->SendResponse(request,HTTPResponse::BadRequest(request,"bad bool value: %s",value->c_str()));
                        return false;
                    }
                }
            } else if(strcmp(fmt,"std::string")==0) {
                auto str=va_arg(v,std::string *);
                if(value) {
                    *str=*value;
                }
            } else if(strcmp(fmt,"window")==0) {
                auto ptr=va_arg(v,BeebWindow **);
                if(value) {
                    *ptr=BeebWindows::FindBeebWindowByName(*value);
                    if(!*ptr) {
                        server->SendResponse(request,HTTPResponse::NotFound(request));
                        return false;
                    }
                }
            } else {
                ASSERT(false);
                server->SendResponse(request,HTTPResponse());
                return false;
            }
        }

        if(arg_index!=parts.size()) {
            server->SendResponse(request,HTTPResponse::BadRequest(request,"too many arguments"));
            return false;
        }

        return true;
    }

    void HandleResetRequest(HTTPServer *server,HTTPRequest &&request,const std::vector<std::string> &path_parts,size_t command_index) {
        BeebWindow *beeb_window;
        if(!this->ParseArgsOrSendResponse(server,request,path_parts,command_index,
                                          "window",nullptr,&beeb_window,
                                          nullptr)) {
            return;
        }

        auto message=std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Run);

//        message->reload_config=true;
//        message->run=true;

        this->SendMessage(beeb_window,server,request,std::move(message));
    }

    void HandlePasteRequest(HTTPServer *server,HTTPRequest &&request,const std::vector<std::string> &path_parts,size_t command_index) {
        BeebWindow *beeb_window;
        if(!this->ParseArgsOrSendResponse(server,request,path_parts,command_index,
                                          "window",nullptr,&beeb_window,
                                          nullptr)) {
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

        this->SendMessage(beeb_window,server,request,std::make_shared<BeebThread::StartPasteMessage>(std::move(ascii)));
    }

    void HandlePokeRequest(HTTPServer *server,HTTPRequest &&request,const std::vector<std::string> &path_parts,size_t command_index) {
        BeebWindow *beeb_window;
        uint32_t addr;
        if(!this->ParseArgsOrSendResponse(server,request,path_parts,command_index,
                                          "window",nullptr,&beeb_window,
                                          "x32",nullptr,&addr,
                                          nullptr))
        {
            return;
        }

        this->SendMessage(beeb_window,server,request,std::make_shared<BeebThread::DebugSetBytesMessage>(addr,std::move(request.body)));
    }

    void HandlePeekRequest(HTTPServer *server,HTTPRequest &&request,const std::vector<std::string> &path_parts,size_t command_index) {
        BeebWindow *beeb_window;
        uint32_t begin;
        uint64_t end;
        bool end_is_len;
        if(!this->ParseArgsOrSendResponse(server,request,path_parts,command_index,
                                          "window",nullptr,&beeb_window,
                                          "x32",nullptr,&begin,
                                          "x64/len",nullptr,&end,&end_is_len,
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
            if(end<begin||end>1ull<<32||end-begin>MAX_PEEK_SIZE) {
                server->SendResponse(request,HTTPResponse::BadRequest(request,"bad end address: %" PRIu64,end));
                return;
            }
        }

        beeb_window->GetBeebThread()->Send(std::make_unique<BeebThreadPeekMessage>(begin,end-begin,server,std::move(request)));
    }

    void HandleCallRequest(HTTPServer *server,HTTPRequest &&request,const std::vector<std::string> &path_parts,size_t command_index) {
        BeebWindow *beeb_window;
        uint16_t addr;
        uint8_t a=0,x=0,y=0;
        bool c=false;
        if(!this->ParseArgsOrSendResponse(server,request,path_parts,command_index,
                                          "window",nullptr,&beeb_window,
                                          "x16",nullptr,&addr,
                                          "u8","a",&a,
                                          "u8","x",&x,
                                          "u8","y",&y,
                                          "bool","c",&c,
                                          nullptr))
        {
            return;
        }

        this->SendMessage(beeb_window,server,request,std::make_shared<BeebThread::DebugAsyncCallMessage>(addr,a,x,y,c));
    }

    void HandleMountRequest(HTTPServer *server,HTTPRequest &&request,const std::vector<std::string> &path_parts,size_t command_index) {
        BeebWindow *beeb_window;
        std::string name;
        uint32_t drive=0;
        if(!this->ParseArgsOrSendResponse(server,request,path_parts,command_index,
                                          "window",nullptr,&beeb_window,
                                          "std::string","name",&name,
                                          "u32","drive",&drive,
                                          nullptr))
        {
            return;
        }

        if(drive>=NUM_DRIVES) {
            server->SendResponse(request,HTTPResponse::BadRequest(request,"bad drive: %" PRIu32,drive));
            return;
        }

        std::shared_ptr<DiscImage> disc_image=LoadDiscImageFromRequestOrSendResponse(server,request,name);
        if(!disc_image) {
            return;
        }

        this->SendMessage(beeb_window,server,request,std::make_shared<BeebThread::LoadDiscMessage>((int)drive,std::move(disc_image),true));
    }

    void HandleRunRequest(HTTPServer *server,HTTPRequest &&request,const std::vector<std::string> &path_parts,size_t command_index) {
        BeebWindow *beeb_window;
        std::string name;
        if(!this->ParseArgsOrSendResponse(server,request,path_parts,command_index,
                                          "window",nullptr,&beeb_window,
                                          "std::string","name",&name,
                                          nullptr))
        {
            return;
        }

        // C64 .PRG.
        {
            if(request.content_type==PRG_CONTENT_TYPE||PathCompare(PathGetExtension(name),PRG_EXTENSION)==0) {
                if(request.body.size()<2) {
                    server->SendResponse(request,HTTPResponse::BadRequest(request));
                    return;
                }

                uint32_t addr=request.body[0]|(uint32_t)(request.body[1]<<8)|0xffff0000u;
                request.body.erase(request.body.begin(),request.body.begin()+2);

                beeb_window->GetBeebThread()->Send(std::make_shared<BeebThread::DebugSetBytesMessage>(addr,std::move(request.body)));
                this->SendMessage(beeb_window,server,request,std::make_shared<BeebThread::DebugAsyncCallMessage>(addr&0xffff,0,0,0,false));
                return;
            }
        }

        // BBC disc image.
        {
            std::shared_ptr<DiscImage> disc_image=this->LoadDiscImageFromRequestOrSendResponse(server,request,name);
            if(!disc_image) {
                return;
            }

            beeb_window->GetBeebThread()->Send(std::make_shared<BeebThread::LoadDiscMessage>(0,std::move(disc_image),true));

            auto message=std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Run|
                                                                                       BeebThreadHardResetFlag_Boot);
            this->SendMessage(beeb_window,server,request,std::move(message));
            return;
        }

        // Mystifying...
        server->SendResponse(request,HTTPResponse::BadRequest(request));
    }

    std::shared_ptr<DiscImage> LoadDiscImageFromRequestOrSendResponse(HTTPServer *server,const HTTPRequest &request,const std::string &name) {
        auto message_list=std::make_shared<MessageList>();
        Messages messages(message_list);

        DiscGeometry geometry;

        if(FindDiscGeometryFromMIMEType(&geometry,
                                        request.content_type.c_str(),
                                        request.body.size(),
                                        &messages))
        {
            // ok...
        } else if(!name.empty()&&
                  FindDiscGeometryFromFileDetails(&geometry,
                                                  name.c_str(),
                                                  request.body.size(),
                                                  &messages))
        {
            // ok...
        } else {
            this->SendMessagesResponse(server,request,message_list);
            return nullptr;
        }

        message_list->ClearMessages();

        std::shared_ptr<DiscImage> disc_image=MemoryDiscImage::LoadFromBuffer(name,HTTP_DISC_IMAGE_LOAD_METHOD,request.body.data(),request.body.size(),geometry,&messages);
        if(!disc_image) {
            this->SendMessagesResponse(server,request,message_list);
            return nullptr;
        }

        return disc_image;
    }

    void HandleRequest(HTTPServer *server,HTTPRequest &&request) {
        std::vector<std::string> path_parts=GetPathParts(request.url_path);

        if(path_parts.empty()) {
            server->SendResponse(request,HTTPResponse::NotFound(request));
            return;
        }

        if(!path_parts.empty()) {
            auto it=m_request_handlers.find(path_parts[0]);
            if(it==m_request_handlers.end()) {
                server->SendResponse(request,HTTPResponse::BadRequest(request,"Unknown request type: %s",path_parts[2].c_str()));
                return;
            }

            (this->*it->second)(server,std::move(request),path_parts,0);
            return;
        }

        server->SendResponse(request,HTTPResponse::BadRequest(request));
    }

    void SendMessagesResponse(HTTPServer *server,const HTTPRequest &request,const std::shared_ptr<MessageList> &message_list) {
        std::string text;

        message_list->ForEachMessage([&text](MessageList::Message *message) {
            if(!text.empty()) {
                text+="\r\n";
            }
            text+=message->text;
        });

        server->SendResponse(request,HTTPResponse::BadRequest(request,"%s",text.c_str()));
    }

    void SendMessage(BeebWindow *beeb_window,
                     HTTPServer *server,
                     const HTTPRequest &request,
                     std::shared_ptr<BeebThread::Message> message)
    {
        auto completion_fun=[server,response_data=request.response_data](bool success,
                                                                         std::string message) {
            LOGF(OUTPUT,"SendMessage completion_fun: connected ID=%" PRIu64 "\n",response_data.connection_id);

            HTTPResponse response;
            if(success) {
                response=HTTPResponse::OK();
            } else {
                response=HTTPResponse::ServiceUnavailable();
            }

            if(!message.empty()) {
                response.content_type=HTTP_TEXT_CONTENT_TYPE;
                response.content_str=std::move(message);
            }

            server->SendResponse(response_data,response);
        };

        std::shared_ptr<BeebThread> beeb_thread=beeb_window->GetBeebThread();
        beeb_thread->Send(std::move(message),std::move(completion_fun));
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPHandler> CreateHTTPMethodsHandler() {
    return std::make_unique<HTTPMethodsHandler>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
