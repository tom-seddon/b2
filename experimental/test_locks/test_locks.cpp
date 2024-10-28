#include <shared/system.h>
#include <shared/log.h>
#include <thread>
#include <shared/mutex.h>

LOG_DEFINE(OUTPUT, "", &log_printer_stdout_and_debugger);

static volatile uint64_t g_counter;
static volatile bool g_done;
static Mutex g_mutex;

static void Thread() {
    while (!g_done) {
        //SleepMS(1);
        LockGuard<Mutex> lock(g_mutex);
        ++g_counter;
    }
}

int main() {
    std::thread tmp(Thread);

    uint64_t a = GetCurrentTickCount();

    for (int i = 0; i < 2000000; ++i) {
        LockGuard<Mutex> lock(g_mutex);

        ++g_counter;
    }

    uint64_t b = GetCurrentTickCount();

    g_done = true;
    tmp.join();

    LOGF(OUTPUT, "took: %.3f sec\n", GetSecondsFromTicks(b - a));
}
