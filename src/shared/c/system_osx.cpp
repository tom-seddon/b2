#include <shared/system.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <stdlib.h>
#include <execinfo.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/clock.h>
#include <mach-o/dyld.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <vector>
#include <string>
#include <mach/mach_time.h>
#include <sys/proc_info.h>
//#define THREAD_NAME_SIZE (MAXTHREADNAMESIZE)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* If true, use `atos' to get higher-quality backtrace symbols. */
#define USE_ATOS 1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static volatile uint32_t g_got_timebase_metrics;
static double g_timebase_secs_per_tick;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://stackoverflow.com/questions/417745/os-x-equivalent-to-outputdebugstring
int IsDebuggerAttached() {
    struct kinfo_proc info;
    info.kp_proc.p_flag = 0;

    int mib[4] = {
        CTL_KERN,
        KERN_PROC,
        KERN_PROC_PID,
        getpid(),
    };

    size_t n = sizeof(info);
    sysctl(mib, sizeof mib / sizeof mib[0], &info, &n, NULL, 0);

    if (info.kp_proc.p_flag & P_TRACED)
        return 1;
    else
        return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if USE_ATOS
struct Image {
    uint32_t dyld_index;
    std::string name;
    uint64_t begin;
    uint64_t end;
};
#endif

// #if USE_ATOS
// static int CompareImagesByBegin(const void *a,const void *b) {
//     const Image *ia=a,*ib=b;

//     if(ia->begin<ib->begin) {
//         return -1;
//     } else if(ib->begin<ia->begin) {
//         return 1;
//     } else {
//         return 0;
//     }
// }
// #endif

#if USE_ATOS
static void GetBacktraceSymbols(char **symbols,
                                void *const *addresses,
                                int num_addresses,
                                const Image &image) {
    std::vector<int> indexes;
    ASSERT(num_addresses >= 0);
    indexes.reserve((size_t)num_addresses);

    for (int i = 0; i < num_addresses; ++i) {
        if ((uint64_t)addresses[i] >= image.begin && (uint64_t)addresses[i] < image.end) {
            indexes.push_back(i);
        }
    }

    if (indexes.empty()) {
        return;
    }

    std::vector<char *> argv;
    argv.reserve(5 + indexes.size() + 1);

    size_t first_allocated;

    int fds[2] = {-1, -1};

    std::vector<char> output;

    // no, the const casts are not nice.
    argv.push_back((char *)"/usr/bin/atos");
    argv.push_back((char *)"-o");
    argv.push_back((char *)image.name.c_str());
    argv.push_back((char *)"-l");

    first_allocated = argv.size();

    {
        char *tmp;
        asprintf(&tmp, "0x%" PRIx64, image.begin);
        argv.push_back(tmp);
    }

    for (int index : indexes) {
        char *tmp;
        asprintf(&tmp, "%p", addresses[index]);
        argv.push_back(tmp);
    }

    argv.push_back(nullptr);

    /* for(int i=0;i<num_addresses;++i) { */
    /*     printf("%d. %p\n",i,addresses[i]); */
    /* } */

    /* for(int i=0;i<argc;++i) { */
    /*     printf("%s ",argv[i]); */
    /*     //printf("new argv[%d]: %s\n",i,argv[i]); */
    /* } */
    /* printf("\n"); */

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
            char buffer[4096];

            close(fds[1]);
            fds[1] = -1;

            for (;;) {
                ssize_t n = read(fds[0], buffer, sizeof buffer);
                //printf("n=%zd\n",n);
                if (n < 0) {
                    goto done;
                } else if (n == 0) {
                    break;
                } else {
                    output.insert(output.end(), buffer, buffer + n);
                }
            }

            output.push_back(0);

            int status;
            if (waitpid(pid, &status, 0) != pid) {
                goto done;
            }

            if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
                goto done;
            }

            {
                size_t i = 0;
                char *s = output.data(), *t;

                while (i < indexes.size() && (t = strsep(&s, "\n")) != NULL) {
                    char **p = &symbols[indexes[i]];
                    if (!*p) {
                        asprintf(p, "%p: %s", addresses[indexes[i]], t);
                    }
                    ++i;
                }
            }

            /* /\* printf("--------------------------------------------------\n"); *\/ */
            /* /\* printf("%s\n",output); *\/ */
            /* /\* printf("--------------------------------------------------\n"); *\/ */

            /* LogDumpBytes(&tmp,output.data(),output.size()); */ // the log changes have broken this
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
#if USE_ATOS

    if (num_addresses <= 0) {
        return NULL;
    }

    std::vector<Image> images;

    /* Regarding _dyld_image_count: "Note that using this count to
     * iterate all images is not thread safe, because another thread
     * may be adding or removing images during the iteration." - not
     * very clear what a good alternative is, though? The following
     * should at least not go wrong too badly... */

    uint32_t dyld_index = 0;
    for (;;) {
        const char *image_name = _dyld_get_image_name(dyld_index);
        const struct mach_header *image_header = _dyld_get_image_header(dyld_index);
        ++dyld_index;

        if (!image_name || !image_header) {
            break;
        }

        Image image;

        image.dyld_index = dyld_index;
        image.name = image_name;
        image.begin = (uint64_t)image_header;

        struct stat st;

        if (stat(image.name.c_str(), &st) == -1 || st.st_size <= 0) {
            continue;
        }

        image.end = image.begin + (uint64_t)st.st_size;

        images.push_back(image);
    }

    if (images.empty()) {
        /* Give up and just call backtrace_symbols. */
        return backtrace_symbols(addresses, num_addresses);
    }

    std::sort(images.begin(), images.end(), [](auto &&a, auto &&b) {
        return a.begin < b.begin;
    });

    for (size_t i = 0; i < images.size() - 1; ++i) {
        Image *im0 = &images[i], *im1 = &images[i + 1];

        if (im0->end > im1->begin) {
            im0->end = im1->begin;
        }
    }

    /* for(size_t i=0;i<images.size();++i) { */
    /*     const Image *im=&images[i]; */
    /*     printf("%3u. 0x%016llX +%llu: %s\n",im->dyld_index,im->begin,im->end,im->name); */
    /* } */

    std::vector<char *> symbols;
    symbols.resize((size_t)num_addresses);

    /* Batch atos invocations by image. */
    for (const Image &image : images) {
        GetBacktraceSymbols(symbols.data(), addresses, num_addresses, image);
    }

    /* If any entries are missing, fill them in with the address. */
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (!symbols[i]) {
            asprintf(&symbols[i], "%p", addresses[i]);
        }
    }

    /* Assess memory requirements. */
    size_t num_array_bytes = (size_t)num_addresses * sizeof(char *);
    size_t num_string_bytes = 0;
    for (size_t i = 0; i < symbols.size(); ++i) {
        num_string_bytes += strlen(symbols[i]) + 1;
    }

    auto result = (char *)malloc(num_array_bytes + num_string_bytes);
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

    for (char *&symbol : symbols) {
        free(symbol);
        symbol = nullptr;
    }

    return (char **)result; //backtrace_symbols(addresses,num_addresses);

#else

    return backtrace_symbols(addresses, num_addresses);

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetLowestSetBitIndex32(uint32_t value) {
    /* gcc docs have the prototype as int __builtin_ffs(unsigned), but
     * clang appears to treat the input as int... */
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
    return __builtin_ffsl((long)value) - 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetHighestSetBitIndex64(uint64_t value) {
    if (value == 0) {
        return -1;
    }

    return 63 - __builtin_clzl(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits32(uint32_t value) {
    return (size_t)__builtin_popcount(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits64(uint64_t value) {
    return (size_t)__builtin_popcountl(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t GetCurrentTickCount(void) {
    return mach_absolute_time();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsPerTick(void) {
    if (!g_got_timebase_metrics) {
        /* It doesn't matter if there's a race here; all the threads
         * will (should...) get the same value. */

        mach_timebase_info_data_t tbi;
        mach_timebase_info(&tbi);

        // Intel: 1/1 = 1.0000
        // M: 125/3 = 41.6667
        double ns_per_tick = (double)tbi.numer / tbi.denom;
        g_timebase_secs_per_tick = ns_per_tick / 1e9;

        g_got_timebase_metrics = 1;
    }

    return g_timebase_secs_per_tick;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsFromTicks(uint64_t ticks) {
    return ticks * GetSecondsPerTick();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetCurrentThreadNameInternal(const char *name) {
    pthread_setname_np(name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
