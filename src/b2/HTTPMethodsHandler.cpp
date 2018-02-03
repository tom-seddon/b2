#include <shared/system.h>
#include "HTTPMethodsHandler.h"
#include "b2.h"
#include <utility>
#include <regex>
#include "HTTPServer.h"

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
    void HandleRequest(HTTPServer *server,HTTPRequest &&request) {
        //std::vector<std::string> parts=GetPathParts(request.url_path);
        server->SendResponse(request,HTTPResponse::BadRequest("hello hello"));
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPHandler> CreateHTTPMethodsHandler() {
    return std::make_unique<HTTPMethodsHandler>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//static std::string PART_RE="[^/]*";
//static std::string HEX_RE="[0-9A-Fa-f]+";
//
//static void HandleRequest(HTTPServer *server,HTTPRequest &&request) {
//    //std::vector<std::string> parts=GetPathParts(request.url_path);
//    //server->SendResponse(request,HTTPResponse::BadRequest("hello hello"));
//
//
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


//bool HTTPMethodsHandler::ThreadHandleRequest(HTTPResponse *response,HTTPServer *server,HTTPRequest &&request) {
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
