#include <shared/system.h>
#include "HTTPClient.h"
#include <curl/curl.h>
#include <vector>
#include <map>
#include <string>
#include <shared/debug.h>
#include "HTTP.h"
#include "Messages.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPClient::HTTPClient() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HTTPClient::~HTTPClient() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//struct ClientReadPayloadState {
//    const std::vector<uint8_t> *payload = nullptr;
//    size_t index = 0;
//};
//
//static size_t ClientReadPayload(char*buffer,
//    size_t size,
//    size_t nitems,
//    void*userdata){
//    auto state = (ClientReadPayloadState*)userdata;
//
//    size_t i,n=state->payload->size();
//
//    for(i=0;i<size*nitems;++i){
//        if(state->index>=n){
//            break;
//        }
//
//        buffer[i]=
//    }

static size_t WriteServerToClientData(char *ptr,
                                      size_t size,
                                      size_t nmemb,
                                      void *userdata) {
    auto buffer = (std::vector<uint8_t> *)userdata;

    size_t n = size * nmemb;

    buffer->insert(buffer->end(), ptr, ptr + n);

    return n;
}

struct ReadClientToServerState {
    const std::vector<uint8_t> *buffer = nullptr;
    size_t index = 0;
};

static size_t ReadClientToServerData(char *buffer,
                                     size_t size,
                                     size_t nitems,
                                     void *userdata) {
    auto state = (ReadClientToServerState *)userdata;

    size_t n = 0;
    const uint8_t *p = nullptr;
    if (state->buffer) {
        n = state->buffer->size();
        p = state->buffer->data();
    }

    size_t i;
    for (i = 0; i < size * nitems; ++i) {
        if (state->index >= n) {
            break;
        }

        buffer[i] = (char)(p[state->index++]);
    }

    return i;
}

static const char *const CURL_INFOTYPE_PREFIXES[] = {
    nullptr,        //    CURLINFO_TEXT = 0,
    "HEADER_IN",    //    CURLINFO_HEADER_IN,    /* 1 */
    "HEADER_OUT",   //    CURLINFO_HEADER_OUT,   /* 2 */
    "DATA_IN",      //    CURLINFO_DATA_IN,      /* 3 */
    "DATA_OUT",     //    CURLINFO_DATA_OUT,     /* 4 */
    "SSL_DATA_IN",  //    CURLINFO_SSL_DATA_IN,  /* 5 */
    "SSL_DATA_OUT", //    CURLINFO_SSL_DATA_OUT, /* 6 */
};

class HTTPClientImpl : public HTTPClient {
  public:
    HTTPClientImpl() {
    }

    ~HTTPClientImpl() {
    }

    void SetMessages(Messages *messages) override {
        m_messages = messages;
    }

    void SetVerbose(bool verbose) override {
        m_verbose = verbose;
    }

    void AddDefaultHeader(const char *key, const char *value) override {
        std::string key_str = key;
        m_default_header_by_key[key_str] = key_str + ": " + value;
    }

    int SendRequest(const HTTPRequest &request, HTTPResponse *response) override {
        ASSERT(response);

        if (!m_curl) {
            m_curl = curl_easy_init();
            if (!m_curl) {
                if (m_messages) {
                    m_messages->e.f("Failed to initialize libcurl\n");
                }
                return -1;
            }

            // see notes regarding DNS timeouts in
            // https://curl.haxx.se/libcurl/c/CURLOPT_NOSIGNAL.html. Hopefully
            // not too much an issue here, as the connection is almost always to
            // 127.0.0.1
            curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);

            curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);
        }

        // Collate full list of header lines.
        std::map<std::string, std::string> header_by_key = m_default_header_by_key;

        for (auto &&key_and_value : request.headers) {
            header_by_key[key_and_value.first] = key_and_value.first + ": " + key_and_value.second;
        }

        // If there's an explicit Content-Type, pop that in. Also, the charset.
        if (!request.content_type.empty()) {
            std::string content_type_header = CONTENT_TYPE + ": " + request.content_type;
            if (!request.content_type_charset.empty()) {
                content_type_header += "; " + CHARSET_PREFIX + request.content_type_charset;
            }
            header_by_key[CONTENT_TYPE] = content_type_header;
        }

        // Form URL.
        std::string url = request.url;
        ASSERT(request.url_path.empty());

        // Add additional request parameters.
        if (!request.query.empty()) {
            if (url.find('?') != std::string::npos) {
                url.push_back('&');
            } else {
                url.push_back('?');
            }

            for (size_t i = 0; i < request.query.size(); ++i) {
                const HTTPQueryParameter &parameter = request.query[i];

                if (i > 0) {
                    url.push_back('&');
                }

                url += GetPercentEncoded(parameter.key);
                url.push_back('=');
                url += GetPercentEncoded(parameter.value);
            }
        }

        // Form headers.
        curl_slist *headers = nullptr;
        for (auto &&key_and_header : header_by_key) {
            headers = curl_slist_append(headers, key_and_header.second.c_str());
        }

        // Curlstuff.
        curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);

        // CURLOPT_POST is weird: https://curl.se/libcurl/c/CURLOPT_POST.html
        //
        // Set it to 0 and do the method by hand.
        curl_easy_setopt(m_curl, CURLOPT_POST, (long)0);
        curl_easy_setopt(m_curl,
                         CURLOPT_CUSTOMREQUEST,
                         request.method.empty() ? "GET" : request.method.c_str());

        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)request.body.size());

        // Server->client data.
        std::vector<uint8_t> server_to_client_data_buffer;
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &WriteServerToClientData);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &server_to_client_data_buffer);

        // Client->server data.
        ReadClientToServerState client_to_server_state = {};
        if (response) {
            client_to_server_state.buffer = &response->content_vec;
        }
        curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, &ReadClientToServerData);
        curl_easy_setopt(m_curl, CURLOPT_READDATA, &client_to_server_state);

        // Debug output.
        if (m_messages) {
            curl_easy_setopt(m_curl, CURLOPT_VERBOSE, (long)m_verbose);
            curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, &ClientDebugFunction);
            curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, this);
        } else {
            curl_easy_setopt(m_curl, CURLOPT_VERBOSE, (long)0);
            curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, nullptr);
            curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, nullptr);
        }

        // Error buffer.
        char curl_error_buffer[CURL_ERROR_SIZE];
        curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, curl_error_buffer);

        // Do the thing.
        CURLcode perform_result = curl_easy_perform(m_curl);

        curl_slist_free_all(headers);
        headers = nullptr;

        if (m_verbose) {
            if (m_messages) {
                m_messages->i.f("curl_easy_perform returned: %d\n", (int)perform_result);
            }
        }

        int http_status;
        if (perform_result == CURLE_OK) {
            char *content_type;
            curl_easy_getinfo(m_curl, CURLINFO_CONTENT_TYPE, &content_type);

            response->content_type.clear();
            if (content_type) {
                response->content_type = content_type;
            }

            long status;
            curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &status);

            response->status = std::to_string(status);
            http_status = (int)status;

            return http_status;
        } else {
            if (m_messages) {
                m_messages->e.f("Request failed: %s: %s\n",
                                curl_easy_strerror(perform_result),
                                curl_error_buffer);
            }

            return -1;
        }
    }

  protected:
  private:
    CURL *m_curl = nullptr;
    Messages *m_messages = nullptr;
    bool m_verbose = false;

    std::map<std::string, std::string> m_default_header_by_key;

    static void ClientDebugFunction(CURL *handle,
                                    curl_infotype type,
                                    char *data,
                                    size_t size,
                                    void *userptr) {
        (void)handle;

        auto client = (HTTPClientImpl *)userptr;
        Log *log = &client->m_messages->i;

        if (type == CURLINFO_TEXT) {
            // Judging by https://curl.haxx.se/libcurl/c/CURLOPT_DEBUGFUNCTION.html,
            // this seems to be the special case.

            log->f("TEXT: ");

            LogIndenter indent(log);

            for (size_t i = 0; i < size; ++i) {
                log->c(data[i]);
            }
        } else {
            const char *prefix;
            if (type >= 0 && (size_t)type < sizeof CURL_INFOTYPE_PREFIXES / sizeof CURL_INFOTYPE_PREFIXES[0]) {
                prefix = CURL_INFOTYPE_PREFIXES[type];
            } else {
                prefix = "?";
            }

            log->f("%s: ", prefix);

            LogIndenter indent(log);

            LogDumpBytes(log, data, size);
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<HTTPClient> CreateHTTPClient() {
    return std::make_unique<HTTPClientImpl>();
}
