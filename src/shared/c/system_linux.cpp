#include <shared/system.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stdlib.h>
#include <execinfo.h>
#include <uuid/uuid.h>
#include <shared/debug.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <sys/prctl.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* If true, use addr2line to get symbols from backtrace_symbols. (The
 * standard libc code doesn't try very hard, and gives poor
 * results.) */
#define USE_ADDR2LINE 1
#define DEBUG_ADDR2LINE 0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://stackoverflow.com/questions/3596781/detect-if-gdb-is-running
int IsDebuggerAttached() {
    /* Frankly it all seems like far too much bother. */
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://www.opensource.apple.com/source/mail_cmds/mail_cmds-24/mail/strlcpy.c
size_t strlcpy(char *dest, const char *src, size_t size) {
    char *d = dest;
    const char *s = src;
    size_t n = size;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (size != 0)
            *d = '\0'; /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return (size_t)(s - src - 1); /* count does not include NUL */
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_ADDR2LINE
struct Module {
    std::string name;
    uint64_t begin;
    uint64_t end;
};
#endif

#if USE_ADDR2LINE
static bool ReadFD(std::vector<char> *output, int fd) {
    char buffer[4096];

    output->clear();

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof buffer);

        if (n < 0) {
            return false;
        } else if (n == 0) {
            break;
        }

        output->insert(output->end(), buffer, buffer + n);
    }

    output->push_back(0);
    return true;
}
#endif

#if USE_ADDR2LINE
static void GetBacktraceSymbols(char **symbols,
                                void *const *addresses,
                                int num_addresses,
                                const Module &module) {
    std::vector<size_t> indexes;
    int fds[2] = {-1, -1};
    std::vector<char> output;

    ASSERT(num_addresses >= 0);
    indexes.reserve((size_t)num_addresses);

    for (int i = 0; i < num_addresses; ++i) {
        if ((uint64_t)(uintptr_t)addresses[i] >= module.begin &&
            (uint64_t)(uintptr_t)addresses[i] < module.end) {
            indexes.push_back((size_t)i);
        }
    }

    if (indexes.empty()) {
        return;
    }

    std::vector<char *> argv;
    argv.reserve(5 + indexes.size() + 1);

    argv.push_back((char *)"/usr/bin/addr2line");
    argv.push_back((char *)"-e");
    argv.push_back((char *)module.name.c_str());
    argv.push_back((char *)"-fi");

    size_t first_allocated = argv.size();

    for (size_t index : indexes) {
        // This appears to be how to get good backtraces for ASLR/PIE
        // on Linux. See, e.g.,
        // https://github.com/scylladb/seastar/issues/334
        uint64_t offset = (uint64_t)(uintptr_t)addresses[index] - module.begin;
        char *tmp;
        if (asprintf(&tmp, "%" PRIx64, offset) == -1) {
            goto done;
        }

        argv.push_back(tmp);
    }

    /* for(int i=0;i<num_addresses;++i) { */
    /*     printf("%d. %p\n",i,addresses[i]); */
    /* } */

#if DEBUG_ADDR2LINE
    for (const char *arg : argv) {
        printf("%s ", arg);
    }
    printf("\n");
#endif

    argv.push_back(nullptr);

    if (pipe(fds) == -1) {
        goto done;
    }

    {
        int pid = fork();
        if (pid == -1) {
            goto done;
        }

        if (pid == 0) {
            dup2(fds[1], STDOUT_FILENO);
            dup2(fds[1], STDERR_FILENO);

            execv(argv[0], &argv[0]);
            _exit(127);
        } else {
            close(fds[1]);
            fds[1] = -1;

            ReadFD(&output, fds[0]);

            int status;
            if (waitpid(pid, &status, 0) != pid) {
                goto done;
            }

            if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
                goto done;
            }

            {
                size_t i = 0;
                char *s = output.data();

                while (i < indexes.size()) {
                    char *loc = strsep(&s, "\n"), *fun = NULL;
                    if (loc != NULL) {
                        fun = strsep(&s, "\n");
                    }

                    if (!loc || !fun) {
                        break;
                    }

                    if (fun[0] != '?' && loc[0] != '?') {
                        char **p = &symbols[indexes[i]];
                        if (!*p) {
                            if (asprintf(p, "%p: %s: %s", addresses[indexes[i]], fun, loc) == -1) {
                                goto done;
                            }
                        }
                    }

                    ++i;
                }
            }

            /* /\* printf("--------------------------------------------------\n"); *\/ */
            /* /\* printf("%s\n",output); *\/ */
            /* /\* printf("--------------------------------------------------\n"); *\/ */

            /* OuLogDumpBytes(&tmp,output.data(),output.size()); */
        }
    }

done:;
    for (size_t i = first_allocated; i < argv.size(); ++i) {
        free(argv[i]);
    }
    argv.clear();

    for (int i = 0; i < 2; ++i) {
        if (fds[i] != -1) {
            close(fds[i]);
            fds[i] = -1;
        }
    }
}
#endif

char **GetBacktraceSymbols(void *const *addresses, int num_addresses) {
#if USE_ADDR2LINE

    if (num_addresses <= 0) {
        /* somebody else's problem... */
        return backtrace_symbols(addresses, num_addresses);
    }

    char *result = NULL;

    std::vector<char *> symbols;
    symbols.resize((size_t)num_addresses);

    // contents of /proc/PID/maps
    std::vector<char> maps;

    // modules list
    std::vector<Module> modules;

    /* I don't know how you're *supposed* to read the /proc files.
     * There's a pretty obvious race condition here. */
    {
        std::string fname = "/proc/" + std::to_string(getpid()) + "/maps";

        int fd = open(fname.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd == -1) {
            goto done;
        }

        bool good = ReadFD(&maps, fd);

        close(fd);
        fd = -1;

        if (!good) {
            goto done;
        }
    }

#if DEBUG_ADDR2LINE
    printf("----------------------------------------------------------------------\n");
    // fingers crossed no overflow.
    printf("%.*s\n", (int)maps.size(), maps.data());
    printf("----------------------------------------------------------------------\n");
#endif

    {
        char *stringp = maps.data();
        for (;;) {
            char *line = strsep(&stringp, "\n");
            if (!line) {
                break;
            }

            size_t line_len = strlen(line);

            std::vector<char> prot, name;
            prot.resize(line_len + 1);
            name.resize(line_len + 1);

            Module module;
            uint64_t pgoff;

            sscanf(line, "%" PRIx64 "-%" PRIx64 " %s %" PRIx64 " %*x:%*x %*u %s\n",
                   &module.begin, &module.end, prot.data(), &pgoff, name.data());

            if (strlen(name.data()) == 0) {
                continue;
            }

            if (name[0] == '[') {
                continue;
            }

            module.name = name.data(); //repeated copies... sad!

            //module.end=module.begin+len;

            modules.push_back(module);

#if DEBUG_ADDR2LINE
            printf("%s:\n", module.name.c_str());
            printf("    From: 0x%" PRIx64 "\n", module.begin);
            printf("    To: 0x%" PRIx64 " (+%" PRIu64 ")\n", module.end, module.end - module.begin);
#endif
        }
    }

    for (const Module &module : modules) {
        GetBacktraceSymbols(symbols.data(), addresses, num_addresses, module);
    }

    /* If any entries are missing, fill them in with the address. */
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (!symbols[i]) {
            if (asprintf(&symbols[i], "%p", addresses[i]) == -1) {
                symbols[i] = nullptr;
            }
        }
    }

    /* Build output buffer. */
    {
        size_t num_array_bytes = symbols.size() * sizeof(char *);
        size_t num_string_bytes = 0;
        for (const char *symbol : symbols) {
            num_string_bytes += strlen(symbol) + 1;
        }

        result = (char *)malloc(num_array_bytes + num_string_bytes);
        if (result) {
            char **strings = (char **)result;
            char *buffer = result + num_array_bytes, *dest = buffer;

            for (size_t i = 0; i < symbols.size(); ++i) {
                strings[i] = dest;
                strcpy(dest, symbols[i]);
                dest += strlen(dest) + 1;
            }
            ASSERT(dest == result + num_array_bytes + num_string_bytes);
        }
    }

done:
    for (char *symbol : symbols) {
        free(symbol);
    }
    symbols.clear();

    return (char **)result;

#else

    return backtrace_symbols(addresses, num_addresses);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetLowestSetBitIndex32(uint32_t value) {
    return __builtin_ffs((int)value) - 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetHighestSetBitIndex32(uint32_t value) {
    if (value == 0) {
        return -1;
    }

    return 31 - __builtin_clz(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetLowestSetBitIndex64(uint64_t value) {
    return __builtin_ffsll((int64_t)value) - 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetHighestSetBitIndex64(uint64_t value) {
    if (value == 0) {
        return -1;
    }

    return 63 - __builtin_clzll(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits32(uint32_t value) {
    return (size_t)__builtin_popcount(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits64(uint64_t value) {
    return (size_t)__builtin_popcountll(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetCurrentThreadNameInternal(const char *name) {
    char tmp[16];
    strlcpy(tmp, name, 16);
    prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr int64_t NS_PER_SEC = 1000 * 1000 * 1000;
static constexpr double SECS_PER_NS = 1.0 / NS_PER_SEC;

uint64_t GetCurrentTickCount(void) {
    struct timespec r;
    clock_gettime(CLOCK_MONOTONIC_RAW, &r);
    return (uint64_t)(NS_PER_SEC * r.tv_sec + r.tv_nsec);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsFromTicks(uint64_t ticks) {
    return ticks * SECS_PER_NS;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsPerTick(void) {
    return SECS_PER_NS;
}
