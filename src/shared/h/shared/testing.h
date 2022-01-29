#ifndef HEADER_3D2FA1AC94784254AE3F43FF2D06F61A
#define HEADER_3D2FA1AC94784254AE3F43FF2D06F61A

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Test macros preserve errno if the test passes. */

#include <string>
#include <functional>

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

    const int64_t *lhs_int64, *rhs_int64;

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

void TestFailed(const char *file, int line, const TestFailArgs *tfa);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AddTestFailFn(TestFailFn fn, void *context);
void RemoveTestFailFnByContext(void *context);

void NORETURN TestQuit(void);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define TEST(...)                       \
    BEGIN_MACRO {                       \
        if (!(__VA_ARGS__)) {           \
            TestStartup();              \
            if (IsDebuggerAttached()) { \
                DEBUG_BREAK();          \
            }                           \
            TestQuit();                 \
        }                               \
    }                                   \
    END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class NumType, template <class> class Oper>
static bool TestXX(NumType lhs,
                   const char *lhs_str,
                   NumType rhs,
                   const char *rhs_str,
                   const char *oper_str,
                   const char *file,
                   int line,
                   void (*failed_fn)(NumType lhs,
                                     const char *lhs_str,
                                     NumType rhs,
                                     const char *rhs_str,
                                     const char *oper_str,
                                     const char *file,
                                     int line)) {
    Oper<NumType> oper;
    if (oper(lhs, rhs)) {
        return true;
    } else {
        (*failed_fn)(lhs, lhs_str, rhs, rhs_str, oper_str, file, line);
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestFailedII(int64_t lhs,
                  const char *lhs_str,
                  int64_t rhs,
                  const char *rhs_str,
                  const char *oper_str,
                  const char *file,
                  int line);

#define TEST_II_2(LHS, RHS, OPER) TEST(TestXX<int64_t, std::OPER>((LHS), #LHS, (RHS), #RHS, #OPER, __FILE__, __LINE__, &TestFailedII))

#define TEST_EQ_II(LHS, RHS) TEST_II_2(LHS, RHS, equal_to)
#define TEST_NE_II(LHS, RHS) TEST_II_2(LHS, RHS, not_equal_to)
#define TEST_LT_II(LHS, RHS) TEST_II_2(LHS, RHS, less)
#define TEST_LE_II(LHS, RHS) TEST_II_2(LHS, RHS, less_equal)
#define TEST_GT_II(LHS, RHS) TEST_II_2(LHS, RHS, greater)
#define TEST_GE_II(LHS, RHS) TEST_II_2(LHS, RHS, greater_equal)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestFailedUU(uint64_t lhs,
                  const char *lhs_str,
                  uint64_t rhs,
                  const char *rhs_str,
                  const char *oper_str,
                  const char *file,
                  int line);

#define TEST_UU_2(LHS, RHS, OPER) TEST(TestXX<uint64_t, std::OPER>((LHS), #LHS, (RHS), #RHS, #OPER, __FILE__, __LINE__, &TestFailedUU))

#define TEST_EQ_UU(LHS, RHS) TEST_UU_2(LHS, RHS, equal_to)
#define TEST_NE_UU(LHS, RHS) TEST_UU_2(LHS, RHS, not_equal_to)
#define TEST_LT_UU(LHS, RHS) TEST_UU_2(LHS, RHS, less)
#define TEST_LE_UU(LHS, RHS) TEST_UU_2(LHS, RHS, less_equal)
#define TEST_GT_UU(LHS, RHS) TEST_UU_2(LHS, RHS, greater)
#define TEST_GE_UU(LHS, RHS) TEST_UU_2(LHS, RHS, greater_equal)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestBool(int got, const char *got_str, int wanted, const char *file, int line);

#define TEST_TRUE(GOT) TEST(TestBool((GOT), #GOT, 1, __FILE__, __LINE__))
#define TEST_FALSE(GOT) TEST(TestBool((GOT), #GOT, 0, __FILE__, __LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestPointer(int got_null, const char *got_str, int want_not_null, const char *file, int line);

#define TEST_NULL(GOT) TEST(TestPointer((GOT) != NULL, #GOT, 0, __FILE__, __LINE__))
#define TEST_NON_NULL(GOT) TEST(TestPointer((GOT) != NULL, #GOT, 1, __FILE__, __LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqSS(const char *got, const char *got_str, const char *wanted, const char *wanted_str, const char *file, int line);
int TestEqSS(const std::string &got, const char *got_str, const std::string &wanted, const char *wanted_str, const char *file, int line);

#define TEST_EQ_SS(GOT, WANTED) TEST(TestEqSS((GOT), #GOT, (WANTED), #WANTED, __FILE__, __LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqAA(const void *got, const char *got_str, const void *wanted, const char *wanted_str, size_t n, const char *file, int line);

#define TEST_EQ_AA(GOT, WANTED, N) TEST(TestEqAA((GOT), #GOT, (WANTED), #WANTED, (N), __FILE__, __LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqPP(const void *got, const char *got_str, const void *wanted, const char *wanted_str, const char *file, int line);

#define TEST_EQ_PP(GOT, WANTED) TEST(TestEqPP((GOT), #GOT, (WANTED), #WANTED, __FILE__, __LINE__))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PRINTF_LIKE(3, 4) TestFail(const char *file, int line, const char *fmt, ...);

#define TEST_FAIL(...) TEST(TestFail(__FILE__, __LINE__, __VA_ARGS__), false)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif //HEADER_3D2FA1AC94784254AE3F43FF2D06F61A
