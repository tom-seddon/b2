#include <shared/system.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://stackoverflow.com/questions/1022957/getting-terminal-width-in-c
int GetTerminalWidth(void) {
    if (isatty(STDOUT_FILENO)) {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

        return w.ws_col;
    } else {
        return INT_MAX;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SleepMS(unsigned ms) {
    struct timespec tp;

    tp.tv_nsec = ((int64_t)ms % 1000) * 1000000;
    tp.tv_sec = (int)(ms / 1000);

    for (;;) {
        int rc = nanosleep(&tp, &tp);
        if (rc == 0) {
            break;
        } else if (rc == -1) {
            if (errno == EINTR) {
                /* Keep waiting. */
            } else {
                /* Better suggestions welcome... */
                break;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
