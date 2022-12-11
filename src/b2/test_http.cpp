#include <shared/system.h>
#include <shared/testing.h>
#include <shared/mutex.h>
#include "HTTPServer.h"
#include "Messages.h"
#include <set>

class HTTPTestHandler : public HTTPHandler {
  public:
    bool ThreadHandleRequest(HTTPResponse *response, HTTPServer *server, HTTPRequest &&request) override {
        (void)server;

        std::lock_guard<Mutex> lock(m_mutex);

        m_urls_requested.insert(request.url);

        *response = HTTPResponse::OK();

        return true;
    }

    std::set<std::string> GetURLsRequested() const {
        std::lock_guard<Mutex> lock(m_mutex);

        return m_urls_requested;
    }

  protected:
  private:
    mutable Mutex m_mutex;
    std::set<std::string> m_urls_requested;
};

static const int PORT = 0xbbcd;

static void WaitForServerListening(HTTPServer *server) {
    for (int i = 0; i < 5000; ++i) {
        if (server->IsServerListening()) {
            return;
        }
        SleepMS(1);
    }

    TEST_FAIL("server took too long to start listening");
}

int main() {

    auto message_list = std::make_shared<MessageList>();
    message_list->SetPrintToStdio(true);

    Messages messages(message_list);

    {
        std::unique_ptr<HTTPServer> server = CreateHTTPServer();

        TEST_TRUE(server->Start(PORT, false, &messages));

        auto handler = std::make_shared<HTTPTestHandler>();
        server->SetHandler(handler);

        WaitForServerListening(server.get());

        {
            HTTPRequest request("http://127.0.0.1:" + std::to_string(PORT) + "/test_url");

            std::unique_ptr<HTTPClient> client = CreateHTTPClient();
            client->SetMessages(&messages);
            client->SetVerbose(true);

            HTTPResponse response;
            int status = client->SendRequest(request, &response);
            TEST_EQ_II(status, 200);

            std::set<std::string> urls_requested = handler->GetURLsRequested();
            TEST_EQ_UU(urls_requested.size(), 1);
        }
    }

    return 0;
}