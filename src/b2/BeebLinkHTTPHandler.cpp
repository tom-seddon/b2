#include <shared/system.h>
#include <shared/debug.h>
#include <shared/mutex.h>
#include <shared/log.h>
#include "BeebLinkHTTPHandler.h"
#include <curl/curl.h>
#include "Messages.h"
#include "misc.h"
#include "BeebThread.h"
#include "BeebWindows.h"
#include <shared/mutex.h>
#include "Messages.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(BEEBLINK);

// This is a dummy log. It's never used for printing, only for
// initialising each HTTP thread's log. Since it's a global, it gets an
// entry in the global table, so it can interact with the command line
// options.
LOG_TAGGED_DEFINE(BEEBLINK_HTTP,"beeblink_http","",&log_printer_stdout_and_debugger,false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string BeebLinkHTTPHandler::DEFAULT_URL="http://127.0.0.1:48875/request";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct BeebLinkHTTPHandler::ThreadState {
    // Shared stuff

    Mutex mutex;
    ConditionVariable cv;

    // set if thread should stop after being woken up.
    bool stop;

    // set if thread should send data after being woken up.
    bool busy;

    std::vector<uint8_t> beeb_to_server_data;

    // Used by the thread once it's running.

    std::string sender_id;

    CURL *curl=nullptr;

    std::unique_ptr<Log> log;

    BeebThread *beeb_thread=nullptr;

    std::shared_ptr<MessageList> message_list;

    // Internal thread stuff.

    // set to the URL to use, once one has been found.
    std::string url;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static Mutex g_mutex;
static bool g_http_verbose;
static std::vector<std::string> g_server_urls;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLinkHTTPHandler::GetHTTPVerbose() {
    std::lock_guard<Mutex> lock(g_mutex);

    return g_http_verbose;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLinkHTTPHandler::SetHTTPVerbose(bool verbose) {
    std::lock_guard<Mutex> lock(g_mutex);

    g_http_verbose=verbose;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLinkHTTPHandler::SetServerURLs(std::vector<std::string> urls) {
    std::lock_guard<Mutex> lock(g_mutex);

    g_server_urls=std::move(urls);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::string> BeebLinkHTTPHandler::GetServerURLs() {
    std::lock_guard<Mutex> lock(g_mutex);

    return g_server_urls;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLinkHTTPHandler::BeebLinkHTTPHandler(BeebThread *beeb_thread,
                                         std::string sender_id,
                                         std::shared_ptr<MessageList> message_list):
    m_ts(std::make_unique<ThreadState>()),
    m_sender_id(std::move(sender_id))
{
    MUTEX_SET_NAME(m_ts->mutex,"BeebLinkHTTPHandler "+sender_id);
    m_ts->beeb_thread=beeb_thread;
    m_ts->message_list=std::move(message_list);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BeebLinkHTTPHandler::~BeebLinkHTTPHandler() {
    {
        std::lock_guard<Mutex> lock(m_ts->mutex);

        m_ts->stop=true;
        m_ts->cv.notify_one();
    }

    m_thread.join();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char *const CURL_INFOTYPE_PREFIXES[]={
    nullptr,//    CURLINFO_TEXT = 0,
    "HEADER_IN",//    CURLINFO_HEADER_IN,    /* 1 */
    "HEADER_OUT",//    CURLINFO_HEADER_OUT,   /* 2 */
    "DATA_IN",//    CURLINFO_DATA_IN,      /* 3 */
    "DATA_OUT",//    CURLINFO_DATA_OUT,     /* 4 */
    "SSL_DATA_IN",//    CURLINFO_SSL_DATA_IN,  /* 5 */
    "SSL_DATA_OUT",//    CURLINFO_SSL_DATA_OUT, /* 6 */
};

static void CurlDebugFunction(CURL *handle,
                              curl_infotype type,
                              char *data,
                              size_t size,
                              void *userptr)
{
    (void)handle;

    auto log=(Log *)userptr;

    if(type==CURLINFO_TEXT) {
        // Judging by https://curl.haxx.se/libcurl/c/CURLOPT_DEBUGFUNCTION.html,
        // this seems to be the special case.

        log->f("TEXT: ");

        LogIndenter indent(log);

        for(size_t i=0;i<size;++i) {
            log->c(data[i]);
        }
    } else {
        const char *prefix;
        if(type>=0&&(size_t)type<sizeof CURL_INFOTYPE_PREFIXES/sizeof CURL_INFOTYPE_PREFIXES[0]) {
            prefix=CURL_INFOTYPE_PREFIXES[type];
        } else {
            prefix="?";
        }

        log->f("%s: ",prefix);

        LogIndenter indent(log);

        LogDumpBytes(log,data,size);
    }
}

bool BeebLinkHTTPHandler::Init(Messages *msg) {
    m_ts->curl=curl_easy_init();
    if(!m_ts->curl) {
        msg->e.f("BeebLink - libcurl initialisation failed.\n");
        return false;
    }

    m_ts->sender_id=m_sender_id;

    if(LOG(BEEBLINK).enabled) {
        std::string prefix=std::string(LOG(BEEBLINK).GetPrefix())+": HTTP "+m_sender_id;
        m_ts->log=std::make_unique<Log>(prefix.c_str(),LOG(BEEBLINK).GetLogPrinter(),true);

        curl_easy_setopt(m_ts->curl,CURLOPT_DEBUGFUNCTION,&CurlDebugFunction);
        curl_easy_setopt(m_ts->curl,CURLOPT_DEBUGDATA,m_ts->log.get());
    }

    // see notes regarding DNS timeouts in https://curl.haxx.se/libcurl/c/CURLOPT_NOSIGNAL.html
    // - hopefully not too much an issue here, as the connection is always to
    // 127.0.0.1 unless otherwise specified.
    curl_easy_setopt(m_ts->curl,CURLOPT_NOSIGNAL,1L);

    curl_easy_setopt(m_ts->curl,CURLOPT_FAILONERROR,1L);

    try {
        m_thread=std::thread([this]() {
            Thread(m_ts.get());
        });
    } catch(const std::exception &e) {
        msg->e.f("Failed to create BeebLink HTTP thread: %s\n",e.what());
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BeebLinkHTTPHandler::GotRequestPacket(std::vector<uint8_t> data) {
    ASSERT(!data.empty());

    {
        std::lock_guard<Mutex> lock(m_ts->mutex);

        m_ts->busy=true;
        m_ts->beeb_to_server_data=std::move(data);
    }

    m_ts->cv.notify_one();

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CurlReadPayloadFunctionState {
    const std::vector<uint8_t> *payload=nullptr;
    size_t index=0;
};

static size_t CurlReadPayloadFunction(char *buffer,
                                      size_t size,
                                      size_t nitems,
                                      void *userdata)
{
    auto state=(CurlReadPayloadFunctionState *)userdata;

    size_t i;

    for(i=0;i<size*nitems;++i) {
        if(state->index>=state->payload->size()) {
            break;
        }

        buffer[i]=(char)(*state->payload)[state->index++];
    }

    return i;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static size_t CurlWriteCallback(char *ptr,
                                size_t size,
                                size_t nmemb,
                                void *userdata)
{
    auto payload=(std::vector<uint8_t> *)userdata;

    size_t n=size*nmemb;

    payload->insert(payload->end(),ptr,ptr+n);

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BeebLinkHTTPHandler::Thread(ThreadState *ts) {
    Messages msg(ts->message_list);

    SetCurrentThreadNamef("%s BLHTTP",ts->sender_id.c_str());

    char curl_error_buffer[CURL_ERROR_SIZE];
    curl_easy_setopt(ts->curl,CURLOPT_ERRORBUFFER,curl_error_buffer);

    const std::string sender_id_header="BeebLink-Sender-Id: "+ts->sender_id;

    curl_slist *headers=nullptr;
    headers=curl_slist_append(headers,"Content-Type: application-binary");
    headers=curl_slist_append(headers,sender_id_header.c_str());

    bool show_errors=true;

    for(;;) {
        std::unique_lock<Mutex> lock(ts->mutex);

        if(ts->stop) {
            break;
        }

        if(!ts->busy) {
            ts->cv.wait(lock);
        }

        if(ts->busy) {
            bool done=false;
            std::vector<uint8_t> server_to_beeb_data;

            std::vector<std::string> urls;
            std::vector<CURLcode> results;

            if(ts->url.empty()) {
                urls=GetServerURLs();
                urls.insert(urls.begin(),DEFAULT_URL);

            } else {
                urls.push_back(ts->url);
            }

            results.resize(urls.size(),CURLE_OK);

            size_t url_index=0;

            ASSERT(!ts->beeb_to_server_data.empty());
            std::vector<uint8_t> beeb_to_server_data=std::move(ts->beeb_to_server_data);

            do {
                curl_easy_setopt(ts->curl,CURLOPT_URL,urls[url_index].c_str());

                curl_easy_setopt(ts->curl,CURLOPT_POST,1L);
                curl_easy_setopt(ts->curl,CURLOPT_CUSTOMREQUEST,"POST");

                CurlReadPayloadFunctionState readdata;
                readdata.payload=&beeb_to_server_data;

                curl_easy_setopt(ts->curl,CURLOPT_READFUNCTION,&CurlReadPayloadFunction);
                curl_easy_setopt(ts->curl,CURLOPT_READDATA,&readdata);

                ASSERT(!beeb_to_server_data.empty());
                auto postfieldsize_large=(curl_off_t)beeb_to_server_data.size();
                //printf("xxx %zu %" PRIu64 " %" PRIu64 "\n",sizeof(curl_off_t),(uint64_t)beeb_to_server_data.size(),(uint64_t)postfieldsize_large);
                curl_easy_setopt(ts->curl,CURLOPT_POSTFIELDSIZE_LARGE,postfieldsize_large);

                curl_easy_setopt(ts->curl,CURLOPT_WRITEFUNCTION,&CurlWriteCallback);
                curl_easy_setopt(ts->curl,CURLOPT_WRITEDATA,&server_to_beeb_data);

                curl_easy_setopt(ts->curl,CURLOPT_VERBOSE,(long)GetHTTPVerbose());

                curl_easy_setopt(ts->curl,CURLOPT_HTTPHEADER,headers);

                if(ts->log) {
                    ts->log->f("curl_easy_perform...\n");
                }

                CURLcode perform_result=curl_easy_perform(ts->curl);

                if(ts->log) {
                    ts->log->f("curl_easy_perform returned: %d\n",(int)perform_result);
                }

                if(perform_result==CURLE_OK) {
                    long status;
                    curl_easy_getinfo(ts->curl,CURLINFO_RESPONSE_CODE,&status);

                    if(ts->url.empty()) {
                        ts->url=urls[url_index];
                    }

                    done=true;

                    // Now it's working, show any errors that appear
                    // in future.
                    show_errors=true;
                } else {
                    if(ts->log) {
                        ts->log->f("Request failed: %s: %s\n",
                                   curl_easy_strerror(perform_result),
                                   curl_error_buffer);
                    }

                    results[url_index]=perform_result;

                    // Try next URL in the list...
                    ++url_index;
                    ASSERT(url_index<=urls.size());
                    if(url_index==urls.size()) {
                        server_to_beeb_data=BeebLink::GetErrorResponsePacketData(255,
                                                                                 "HTTP request failed");

                        if(show_errors) {
                            for(size_t i=0;i<urls.size();++i) {
                                msg.e.f("BeebLink - failed to connect to %s\n",urls[i].c_str());
                                msg.i.f("(error: %s)\n",curl_easy_strerror(results[i]));
                            }
                        }

                        show_errors=false;//don't spam!
                        done=true;
                    }
                }
            } while(!done);

            ASSERT(!server_to_beeb_data.empty());
            ASSERT(ts->beeb_thread);
            ts->beeb_thread->Send(std::make_shared<BeebThread::BeebLinkResponseMessage>(std::move(server_to_beeb_data)));

            ts->busy=false;
        }
    }

    curl_easy_cleanup(ts->curl);
    ts->curl=nullptr;

    curl_slist_free_all(headers);
    headers=nullptr;
}
