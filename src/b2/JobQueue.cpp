#include <shared/system.h>
#include "JobQueue.h"
#include <functional>
#include <system_error>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

JobQueue::Job::Job() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

JobQueue::Job::~Job() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool JobQueue::Job::IsRunning() const {
    return m_running.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool JobQueue::Job::IsFinished() const {
    return m_finished.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void JobQueue::Job::Cancel() {
    m_canceled.store(true, std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool JobQueue::Job::WasCanceled() const {
    return m_canceled.load(std::memory_order_acquire);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool JobQueue::Job::HasImGui() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void JobQueue::Job::DoImGui() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

JobQueue::JobQueue() {
    MUTEX_SET_NAME(m_jobs_mutex, "Jobs");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

JobQueue::~JobQueue() {
    {
        LockGuard<Mutex> lock(m_jobs_mutex);

        for (auto &&thread : m_threads) {
            if (!!thread.job) {
                thread.job->Cancel();
            }
        }

        for (auto &&job : m_jobs) {
            job->Cancel();
        }

        m_quit = true;
    }

    m_jobs_cv.notify_all();

    for (auto &&thread : m_threads) {
        thread.thread.join();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool JobQueue::Init(unsigned num_threads) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
    }

    if (num_threads == 0) {
        num_threads = 1; // got to do something.
    }

    m_threads.resize(num_threads);

    for (unsigned i = 0; i < num_threads; ++i) {
        ThreadData *td = &m_threads[i];

        td->index = i;

        try {
            td->thread = std::thread(std::bind(&JobQueue::ThreadFunc, this, td));
        } catch (const std::system_error &) {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void JobQueue::AddJob(std::shared_ptr<Job> job) {
    {
        LockGuard<Mutex> lock(m_jobs_mutex);

        m_jobs.push_back(std::move(job));
    }

    m_jobs_cv.notify_one();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<JobQueue::Job>> JobQueue::GetJobs() const {
    std::vector<std::shared_ptr<Job>> jobs;

    LockGuard<Mutex> lock(m_jobs_mutex);

    for (const ThreadData &thread : m_threads) {
        std::shared_ptr<Job> job = thread.job;

        if (!!job) {
            jobs.emplace_back(std::move(job));
        }
    }

    if (!m_jobs.empty()) {
        jobs.insert(jobs.end(), m_jobs.begin(), m_jobs.end());
    }

    return jobs;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void JobQueue::ThreadFunc(ThreadData *td) {
    SetCurrentThreadNamef("JobQueue%zu", td->index);

    for (;;) {
        UniqueLock<Mutex> lock(m_jobs_mutex);

        td->job = nullptr;

        if (m_quit) {
            break;
        }

        if (m_jobs.empty()) {
            m_jobs_cv.wait(lock);
        }

        if (!m_jobs.empty()) {
            td->job = m_jobs.front();
            m_jobs.erase(m_jobs.begin());

            // take a copy of td->job, since it will be used with the
            // mutex unlocked.
            std::shared_ptr<Job> job = td->job;

            lock.unlock();

            if (job->WasCanceled()) {
                // nothing to do...
            } else {
                job->m_running.store(true, std::memory_order_release);

                job->ThreadExecute();

                job->m_running.store(false, std::memory_order_release);
            }

            job->m_finished.store(true, std::memory_order_release);
        }
    }
}
