#include <shared/system.h>
#include "HTTPMethodsHandler.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "b2.h"
#include <utility>
#include <regex>
#include "HTTPServer.h"
#include "misc.h"
#include "BeebWindows.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include <inttypes.h>
#include <beeb/MemoryDiscImage.h>
#include "Messages.h"
#include <shared/path.h>
#include <beeb/DiscGeometry.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string HTTP_DISC_IMAGE_LOAD_METHOD = "http";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<std::string> GetPathParts(const std::string &path) {
    std::vector<std::string> parts;
    std::string part;

    for (char c : path) {
        if (c == '/') {
            if (!part.empty()) {
                parts.push_back(part);
                part.clear();
            }
        } else {
            part.append(1, c);
        }
    }

    if (!part.empty()) {
        parts.push_back(part);
    }

    return parts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPMethodsHandler : public HTTPHandler {
    struct HandleRequestData {
        HTTPServer *server;
        HTTPRequest request;
    };

  public:
    bool ThreadHandleRequest(HTTPResponse *response, HTTPServer *server, HTTPRequest &&request) {
        (void)response;

        auto data = new HandleRequestData{};

        data->server = server;
        data->request = std::move(request);

        PushFunctionMessage([this, data]() {
            HTTPServer *server = data->server;
            HTTPRequest request = std::move(data->request);

            delete data;

            this->HandleRequest(server, std::move(request));
        });

        return false;
    }

  protected:
  private:
    const std::map<std::string, void (HTTPMethodsHandler::*)(HTTPServer *, HTTPRequest &&, const std::vector<std::string> &, size_t)> m_request_handlers = {
#if BBCMICRO_DEBUGGER
        {"reset", &HTTPMethodsHandler::HandleResetRequest},
        {"paste", &HTTPMethodsHandler::HandlePasteRequest},
        {"poke", &HTTPMethodsHandler::HandlePokeRequest},
        {"peek", &HTTPMethodsHandler::HandlePeekRequest},
        {"mount", &HTTPMethodsHandler::HandleMountRequest},
        {"run", &HTTPMethodsHandler::HandleRunRequest},
#endif
        {"launch", &HTTPMethodsHandler::HandleLaunchRequest},
    };

    // Parse path parts.
    //
    // Send some kind of error and return false if there's a problem.
    bool ParseArgsOrSendResponse(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &parts, size_t command_index, const char *partspec0, ...) {
        va_list v;
        va_start(v, partspec0);

        bool result = this->ParseArgsOrSendResponse2(server, request, parts, command_index, partspec0, v);

        va_end(v);

        return result;
    }

    template <class T>
    bool HandleArgOrSendResponse(T *result,
                                 const std::string *value,
                                 bool (*f)(T *, const std::string &, int, const char **),
                                 int radix,
                                 HTTPServer *server,
                                 const HTTPRequest &request,
                                 const char *what) {
        if (value) {
            if (!(*f)(result, *value, radix, nullptr)) {
                server->SendResponse(request, HTTPResponse::BadRequest(request, "bad %s: %s", what, value->c_str()));
                return false;
            }
        }

        return true;
    }

    bool ParseArgsOrSendResponse2(HTTPServer *server, const HTTPRequest &request, const std::vector<std::string> &parts, size_t command_index, const char *fmt0, va_list v) {
        size_t arg_index = command_index + 1;
        BeebWindow *beeb_window = nullptr;

        std::string log_string;
        LogPrinterString log_printer_string(&log_string);
        Log log("", &log_printer_string);

        for (const char *fmt = fmt0; fmt; fmt = va_arg(v, const char *)) {
            const char *name = va_arg(v, const char *);

            const std::string *value;
            if (name) {
                value = nullptr;

                for (const HTTPQueryParameter &q : request.query) {
                    if (q.key == name) {
                        value = &q.value;
                        break;
                    }
                }
            } else {
                value = &parts[arg_index++];
            }

            if (strcmp(fmt, "u8") == 0) {
                if (!this->HandleArgOrSendResponse(va_arg(v, uint8_t *), value, &GetUInt8FromString, 0, server, request, "8-bit value")) {
                    return false;
                }
            } else if (strcmp(fmt, "x16") == 0) {
                if (!this->HandleArgOrSendResponse(va_arg(v, uint16_t *), value, &GetUInt16FromString, 16, server, request, "16-bit hex value")) {
                    return false;
                }
            } else if (strcmp(fmt, "x32") == 0) {
                if (!this->HandleArgOrSendResponse(va_arg(v, uint32_t *), value, &GetUInt32FromString, 16, server, request, "32-bit hex value")) {
                    return false;
                }
            } else if (strcmp(fmt, "u32") == 0) {
                if (!this->HandleArgOrSendResponse(va_arg(v, uint32_t *), value, &GetUInt32FromString, 0, server, request, "32-bit value")) {
                    return false;
                }
            } else if (strcmp(fmt, "x64") == 0) {
                if (!this->HandleArgOrSendResponse(va_arg(v, uint64_t *), value, &GetUInt64FromString, 16, server, request, "64-bit hex value")) {
                    return false;
                }
            } else if (strcmp(fmt, "x64/len") == 0) {
                auto u64 = va_arg(v, uint64_t *);
                auto is_len = va_arg(v, bool *);
                size_t index = 0;
                if (value) {
                    if ((*value)[index] == '+') {
                        *is_len = true;

                        if (!GetUInt64FromString(u64, value->c_str() + 1)) {
                            server->SendResponse(request, HTTPResponse::BadRequest(request, "bad length: %s", value->c_str()));
                            return false;
                        }
                    } else {
                        *is_len = false;

                        if (!GetUInt64FromString(u64, *value, 16)) {
                            server->SendResponse(request, HTTPResponse::BadRequest(request, "bad address: %s", value->c_str()));
                            return false;
                        }
                    }
                }
            } else if (strcmp(fmt, "bool") == 0) {
                auto b = va_arg(v, bool *);
                if (value) {
                    if (!GetBoolFromString(b, *value)) {
                        server->SendResponse(request, HTTPResponse::BadRequest(request, "bad bool value: %s", value->c_str()));
                        return false;
                    }
                }
            } else if (strcmp(fmt, "std::string") == 0) {
                auto str = va_arg(v, std::string *);
                if (value) {
                    *str = *value;
                }
            } else if (strcmp(fmt, "window") == 0) {
                auto ptr = va_arg(v, BeebWindow **);
                if (value) {
                    *ptr = BeebWindows::FindBeebWindowByName(*value);
                    if (!*ptr) {
                        server->SendResponse(request, HTTPResponse::NotFound(request));
                        return false;
                    }
                }
                if (!beeb_window) {
                    beeb_window = *ptr;
                }
            }                                   //<-- note
#if BBCMICRO_DEBUGGER                           //<-- note
            else if (strcmp(fmt, "dso") == 0) { //<-- note
                // Paging overrides. The BBCMicroType to use is inferred from
                // the first `window' type seen.
                auto ptr = va_arg(v, uint32_t *);
                ASSERT(beeb_window);
                if (value) {
                    std::shared_ptr<const BBCMicroReadOnlyState> state;
                    beeb_window->GetBeebThread()->DebugGetState(&state, nullptr);

                    if (!ParseAddressSuffix(ptr,
                                            state->type,
                                            value->c_str(),
                                            &log)) {
                        server->SendResponse(request, HTTPResponse::BadRequest(request, "%s", log_string.c_str()));
                        return false;
                    }
                }
            }      //<-- note
#endif             //<-- note
            else { //<-- note
                ASSERT(false);
                server->SendResponse(request, HTTPResponse());
                return false;
            }
        }

        if (arg_index != parts.size()) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "too many arguments"));
            return false;
        }

        return true;
    }

#if BBCMICRO_DEBUGGER
    void HandleResetRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string config_name;
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index,
                                           "window", nullptr, &beeb_window,
                                           "std::string", "config", &config_name,
                                           nullptr)) {
            return;
        }

        if (!config_name.empty()) {
            auto message_list = std::make_shared<MessageList>("HTTP reset");
            Messages messages(message_list);

            BeebLoadedConfig loaded_config;
            if (!BeebWindows::LoadConfigByName(&loaded_config, config_name, &messages)) {
                this->SendMessagesResponse(server, request, message_list);
                return;
            }

            auto config_message = std::make_shared<BeebThread::HardResetAndChangeConfigMessage>(BeebThreadHardResetFlag_Run,
                                                                                                std::move(loaded_config));
            beeb_window->GetBeebThread()->Send(std::move(config_message));
        }

        auto reset_message = std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Run);

        //        message->reload_config=true;
        //        message->run=true;

        this->SendMessage(beeb_window, server, request, std::move(reset_message));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandlePasteRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index,
                                           "window", nullptr, &beeb_window,
                                           nullptr)) {
            return;
        }

        std::string ascii;
        if (request.content_type == HTTP_TEXT_CONTENT_TYPE && (request.content_type_charset.empty() || request.content_type_charset == HTTP_ISO_8859_1_CHARSET)) {
            if (GetBBCASCIIFromISO8859_1(&ascii, request.body) != 0) {
                server->SendResponse(request, HTTPResponse::BadRequest(request));
                return;
            }
        } else if (request.content_type == HTTP_TEXT_CONTENT_TYPE && request.content_type_charset == HTTP_UTF8_CHARSET) {
            if (!GetBBCASCIIFromUTF8(&ascii, request.body, nullptr, nullptr, nullptr)) {
                server->SendResponse(request, HTTPResponse::BadRequest(request));
                return;
            }
        } else {
            // Maybe support octet-stream?? Like, if you've got
            // verbatim *SPOOL output from a real BBC or something?
            server->SendResponse(request, HTTPResponse::BadRequest(request, "Unsupported Content-Type \"%s\", charset \"%s\"\n", request.content_type.c_str(), request.content_type_charset.c_str()));
            return;
        }

        FixBBCASCIINewlines(&ascii);

        this->SendMessage(beeb_window, server, request, std::make_shared<BeebThread::StartPasteMessage>(std::move(ascii)));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandlePokeRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        uint16_t addr;
        uint32_t dso = 0;
        bool mos = false;
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index,
                                           "window", nullptr, &beeb_window,
                                           "x16", nullptr, &addr,
                                           "dso", "s", &dso,
                                           "bool", "mos", &mos,
                                           nullptr)) {
            return;
        }

        if (addr + request.body.size() > 0x10000) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "can't poke past 0xffff"));
            return;
        }

        this->SendMessage(beeb_window, server, request, std::make_shared<BeebThread::DebugSetBytesMessage>(addr, dso, mos, std::move(request.body)));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandlePeekRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        uint16_t begin;
        uint64_t end;
        bool end_is_len;
        uint32_t dso = 0;
        bool mos = false;
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index,
                                           "window", nullptr, &beeb_window,
                                           "x16", nullptr, &begin,
                                           "x64/len", nullptr, &end, &end_is_len,
                                           "dso", "s", &dso,
                                           "bool", "mos", &mos,
                                           nullptr)) {
            return;
        }

        if (end_is_len) {
            end += begin;
        }

        if (end > 0x10000) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "can't peek past 0xffff"));
            return;
        }

        beeb_window->GetBeebThread()->Send(std::make_unique<BeebThread::CallbackMessage>([begin, end, dso, mos, server, response_data = request.response_data](BBCMicro *m) -> void {
            std::vector<uint8_t> data;
            data.resize(end - begin);

            m->DebugGetBytes(data.data(), data.size(), {begin}, dso, mos);

            HTTPResponse response(HTTP_OCTET_STREAM_CONTENT_TYPE, std::move(data));
            server->SendResponse(response_data, std::move(response));
        }));
    }

#endif

#if BBCMICRO_DEBUGGER
    void HandleMountRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string name;
        uint32_t drive = 0;
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index,
                                           "window", nullptr, &beeb_window,
                                           "std::string", "name", &name,
                                           "u32", "drive", &drive,
                                           nullptr)) {
            return;
        }

        if (drive >= NUM_DRIVES) {
            server->SendResponse(request, HTTPResponse::BadRequest(request, "bad drive: %" PRIu32, drive));
            return;
        }

        std::shared_ptr<DiscImage> disc_image = LoadDiscImageFromRequestOrSendResponse(server, request, name);
        if (!disc_image) {
            return;
        }

        this->SendMessage(beeb_window, server, request, std::make_shared<BeebThread::LoadDiscMessage>((int)drive, std::move(disc_image), true));
    }
#endif

#if BBCMICRO_DEBUGGER
    void HandleRunRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        BeebWindow *beeb_window;
        std::string name;
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index,
                                           "window", nullptr, &beeb_window,
                                           "std::string", "name", &name,
                                           nullptr)) {
            return;
        }

        // Assume BBC disc image.
        std::shared_ptr<DiscImage> disc_image = this->LoadDiscImageFromRequestOrSendResponse(server, request, name);
        if (!disc_image) {
            return;
        }

        beeb_window->GetBeebThread()->Send(std::make_shared<BeebThread::LoadDiscMessage>(0, std::move(disc_image), true));

        auto message = std::make_shared<BeebThread::HardResetAndReloadConfigMessage>(BeebThreadHardResetFlag_Run |
                                                                                     BeebThreadHardResetFlag_Boot);
        this->SendMessage(beeb_window, server, request, std::move(message));
        return;
    }
#endif

    void HandleLaunchRequest(HTTPServer *server, HTTPRequest &&request, const std::vector<std::string> &path_parts, size_t command_index) {
        std::string path;
        if (!this->ParseArgsOrSendResponse(server, request, path_parts, command_index,
                                           "std::string", "path", &path,
                                           nullptr)) {
            return;
        }

        BeebWindowLaunchArguments arguments;
        arguments.file_path = path;
        arguments.type = BeebWindowLaunchType_UseExistingProcess;

        BeebWindow *beeb_window = BeebWindows::FindMRUBeebWindow();
        beeb_window->Launch(arguments);

        server->SendResponse(request, HTTPResponse::OK());
    }

    std::shared_ptr<DiscImage> LoadDiscImageFromRequestOrSendResponse(HTTPServer *server, const HTTPRequest &request, const std::string &name) {
        auto message_list = std::make_shared<MessageList>("HTTP disc image request");
        Messages messages(message_list);

        DiscGeometry geometry;

        if (FindDiscGeometryFromMIMEType(&geometry,
                                         request.content_type.c_str(),
                                         request.body.size(),
                                         messages)) {
            // ok...
        } else if (!name.empty() &&
                   FindDiscGeometryFromFileDetails(&geometry,
                                                   name.c_str(),
                                                   request.body.size(),
                                                   &messages)) {
            // ok...
        } else {
            this->SendMessagesResponse(server, request, message_list);
            return nullptr;
        }

        message_list->ClearMessages();

        std::shared_ptr<DiscImage> disc_image = MemoryDiscImage::LoadFromBuffer(name, HTTP_DISC_IMAGE_LOAD_METHOD, request.body.data(), request.body.size(), geometry, messages);
        if (!disc_image) {
            this->SendMessagesResponse(server, request, message_list);
            return nullptr;
        }

        return disc_image;
    }

    void HandleRequest(HTTPServer *server, HTTPRequest &&request) {
        std::vector<std::string> path_parts = GetPathParts(request.url_path);

        if (path_parts.empty()) {
            server->SendResponse(request, HTTPResponse::NotFound(request));
            return;
        }

        if (!path_parts.empty()) {
            auto it = m_request_handlers.find(path_parts[0]);
            if (it == m_request_handlers.end()) {
                server->SendResponse(request, HTTPResponse::BadRequest(request, "Unknown request type: %s", path_parts[2].c_str()));
                return;
            }

            (this->*it->second)(server, std::move(request), path_parts, 0);
            return;
        }

        server->SendResponse(request, HTTPResponse::BadRequest(request));
    }

    void SendMessagesResponse(HTTPServer *server, const HTTPRequest &request, const std::shared_ptr<MessageList> &message_list) {
        std::string text;

        message_list->ForEachMessage([&text](MessageList::Message *message) {
            if (!text.empty()) {
                text += "\r\n";
            }
            text += message->text;
        });

        server->SendResponse(request, HTTPResponse::BadRequest(request, "%s", text.c_str()));
    }

    void SendMessage(BeebWindow *beeb_window,
                     HTTPServer *server,
                     const HTTPRequest &request,
                     std::shared_ptr<BeebThread::Message> message) {
        auto completion_fun = [server, response_data = request.response_data](bool success,
                                                                              std::string message) {
            LOGF(OUTPUT, "SendMessage completion_fun: connected ID=%" PRIu64 "\n", response_data.connection_id);

            HTTPResponse response;
            if (success) {
                response = HTTPResponse::OK();
            } else {
                response = HTTPResponse::ServiceUnavailable();
            }

            if (!message.empty()) {
                response.content_type = HTTP_TEXT_CONTENT_TYPE;
                response.content_str = std::move(message);
            }

            server->SendResponse(response_data, response);
        };

        std::shared_ptr<BeebThread> beeb_thread = beeb_window->GetBeebThread();
        beeb_thread->Send(std::move(message), std::move(completion_fun));
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<HTTPHandler> CreateHTTPMethodsHandler() {
    return std::make_shared<HTTPMethodsHandler>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
