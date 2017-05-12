#ifndef HEADER_3D2FA1AC94784254AE3F43FF2D06F61A 
#define HEADER_3D2FA1AC94784254AE3F43FF2D06F61A
#include "cpp_begin.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Test macros preserve errno if the test passes. */

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Call to disable some modal Windows dialogs that are a bit annoying.
 * Does nothing on Linux/OS X. (Automatically called as necessary.)
 */
void TestStartup(void);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TestFailArgs {
    void *context;
    
    const int64_t *wanted_int64,*got_int64;

    /* These may return at some point. */
    
    /* const uint64_t *wanted_uint64,*got_uint64; */
    
    /* const int *wanted_bool,*got_bool; */
    
    /* const char *wanted_str,*got_str; */

    /* /\* array_size is only valid if wanted_array/got_array are non-NULL. *\/ */
    /* const void *wanted_array,*got_array; */
    /* size_t array_size; */

    /* const void *wanted_ptr,*got_ptr; */
};
typedef struct TestFailArgs TestFailArgs;

typedef void (*TestFailFn)(const TestFailArgs *tfa);

void TestFailed(const char *file,int line,const TestFailArgs *tfa);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AddTestFailFn(TestFailFn fn,void *context);
void RemoveTestFailFnByContext(void *context);

void TestQuit(void);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define TEST(EXPR)\
    BEGIN_MACRO {\
        if(!(EXPR)) {\
            TestStartup();\
            BREAK();\
            TestQuit();\
        }\
    } END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqII(int64_t got,const char *got_str,int64_t wanted,const char *wanted_str,const char *file,int line);

#define TEST_EQ_II(GOT,WANTED) TEST(TestEqII((GOT),#GOT,(WANTED),#WANTED,__FILE__,__LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqUU(uint64_t got,const char *got_str,uint64_t wanted,const char *wanted_str,const char *file,int line);

#define TEST_EQ_UU(GOT,WANTED) TEST(TestEqUU((GOT),#GOT,(WANTED),#WANTED,__FILE__,__LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestBool(int got,const char *got_str,int wanted,const char *file,int line);

#define TEST_TRUE(GOT) TEST(TestBool((GOT),#GOT,1,__FILE__,__LINE__))
#define TEST_FALSE(GOT) TEST(TestBool((GOT),#GOT,0,__FILE__,__LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestPointer(int got_null,const char *got_str,int want_not_null,const char *file,int line);

#define TEST_NULL(GOT) TEST(TestPointer((GOT)!=NULL,#GOT,0,__FILE__,__LINE__))
#define TEST_NON_NULL(GOT) TEST(TestPointer((GOT)!=NULL,#GOT,1,__FILE__,__LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqSS(const char *got,const char *got_str,const char *wanted,const char *wanted_str,const char *file,int line);

#define TEST_EQ_SS(GOT,WANTED) TEST(TestEqSS((GOT),#GOT,(WANTED),#WANTED,__FILE__,__LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqAA(const void *got,const char *got_str,const void *wanted,const char *wanted_str,size_t n,const char *file,int line);

#define TEST_EQ_AA(GOT,WANTED,N) TEST(TestEqAA((GOT),#GOT,(WANTED),#WANTED,(N),__FILE__,__LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqPP(const void *got,const char *got_str,const void *wanted,const char *wanted_str,const char *file,int line);

#define TEST_EQ_PP(GOT,WANTED) TEST(TestEqPP((GOT),#GOT,(WANTED),#WANTED,__FILE__,__LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "cpp_end.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus

// This code started out as C99. Here's a quick workaround for
// std::string.

#include <string>

static inline int TestEqSS2(const std::string &got,const char *got_str,const std::string &wanted,const char *wanted_str,const char *file,int line) {
    return TestEqSS(got.c_str(),got_str,wanted.c_str(),wanted_str,file,line);
}

#define TEST_EQ_SS2(GOT,WANTED) TEST(TestEqSS2((GOT),#GOT,(WANTED),#WANTED,__FILE__,__LINE__))

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif//HEADER_3D2FA1AC94784254AE3F43FF2D06F61A
