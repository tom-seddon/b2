#include <shared/system.h>
#include <shared/testing.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

/* There's no brilliant way of testing this, so this just does it a
 * slightly crappy way: thread tries to wait for THREAD_SLEEP_TIME;
 * main thread waits for MAIN_THREAD_SLEEP_TIME then sends the thread
 * a signal; main thread waits for thread and checks the total wait
 * time (as reported by GetCurrentTimeMS) was more like
 * THREAD_SLEEP_TIME then MAIN_THREAD_SLEEP_TIME.
 *
 * This is potentially flaky, but it seems to work(TM) on Linux and OS
 * X, and fairly reliably, even with these comparatively short time
 * periods.
 *
 * (Luckily, none of this is an issue on Windows.)
 */
static const unsigned THREAD_SLEEP_TIME = 50;
static const unsigned MAIN_THREAD_SLEEP_TIME = 5;

static volatile int32_t g_handled_signal;
static int g_pipe[2];
static volatile uint64_t g_wait_ticks;

static void HandleSignal(int sig) {
    (void)sig;
    g_handled_signal = 1;
}

static void *Thread(void *arg) {
    (void)arg;

    uint8_t tmp;
    TEST_EQ_II(read(g_pipe[0], &tmp, 1), 1);

    uint64_t a = GetCurrentTickCount();

    SleepMS(THREAD_SLEEP_TIME);

    g_wait_ticks = GetCurrentTickCount() - a;

    return NULL;
}

int main(void) {
    TEST_EQ_II(pipe(g_pipe), 0);

    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = &HandleSignal;

    TEST_EQ_II(sigaction(SIGUSR1, &sa, NULL), 0);

    pthread_t thread;
    TEST_EQ_II(pthread_create(&thread, NULL, &Thread, NULL), 0);

    uint8_t tmp = 55;
    TEST_EQ_II(write(g_pipe[1], &tmp, 1), 1);

    /* N.B. this is super lame. */
    SleepMS(MAIN_THREAD_SLEEP_TIME);

    TEST_EQ_II(pthread_kill(thread, SIGUSR1), 0);

    TEST_EQ_II(pthread_join(thread, NULL), 0);

    TEST_TRUE(GetSecondsFromTicks(g_wait_ticks) * 1000. >= THREAD_SLEEP_TIME);
}
