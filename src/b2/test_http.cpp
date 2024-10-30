#include <shared/system.h>
#include <shared/testing.h>
#include <shared/mutex.h>
#include "HTTPServer.h"
#include "Messages.h"
#include <set>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_EXTERN(HTTPSV);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class HTTPTestHandler : public HTTPHandler {
  public:
    bool ThreadHandleRequest(HTTPResponse *response, HTTPServer *server, HTTPRequest &&request) override {
        (void)server;

        LockGuard<Mutex> lock(m_mutex);

        m_requests.push_back(request);

        *response = HTTPResponse::OK();

        return true;
    }

    std::vector<HTTPRequest> GetRequests() const {
        LockGuard<Mutex> lock(m_mutex);

        return m_requests;
    }

  protected:
  private:
    mutable Mutex m_mutex;
    std::vector<HTTPRequest> m_requests;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const int PORT = 0xbbcd;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main() {

    auto message_list = std::make_shared<MessageList>("test_http");
    message_list->SetPrintToStdio(true);

    Messages messages(message_list);

    LOG(HTTPSV).enabled = true;

    {
        std::unique_ptr<HTTPServer> server = CreateHTTPServer();

        TEST_TRUE(server->Start(PORT, &messages));

        auto handler = std::make_shared<HTTPTestHandler>();
        server->SetHandler(handler);

        {
            HTTPRequest request("http://127.0.0.1:" + std::to_string(PORT) + "/test_url");

            std::unique_ptr<HTTPClient> client = CreateHTTPClient();
            client->SetMessages(&messages);
            client->SetVerbose(true);

            HTTPResponse response;
            int status = client->SendRequest(request, &response);
            TEST_EQ_II(status, 200);

            status = client->SendRequest(HTTPRequest("http://127.0.0.1:" + std::to_string(PORT) + "/test_url?key=value"), &response);
        }

        std::vector<HTTPRequest> requests = handler->GetRequests();

        TEST_EQ_UU(requests.size(), 2);

        TEST_EQ_SS(requests[0].url_path, "/test_url");
        TEST_TRUE(requests[0].query.empty());
        TEST_EQ_SS(requests[0].url, "/test_url");

        TEST_EQ_SS(requests[1].url_path, "/test_url");
        TEST_EQ_UU(requests[1].query.size(), 1);
        TEST_EQ_SS(requests[1].query[0].key, "key");
        TEST_EQ_SS(requests[1].query[0].value, "value");
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
