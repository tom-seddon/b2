#ifndef HEADER_A3D79A315D784CF68E5B471420EBF9FA // -*- mode:c++ -*-
#define HEADER_A3D79A315D784CF68E5B471420EBF9FA

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <thread>
#include <shared/mutex.h>
#include <condition_variable>
#include <vector>
#include <atomic>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class JobQueue {
  public:
    class Job {
      public:
        Job();
        virtual ~Job() = 0;

        bool IsRunning() const;
        bool IsFinished() const;

        void Cancel();
        bool WasCanceled() const;

        // Default impl returns false.
        virtual bool HasImGui() const;

        // Do whatever. No need to call base class. Default impl does
        // nothing.
        virtual void DoImGui();

        virtual void ThreadExecute() = 0;

      protected:
        Job(const Job &) = delete;
        Job &operator=(const Job &) = delete;
        Job(Job &&) = delete;
        Job &operator=(Job &&) = delete;

      private:
        mutable std::atomic<bool> m_running{false};
        mutable std::atomic<bool> m_canceled{false};
        mutable std::atomic<bool> m_finished{false};

        friend class JobQueue;
    };

    JobQueue();
    ~JobQueue();

    bool Init(unsigned num_threads = 0);

    void AddJob(std::shared_ptr<Job> job);

    // Get all jobs, running and waiting.
    std::vector<std::shared_ptr<Job>> GetJobs() const;

  protected:
  private:
    struct ThreadData {
        std::thread thread;
        std::shared_ptr<Job> job;
        size_t index;
    };

    std::vector<ThreadData> m_threads;

    mutable Mutex m_jobs_mutex;
    std::condition_variable_any m_jobs_cv;
    std::vector<std::shared_ptr<Job>> m_jobs;
    bool m_quit = false;

    void ThreadFunc(ThreadData *td);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
