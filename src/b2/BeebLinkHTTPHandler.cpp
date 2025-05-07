#include <shared/system.h>
#include <shared/debug.h>
#include <shared/mutex.h>
#include <shared/log.h>
#include "BeebLinkHTTPHandler.h"
#include "Messages.h"
#include "misc.h"
#include "BeebThread.h"
#include "BeebWindows.h"
#include <shared/mutex.h>
#include <condition_variable>
#include "Messages.h"
#include <http/http.h>
#include <http/HTTPClient.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(BEEBLINK);

// This is a dummy log. It's never used for printing, only for
// initialising each HTTP thread's log. Since it's a global, it gets an
// entry in the global table, so it can interact with the command line
// options.
LOG_TAGGED_DEFINE(BEEBLINK_HTTP, "beeblink_http", "", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string BeebLinkHTTPHandler::DEFAULT_URL = "http://127.0.0.1:48875/request";

static const std::string BEEBLINK_SENDER_ID = "BeebLink-Sender-Id";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Request {
    std::vector<uint8_t> data;
    bool is_fire_and_forget = false;

    Request() = default;
    Request(const Request &) = delete;
    Request &operator=(const Request &) = delete;
    Request(Request &&) = default;
    Request &operator=(Request &&) = default;
};

struct BeebLinkHTTPHandler::ThreadState {
    // Shared stuff

    Mutex mutex;
    std::condition_variable_any cv;

    bool stop = false; // set if thread should stop after being woken up.
    std::vector<Request> request_queue;
    bool reset_url = false; //set if thread should reset the URL to use

    // Used by the thread once it's running.

    std::string sender_id;

    std::unique_ptr<Log> log;

    BeebThread *beeb_thread = nullptr;

    std::shared_ptr<MessageList> message_list;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static Mutex g_mutex;
static MutexNameSetter g_mutex_name_setter(&g_mutex, "BeebLink URLs");
static std::vector<std::string> g_server_urls;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLinkHTTPHandler::SetServerURLs(std::vector<std::string> urls) {
    LockGuard<Mutex> lock(g_mutex);

    g_server_urls = std::move(urls);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::string> BeebLinkHTTPHandler::GetServerURLs() {
    LockGuard<Mutex> lock(g_mutex);

    return g_server_urls;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLinkHTTPHandler::BeebLinkHTTPHandler(BeebThread *beeb_thread,
                                         std::string sender_id,
                                         std::shared_ptr<MessageList> message_list)
    : m_ts(std::make_unique<ThreadState>())
    , m_sender_id(std::move(sender_id)) {
    MUTEX_SET_NAME(m_ts->mutex, ("BeebLinkHTTPHandler " + sender_id));
    m_ts->beeb_thread = beeb_thread;
    m_ts->message_list = std::move(message_list);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLinkHTTPHandler::~BeebLinkHTTPHandler() {
    {
        LockGuard<Mutex> lock(m_ts->mutex);

        m_ts->stop = true;
        m_ts->cv.notify_one();
    }

    m_thread.join();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLinkHTTPHandler::Init(Messages *msg) {
    m_ts->sender_id = m_sender_id;

    std::string prefix = std::string(LOG(BEEBLINK_HTTP).GetPrefix());
    if (!prefix.empty()) {
        prefix += ": ";
    }
    prefix += "BLHTTP " + m_sender_id;

    m_ts->log = std::make_unique<Log>(prefix.c_str(), LOG(BEEBLINK_HTTP).GetLogPrinter(), LOG(BEEBLINK_HTTP).enabled);

    try {
        m_thread = std::thread([this]() {
            Thread(m_ts.get());
        });
    } catch (const std::exception &e) {
        msg->e.f("Failed to create BeebLink HTTP thread: %s\n", e.what());
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLinkHTTPHandler::Reset() {
    LockGuard<Mutex> lock(m_ts->mutex);

    m_ts->reset_url = true;

    // No need to notify condition variable watchers. It'll take place on the
    // next operation.
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLinkHTTPHandler::GotRequestPacket(std::vector<uint8_t> data, bool is_fire_and_forget) {
    ASSERT(!data.empty());

    {
        LockGuard<Mutex> lock(m_ts->mutex);

        m_ts->request_queue.push_back({std::move(data), is_fire_and_forget});
    }

    m_ts->cv.notify_one();

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool SendHTTPRequest(std::string *result_str,
                            int *status,
                            std::vector<uint8_t> *server_to_beeb_data,
                            HTTPClient *client,
                            const std::string &url,
                            const std::string &sender_id,
                            Request &&beeblink_request) {
    HTTPRequest http_request;
    http_request.headers[BEEBLINK_SENDER_ID] = sender_id;
    http_request.content_type = HTTP_OCTET_STREAM_CONTENT_TYPE;

    http_request.method = "POST";
    http_request.url = url;
    http_request.body = std::move(beeblink_request.data);

    HTTPResponse http_response;
    *status = client->SendRequest(http_request, &http_response);
    if (*status == 200) {
        *server_to_beeb_data = std::move(http_response.content);
        return true;
    } else {
        *result_str = std::move(http_response.status);
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLinkHTTPHandler::Thread(ThreadState *ts) {
    Messages ui_logs(ts->message_list);

    SetCurrentThreadNamef("%s BLHTTP", ts->sender_id.c_str());

    std::unique_ptr<HTTPClient> client = CreateHTTPClient();

    // This is a bit dumb. I got the logging arrangement slightly wrong.
    //
    // The client checks each Log's enabled flag before printing anything
    // voluminous, so there's not much meaningful overhead.
    LogSet ts_logs{*ts->log, *ts->log, *ts->log};
    client->SetLogs(&ts_logs);

    bool show_errors = true;
    std::string url_found;

    for (;;) {
        UniqueLock<Mutex> lock(ts->mutex);

        if (ts->stop) {
            break;
        }

        if (ts->request_queue.empty()) {
            ts->cv.wait(lock);
        }

        if (ts->reset_url) {
            url_found.clear();

            ts->reset_url = false;
            show_errors = true;
        }

        if (!ts->request_queue.empty()) {
            std::vector<Request> request_queue = std::move(ts->request_queue);
            ts->request_queue.clear();
            lock.unlock();

            std::vector<uint8_t> server_to_beeb_data;

            for (Request &request : request_queue) {
                std::vector<std::string> urls;

                if (url_found.empty()) {
                    urls = GetServerURLs();
                    urls.insert(urls.begin(), DEFAULT_URL);

                    // Form the V2 URL for each one.
                    for (std::string &url : urls) {
                        url.append("/2");
                    }
                } else {
                    urls.push_back(url_found);
                }

                std::vector<std::string> result_strs;
                result_strs.resize(urls.size());

                size_t url_index = 0;

                bool done = false;
                do {
                    int status;
                    bool good = SendHTTPRequest(&result_strs[url_index],
                                                &status,
                                                &server_to_beeb_data,
                                                client.get(),
                                                urls[url_index],
                                                ts->sender_id,
                                                std::move(request));

                    if (good && status == 200) {
                        if (url_found.empty()) {
                            url_found = urls[url_index];
                        }

                        done = true;

                        // Now it's working, show any errors that appear in future.
                        show_errors = true;
                    } else {
                        // Try next URL in the list...
                        ++url_index;
                        ASSERT(url_index <= urls.size());
                        if (url_index == urls.size()) {
                            server_to_beeb_data = GetBeebLinkErrorResponsePacketData(255,
                                                                                     "HTTP request failed");

                            if (show_errors) {
                                for (size_t i = 0; i < urls.size(); ++i) {
                                    ui_logs.e.f("BeebLink - failed to connect to %s\n", urls[i].c_str());
                                    ui_logs.i.f("(error: %s)\n", result_strs[i].c_str());
                                }
                            }

                            done = true;
                        }
                    }

                } while (!done);

                ASSERT(!server_to_beeb_data.empty());
                ASSERT(ts->beeb_thread);
                if (!request.is_fire_and_forget) {
                    ts->beeb_thread->Send(std::make_shared<BeebThread::BeebLinkResponseMessage>(std::move(server_to_beeb_data)));
                }
            }
        }
    }
}
