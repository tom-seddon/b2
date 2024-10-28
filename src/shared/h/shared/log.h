#ifndef HEADER_FE87E726ACEF4A0C8277FF6432F9038E // -*- c++-mode -*-
#define HEADER_FE87E726ACEF4A0C8277FF6432F9038E

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <stdarg.h>
#include <stddef.h>
#include <shared/mutex.h>
#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class LogPrinter {
  public:
    LogPrinter();
    virtual ~LogPrinter() = 0;

    // Print entire string. (This will most likely be an entire line,
    // prefix and all, but it might not be.) The string will be
    // 0-terminated - str[str_len]==0.
    virtual void Print(const char *str, size_t str_len) = 0;

    // Lockable.
    void lock();
    void unlock();
    bool try_lock();

    void SetMutexName(std::string name);

  protected:
  private:
    Mutex m_mutex;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class LogPrinterStd : public LogPrinter {
  public:
    LogPrinterStd(bool to_stdout, bool to_stderr, bool debugger);

    void Print(const char *str, size_t str_len) override;

  protected:
  private:
    bool m_stdout = true;
    bool m_stderr = false;
    bool m_debugger = false;
};

extern LogPrinterStd log_printer_nowhere;
extern LogPrinterStd log_printer_stdout;
extern LogPrinterStd log_printer_stderr;
extern LogPrinterStd log_printer_debugger;
extern LogPrinterStd log_printer_stdout_and_debugger;
extern LogPrinterStd log_printer_stderr_and_debugger;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class LogPrinterString : public LogPrinter {
  public:
    // When the string pointer is null, output is discarded.
    explicit LogPrinterString(std::string *str = nullptr);

    void Print(const char *str, size_t str_len) override;

  protected:
  private:
    std::string *m_str = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Log {
  public:
    static const size_t MAX_PREFIX_SIZE = 50;
    static const size_t MAX_INDENT_STACK_DEPTH = 10;
    static const size_t MAX_BUFFER_SIZE = 500;

    /* Default internal buffer size for printf. No problem if the
     * expanded format string is longer than this - it will still
     * work. This value is exposed only so the test code can check
     * this works. */
    static const size_t PRINTF_BUFFER_SIZE = 1000;

    /* As tested by the LOG_PRINT macro. It's public, so it can be
     * changed externally, but it will be updated by the next
     * Enable/Disable call.
     */
    bool enabled = true;

    Log(const char *prefix, LogPrinter *printer, bool enabled = true);

    // copies printer and enabled flag.
    Log(const char *prefix, const Log &log);
    ~Log();

    Log(const Log &) = default;
    Log &operator=(const Log &) = default;
    Log(Log &&) = default;
    Log &operator=(Log &&) = default;

    int PRINTF_LIKE(2, 3) f(const char *fmt, ...);
    int v(const char *fmt, va_list v);
    void s(const char *str);
    void c(char c);
    void Enable();
    void Disable();

    void PushIndent();
    void PushIndent(int delta);
    void PopIndent();

    bool IsAtBOL() const;
    int GetColumn() const;

    void Flush();

    /* Ensures output is at beginning of line. If at beginning of line
     * already, does nothing. If not, prints (fmt,...) (or nothing, if
     * fmt is NULL), followed, if necessary, by a newline.
     */
    void PRINTF_LIKE(2, 3) EnsureBOL(const char *fmt = nullptr, ...);

    LogPrinter *GetLogPrinter() const;
    void SetLogPrinter(LogPrinter *printer);

    const char *GetPrefix() const;
    void SetPrefix(const char *prefix);

  protected:
  private:
    char m_prefix[MAX_PREFIX_SIZE] = {};
    LogPrinter *m_printer = nullptr;
    int m_enable_count = 0;
    bool m_bol = true;
    int m_column = 0;
    int m_indent = 0;
    int m_indent_stack[MAX_INDENT_STACK_DEPTH] = {};
    size_t m_indent_stack_depth = 0;
    size_t m_buffer_size = 0;
    char m_buffer[MAX_BUFFER_SIZE] = {};

    void RawChar(char c);
    void PushIndentInternal(int indent);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class LogIndenter {
  public:
    explicit LogIndenter(Log *log);
    ~LogIndenter();

    LogIndenter(const LogIndenter &) = delete;
    LogIndenter &operator=(const LogIndenter &) = delete;

    LogIndenter(LogIndenter &&) noexcept;
    LogIndenter &operator=(LogIndenter &&) noexcept;

    void PopIndent();

  protected:
  private:
    Log *m_log = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class LogWithTag {
  public:
    const char *const tag = nullptr;
    Log *const log = nullptr;

    explicit LogWithTag(const char *tag, Log *log);
    ~LogWithTag();

    static const LogWithTag *GetFirst();
    const LogWithTag *GetNext() const;

  protected:
  private:
    LogWithTag *m_next = nullptr;

    static LogWithTag *ms_first;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct LogSet {
    Log &i;
    Log &w;
    Log &e;

    LogSet() = delete;
    //LogSet(Log &i, Log &w, Log &e);
    LogSet(const LogSet &src) = delete;
    LogSet &operator=(const LogSet &src) = delete;
    LogSet(LogSet &&src) = delete;
    LogSet &operator=(const LogSet &&src) = delete;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogDumpBytes(Log *log, const void *p, size_t n);

typedef int (*LogDumpHighlightFn)(size_t offset, void *data);

struct LogDumpBytesExData {
    size_t num_dump_columns;

    LogDumpHighlightFn highlight_fn;
    void *highlight_data;

    uint64_t first_address;
};

void LogDumpBytesEx(Log *log, const void *p, size_t n,
                    const LogDumpBytesExData *ex_data);

/* Prints the given string, escaped, so it's entirely printable chars. */
void LogStringPrintable(Log *log, const char *str);

/* Print stack trace. */
void LogStackTrace(Log *log);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// These macros provide a simple way of defining new logs (whose names
// needn't be valid prefixes of C identifiers), printing to them from
// any source file, even when the definition isn't visible, and
// printing in a fairly efficient manner (the Log's public enabled
// field is tested, so the format arguments are only evaluated when
// the log is enabled).

#define LOG__IS_ENABLED(X) ((X).enabled)

#define LOG(X) g_log_##X

#define LOG_EXTERN(X) extern Log LOG(X)

// C4456: declaration of 'IDENTIFIER' hides previous local
// declaration, which can pop up if LOG_EXTERN was previously used in
// the same scope. Not a very useful warning in this case.
#define LOG__PRINT(X, FUNC, ARGS)      \
    BEGIN_MACRO {                      \
        VC_WARN_PUSH_DISABLE(4456);    \
        LOG_EXTERN(X);                 \
        VC_WARN_POP();                 \
                                       \
        if (LOG__IS_ENABLED(LOG(X))) { \
            LOG(X).FUNC ARGS;          \
        }                              \
    }                                  \
    END_MACRO

#define LOG_DEFINE(NAME, ...) Log LOG(NAME)(__VA_ARGS__)

#define LOG_TAGGED_DEFINE(NAME, TAG, ...) \
    LOG_DEFINE(NAME, __VA_ARGS__);        \
    static LogWithTag g_log_with_tag_##NAME(#TAG, &LOG(NAME))

// The indentation level is popped at the end of the current scope.
#define LOGI(X) LogIndenter CONCAT2(indenter, __COUNTER__)(&LOG(X))

#define LOGF(X, ...) LOG__PRINT(X, f, (__VA_ARGS__))
#define LOGV(X, FMT, V) LOG__PRINT(X, v, ((FMT), (V)))
#define LOG_STR(X, STR) LOG__PRINT(X, s, ((STR)))

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif //HEADER_FE87E726ACEF4A0C8277FF6432F9038E
