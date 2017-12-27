#include <shared/system.h>
#include <shared/testing.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <string>
#include <atomic>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Can probably work around any of these if there's a problem...

#if ATOMIC_BOOL_LOCK_FREE!=2
#error not always lock free (ATOMIC_BOOL_LOCK_FREE)
#endif
#if ATOMIC_CHAR_LOCK_FREE!=2
#error not always lock free (ATOMIC_CHAR_LOCK_FREE)
#endif
#if ATOMIC_CHAR16_T_LOCK_FREE!=2
#error not always lock free (ATOMIC_CHAR16_T_LOCK_FREE)
#endif
#if ATOMIC_CHAR32_T_LOCK_FREE!=2
#error not always lock free (ATOMIC_CHAR32_T_LOCK_FREE)
#endif
#if ATOMIC_WCHAR_T_LOCK_FREE!=2
#error not always lock free (ATOMIC_WCHAR_T_LOCK_FREE)
#endif
#if ATOMIC_SHORT_LOCK_FREE!=2
#error not always lock free (ATOMIC_SHORT_LOCK_FREE)
#endif
#if ATOMIC_INT_LOCK_FREE!=2
#error not always lock free (ATOMIC_INT_LOCK_FREE)
#endif
#if ATOMIC_LONG_LOCK_FREE!=2
#error not always lock free (ATOMIC_LONG_LOCK_FREE)
#endif
#if ATOMIC_LLONG_LOCK_FREE!=2
#error not always lock free (ATOMIC_LLONG_LOCK_FREE)
#endif
#if ATOMIC_POINTER_LOCK_FREE!=2
#error not always lock free (ATOMIC_POINTER_LOCK_FREE)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void TestImplementationRequirements(void) {
    /* These requirements may be relaxed later. */
    TEST_TRUE(sizeof(void *)==4||sizeof(void *)==8);
    TEST_EQ_UU(CHAR_BIT,8);

    /* No harm in knowing... */
    TEST_EQ_UU(sizeof(void *),sizeof(void (*)(void)));

    /* NULL must be all bits clear. */
    void **p=(void **)calloc(1,sizeof *p);
    TEST_NON_NULL(p);
    TEST_NULL(*p);
    free(p);
    p=NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void TestBitScans(void) {
    TEST_EQ_II(GetLowestSetBitIndex32(0),-1);
    TEST_EQ_II(GetHighestSetBitIndex32(0),-1);

    TEST_EQ_II(GetLowestSetBitIndex64(0),-1);
    TEST_EQ_II(GetHighestSetBitIndex64(0),-1);

    for(int i=0;i<32;++i) {
        TEST_EQ_II(GetLowestSetBitIndex32(1u<<i),i);
        TEST_EQ_II(GetHighestSetBitIndex32(1u<<i),i);
    }

    for(int i=0;i<64;++i) {
        TEST_EQ_II(GetLowestSetBitIndex64(1ull<<i),i);
        TEST_EQ_II(GetHighestSetBitIndex64(1ull<<i),i);
    }

    TEST_EQ_UU(GetNumSetBits32(0),0);
    TEST_EQ_UU(GetNumSetBits64(0),0);

    for(int i=0;i<1000;++i) {
        uint8_t buf[8];
        for(int j=0;j<8;++j) {
            buf[j]=(uint8_t)rand();
        }

        uint32_t v32;
        memcpy(&v32,buf,4);

        uint64_t v64;
        memcpy(&v64,buf,8);

        size_t n32=0,n64=0;
        for(int j=0;j<64;++j) {
            uint64_t mask=1ull<<j;

            if(v32&mask) {
                ++n32;
            }

            if(v64&mask) {
                ++n64;
            }
        }

        TEST_EQ_UU(GetNumSetBits32(v32),n32);
        TEST_EQ_UU(GetNumSetBits64(v64),n64);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int PRINTF_LIKE(3,4) wrap_vsnprintf(char *buf,size_t buf_size,const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    int n=vsnprintf(buf,buf_size,fmt,v);
    va_end(v);

    return n;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

static void
#ifdef __GNUC__
// if gcc is allowed to inline the function, a format-truncation
// warning is emitted from main, into which it has presumably been
// inlined.
__attribute__((noinline))
#endif
TestC99() {
    char buf[3];
    size_t value=12345;
    int n;

    n=snprintf(buf,sizeof buf,"%zu",value);
    TEST_EQ_II(n,5);
    TEST_EQ_SS(buf,"12");

    n=wrap_vsnprintf(buf,sizeof buf,"%zu",value);
    TEST_EQ_II(n,5);
    TEST_EQ_SS(buf,"12");

    snprintf(buf,sizeof buf,"%zx",value);
    TEST_EQ_SS(buf,"30");
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int PRINTF_LIKE(2,3) wrap_vasprintf(char **buf,const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    int n=vasprintf(buf,fmt,v);
    va_end(v);

    return n;
}

static void TestGNU() {
    std::string large_string;
    large_string.reserve(1024*1024+1024);

    while(large_string.size()<1024*1024) {
        large_string+=std::to_string(large_string.size());
    }

    char *tmp;
    int n;

    n=asprintf(&tmp,"%s",large_string.c_str());
    TEST_TRUE(n>=0);
    TEST_EQ_UU((size_t)n,large_string.size());
    TEST_EQ_UU(strlen(tmp),large_string.size());
    TEST_EQ_SS(tmp,large_string.c_str());
    free(tmp);
    tmp=nullptr;

    n=wrap_vasprintf(&tmp,"%s",large_string.c_str());
    TEST_TRUE(n>=0);
    TEST_EQ_UU((size_t)n,large_string.size());
    TEST_EQ_UU(strlen(tmp),large_string.size());
    TEST_EQ_SS(tmp,large_string.c_str());
    free(tmp);
    tmp=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void TestBSD() {
    char buf[5];

    char str[]="LONGERSTRING";

    strlcpy(buf,str,5);

    TEST_EQ_UU(strlen(buf),4);
    TEST_EQ_SS(buf,"LONG");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(void) {
    TestImplementationRequirements();

    TestC99();
    TestGNU();
    TestBSD();

    TestBitScans();
}
