#include <shared/system.h>
#include "JobQueue.h"
#include <shared/testing.h>
#include <stdio.h>
#include <inttypes.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct  BlockJob:
    JobQueue::Job
{
    void ThreadExecute() override {
        printf("blocker started.\n");
        
        while(!this->WasCanceled()) {
            SleepMS(100);

            //printf("f=%" PRId32 " c=%" PRId32 "\n",m_finished,m_canceled);
        }

        printf("blocker finished.\n");
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TestJob1:
    JobQueue::Job
{
    std::atomic<int32_t> *value=nullptr;
    
    void ThreadExecute() override {
        if(this->value) {
            ++*this->value;
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(void) {
    setbuf(stdout,nullptr);
    
    {
        JobQueue jq;

        TEST_TRUE(jq.Init(1));

        auto &&blocker=std::make_shared<BlockJob>();

        jq.AddJob(blocker);

        std::atomic<int32_t> counter{0};
        
        std::vector<std::shared_ptr<TestJob1>> test_jobs;
        test_jobs.resize(10);
        
        for(size_t i=0;i<test_jobs.size();++i) {
            std::shared_ptr<TestJob1> p=std::make_shared<TestJob1>();
            p->value=&counter;
            
            test_jobs[i]=p;
            jq.AddJob(p);
        }

        TEST_EQ_II(counter,0);

        printf("waiting for blocker to start...\n");
        
        while(!blocker->IsRunning()) {
            SleepMS(1);
        }

        TEST_FALSE(blocker->IsFinished());
        TEST_FALSE(blocker->WasCanceled());

        blocker->Cancel();

        printf("waiting for blocker to finish...\n");
        while(!blocker->IsFinished()) {
            SleepMS(1);
        }
        
        TEST_TRUE(blocker->WasCanceled());

        printf("waiting for test jobs to finish...\n");
        
        for(;;) {
            size_t num_finished=0;
            
            for(auto &&test_job:test_jobs) {
                if(test_job->IsFinished()) {
                    ++num_finished;
                }
            }

            if(num_finished==test_jobs.size()) {
                break;
            }

            SleepMS(1);
        }

        TEST_EQ_II(counter,(int32_t)test_jobs.size());

        printf("all done...\n");
    }
}
