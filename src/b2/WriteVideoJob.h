#ifndef HEADER_8640141AD64F4187836B07DCC2A93CD0// -*- mode:c++ -*-
#define HEADER_8640141AD64F4187836B07DCC2A93CD0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "VideoWriter.h"
#include "Messages.h"
#include "JobQueue.h"
#include <memory>
#include <SDL.h>
#include "Timeline.h"
#include <atomic>
#include "BeebThread.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class WriteVideoJob:
    public JobQueue::Job
{
public:
    WriteVideoJob(std::vector<BeebThread::TimelineEvent> events,
                  std::unique_ptr<VideoWriter> writer,
                  std::shared_ptr<MessageList> message_list);
    ~WriteVideoJob();

    bool WasSuccessful() const;

    bool HasImGui() const override;
    void DoImGui() override;

    void ThreadExecute() override;
protected:
private:
    std::shared_ptr<MessageList> m_message_list;
    std::vector<BeebThread::TimelineEvent> m_events;
    std::unique_ptr<VideoWriter> m_writer;
    bool m_success=false;
    std::atomic<uint64_t> m_ticks{0};
    std::atomic<uint64_t> m_cycles_done{0};
    std::atomic<uint64_t> m_cycles_total{0};
    std::string m_file_name;

    Messages m_msg;

    bool Error(const char *fmt,...);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
