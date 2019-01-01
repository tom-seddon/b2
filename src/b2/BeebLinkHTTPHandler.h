#ifndef HEADER_7FB2AFC4A58949F5A9F262E6A5A574A3// -*- mode:c++ -*-
#define HEADER_7FB2AFC4A58949F5A9F262E6A5A574A3

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <beeb/BeebLink.h>
#include <string>
#include <thread>
#include <vector>

class BeebThread;
class Messages;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebLinkHTTPHandler:
public BeebLinkHandler
{
public:
    explicit BeebLinkHTTPHandler(BeebThread *beeb_thread,
                                 std::string sender_id);
    ~BeebLinkHTTPHandler();

    bool Init(Messages *msg);

    bool GotRequestPacket(std::vector<uint8_t> data) override;
protected:
private:
    struct ThreadState;

    std::unique_ptr<ThreadState> m_ts;
    std::string m_sender_id;
    std::thread m_thread;

    static void Thread(ThreadState *ts);

    BeebLinkHTTPHandler(const BeebLinkHTTPHandler &)=delete;
    BeebLinkHTTPHandler &operator=(const BeebLinkHTTPHandler &)=delete;
    BeebLinkHTTPHandler(BeebLinkHTTPHandler &&)=delete;
    BeebLinkHTTPHandler &operator=(BeebLinkHTTPHandler &&)=delete;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
