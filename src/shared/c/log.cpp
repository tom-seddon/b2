#include <shared/system.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <shared/system_specific.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <inttypes.h>
#include <shared/mutex.h>
#include <algorithm>
#include <map>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinter::LogPrinter() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinter::~LogPrinter() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogPrinter::lock() {
    m_mutex.lock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogPrinter::unlock() {
    m_mutex.unlock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LogPrinter::try_lock() {
    return m_mutex.try_lock();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogPrinter::SetMutexName(std::string name) {
    (void)name;
    MUTEX_SET_NAME(m_mutex, name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinterStd::LogPrinterStd(bool to_stdout, bool to_stderr, bool debugger)
    : m_stdout(to_stdout)
    , m_stderr(to_stderr)
    , m_debugger(debugger) {
    this->SetMutexName((std::string("LogPrinterStd (out=") + BOOL_STR(to_stdout) + " err=" + BOOL_STR(to_stderr) + " debug=" + BOOL_STR(debugger) + ")"));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogPrinterStd::Print(const char *str, size_t str_len) {
    if (m_stdout) {
        fwrite(str, str_len, 1, stdout);
    }

    if (m_stderr) {
        fwrite(str, str_len, 1, stderr);
    }

    if (m_debugger) {
#if SYSTEM_WINDOWS
        OutputDebugStringA(str);
#else
        // Try to avoid printing the same thing multiple times.
        if (!m_stdout && !m_stderr) {
            fwrite(str, str_len, 1, stderr);
        }
#endif
    }
}

LogPrinterStd log_printer_nowhere(false, false, false);
LogPrinterStd log_printer_stdout(true, false, false);
LogPrinterStd log_printer_stderr(false, true, false);
LogPrinterStd log_printer_debugger(false, false, true);
LogPrinterStd log_printer_stdout_and_debugger(true, false, true);
LogPrinterStd log_printer_stderr_and_debugger(false, true, true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinterString::LogPrinterString(std::string *str)
    : m_str(str) {
    this->SetMutexName("LogPrinterString");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogPrinterString::Print(const char *str, size_t str_len) {
    if (m_str) {
        m_str->append(str, str_len);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Log::Log(const char *prefix, LogPrinter *printer, bool enabled_) {
    this->SetPrefix(prefix);

    this->SetLogPrinter(printer);

    if (enabled_) {
        m_enable_count = 1;
        this->enabled = true;
    } else {
        m_enable_count = 0;
        this->enabled = false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Log::Log(const char *prefix, const Log &log)
    : Log(prefix, log.m_printer, log.enabled) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Log::~Log() {
    this->SetLogPrinter(nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Log::f(const char *fmt, ...) {
    if (!this->enabled) {
        return 0;
    }

    va_list v;
    va_start(v, fmt);
    int n = this->v(fmt, v);
    va_end(v);

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Log::v(const char *fmt, va_list v_) {
    if (!this->enabled) {
        return 0;
    }

    char tmp[PRINTF_BUFFER_SIZE];

    va_list v;
    va_copy(v, v_);
    int n = vsnprintf(tmp, sizeof tmp, fmt, v);
    va_end(v);

    // It sounds like vsnprintf can't return a negative value.
    if ((size_t)n < sizeof tmp - 1) {
        this->s(tmp);
    } else {
        char *buf;

        va_copy(v, v_);
        n = vasprintf(&buf, fmt, v);
        va_end(v);

        this->s(buf);

        free(buf);
        buf = NULL;
    }

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::s(const char *str) {
    if (!this->enabled || !str) {
        return;
    }

    for (const char *c = str; *c != 0; ++c) {
        this->c(*c);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::c(char c) {
    if (!this->enabled) {
        return;
    }

    bool print = true;

    if (m_bol) {
        if (c == '\t') {
            if (m_prefix[0] != 0) {
                for (const char *prefix_c = m_prefix;
                     *prefix_c != 0;
                     ++prefix_c) {
                    this->RawChar(' ');
                }

                this->RawChar(' ');
                this->RawChar(' ');
            }

            print = false;
        } else {
            if (m_prefix[0] != 0) {
                for (const char *prefix_c = m_prefix;
                     *prefix_c != 0;
                     ++prefix_c) {
                    this->RawChar(*prefix_c);
                }

                this->RawChar(':');
                this->RawChar(' ');
            }
        }

        /* Columns spent printing the prefix don't count. */
        m_column = 0;

        for (int i = 0; i < m_indent; ++i) {
            this->RawChar(' ');
        }
    }

    if (print) {
        this->RawChar(c);
    }

    m_bol = c == '\n';
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::Enable() {
    ++m_enable_count;

    this->enabled = m_enable_count > 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::Disable() {
    --m_enable_count;

    this->enabled = m_enable_count > 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::PushIndent() {
    this->PushIndentInternal(m_column);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::PushIndent(int delta) {
    this->PushIndentInternal(m_indent + delta);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::PopIndent() {
    if (m_indent_stack_depth > 0) {
        --m_indent_stack_depth;

        if (m_indent_stack_depth < MAX_INDENT_STACK_DEPTH) {
            m_indent = m_indent_stack[m_indent_stack_depth];
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Log::IsAtBOL() const {
    return m_bol;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Log::GetColumn() const {
    return m_column;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::RawChar(char c) {
    m_buffer[m_buffer_size++] = c;

    if (c == '\n' || m_buffer_size == MAX_BUFFER_SIZE - 1) {
        this->Flush();
    }

    if (c == '\n' || c == '\r') {
        m_column = 0;
    } else {
        ++m_column;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::Flush() {
    if (m_buffer_size == 0) {
        return;
    }

    ASSERT(m_buffer_size < MAX_BUFFER_SIZE);
    m_buffer[m_buffer_size] = 0;

    if (m_printer) {
        LockGuard<LogPrinter> lock(*m_printer);

        m_printer->Print(m_buffer, m_buffer_size);
    }

    m_buffer_size = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::EnsureBOL(const char *fmt, ...) {
    if (!m_bol) {
        if (fmt) {
            va_list v;

            va_start(v, fmt);
            this->v(fmt, v);
            va_end(v);
        }

        if (!m_bol) {
            this->s("\n");
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinter *Log::GetLogPrinter() const {
    return m_printer;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::SetLogPrinter(LogPrinter *printer) {
    if (printer != m_printer) {
        this->Flush();
        m_printer = printer;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *Log::GetPrefix() const {
    return m_prefix;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::SetPrefix(const char *prefix) {
    strlcpy(m_prefix, prefix, MAX_PREFIX_SIZE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::PushIndentInternal(int indent) {
    if (m_indent_stack_depth < MAX_INDENT_STACK_DEPTH) {
        m_indent_stack[m_indent_stack_depth] = m_indent;
        m_indent = indent;
    }

    ++m_indent_stack_depth;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogIndenter::LogIndenter(Log *log)
    : m_log(log) {
    if (m_log) {
        m_log->PushIndent();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogIndenter::~LogIndenter() {
    this->PopIndent();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogIndenter::LogIndenter(LogIndenter &&oth) noexcept
    : m_log(oth.m_log) {
    oth.m_log = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogIndenter &LogIndenter::operator=(LogIndenter &&oth) noexcept {
    if (this != &oth) {
        m_log = oth.m_log;
        oth.m_log = nullptr;
    }

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogIndenter::PopIndent() {
    if (m_log) {
        m_log->PopIndent();
        m_log = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogWithTag *LogWithTag::ms_first;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const LogWithTag *LogWithTag::GetFirst() {
    return ms_first;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogWithTag::LogWithTag(const char *tag_, Log *log_)
    : tag(tag_)
    , log(log_)
    , m_next(ms_first) {
    ms_first = this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogWithTag::~LogWithTag() {
    // Dumb
    bool found = false;
    for (LogWithTag **ptr = &ms_first; *ptr; ptr = &(*ptr)->m_next) {
        if (*ptr == this) {
            *ptr = m_next;
            found = true;
            break;
        }
    }

    ASSERT(found);
    (void)found;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const LogWithTag *LogWithTag::GetNext() const {
    return m_next;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogDumpBytes(Log *log, const void *begin, size_t size) {
    LogDumpBytesEx(log, begin, size, NULL);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum {
    MAX_NUM_DUMP_COLUMNS = 32,
};

static const LogDumpBytesExData DEFAULT_DUMP_BYTES_EX_DATA = {16};

void LogDumpBytesEx(Log *log,
                    const void *begin,
                    size_t size,
                    const LogDumpBytesExData *ex_data) {
    if (size == 0) {
        log->f("<<no data>>\n");
        return;
    }

    auto p = (const uint8_t *)begin;
    size_t offset = 0;
    char cs[MAX_NUM_DUMP_COLUMNS + 1];

    if (!ex_data) {
        ex_data = &DEFAULT_DUMP_BYTES_EX_DATA;
    }

    size_t num_dump_columns = ex_data->num_dump_columns;
    if (num_dump_columns == 0) {
        num_dump_columns = DEFAULT_DUMP_BYTES_EX_DATA.num_dump_columns;
    } else if (num_dump_columns > MAX_NUM_DUMP_COLUMNS) {
        num_dump_columns = MAX_NUM_DUMP_COLUMNS;
    }

    cs[num_dump_columns] = 0;

    int offset_width;
    if (ex_data->first_address & 0xFF00000000000000ULL) {
        offset_width = 16;
    } else if (ex_data->first_address & 0x00FF000000000000ULL) {
        offset_width = 14;
    } else if (ex_data->first_address & 0x0000FF0000000000ULL) {
        offset_width = 12;
    } else if (ex_data->first_address & 0x000000FF00000000ULL) {
        offset_width = 10;
    } else {
        offset_width = 8;
    }

    while (offset < size) {
        log->f("0x%0*" PRIX64 ": ", offset_width, ex_data->first_address + offset);

        char sep[2] = {0, 0};

        for (size_t i = 0; i < num_dump_columns; ++i) {
            if (offset + i < size) {
                log->f("%02X", p[offset + i]);
            } else {
                log->s("  ");
            }

            sep[0] = ' ';
            if (ex_data->highlight_fn) {
                if ((*ex_data->highlight_fn)(offset + i, ex_data->highlight_data)) {
                    sep[0] = '*';
                }
            }
            log->s(sep);
        }

        log->s(" ");

        for (size_t i = 0; i < num_dump_columns; ++i) {
            if (offset + i < size) {
                char c = (char)p[offset + i];

                if (c >= 32 && c <= 126) {
                    cs[i] = c;
                } else {
                    cs[i] = '.';
                }
            } else {
                cs[i] = ' ';
            }
        }

        log->s(cs);

        log->s("\n");

        offset += num_dump_columns;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogStringPrintable(Log *log, const char *str) {
    if (!log) {
        return;
    }

    if (!str) {
        log->s("<<NULL>>");
    } else {
        for (const char *c = str; *c != 0; ++c) {
            uint8_t x = (uint8_t)*c;

            if (x < 32) {
                switch (x) {
                case '\n':
                    log->s("\\n");
                    break;

                case '\t':
                    log->s("\\t");
                    break;

                case '\r':
                    log->s("\\r");
                    break;

                case '\b':
                    log->s("\\b");
                    break;

                default:
                    goto hex;
                }
            } else if (x >= 32 && x <= 126) {
                log->c((char)x);
            } else {
            hex:
                // The \x notation isn't restricted to 2 chars. So use octal
                // notation if there's a hex digit following.
                if ((c[1] >= '0' && c[1] <= '9') ||
                    (c[1] >= 'A' && c[1] <= 'F') ||
                    (c[1] >= 'a' && c[1] <= 'f')) {
                    log->f("\\%03o", x);
                } else {
                    log->f("\\x%02x", x);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogStackTrace(Log *log) {
    if (!log) {
        return;
    }

    void *buffer[100];
    int n = backtrace(buffer, sizeof buffer / sizeof buffer[0]);

    char **symbols = GetBacktraceSymbols(buffer, n);

    log->f("Stack trace:");

    if (!symbols) {
        log->f(" (no symbols available)");
    }

    log->f("\n");

    for (int i = 0; i < n; ++i) {
        log->f("    %d. ", i);

        if (symbols) {
            log->f("%s", symbols[i]);
        } else {
            log->f("%p", buffer[i]);
        }

        log->f("\n");
    }

    free(symbols);
    symbols = NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
