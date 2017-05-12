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
#include <mutex>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<std::string,std::vector<Log *>> *g_log_lists_by_tag;
static const std::vector<Log *> g_empty_log_list;

static void InitLogList() {
    static std::map<std::string,std::vector<Log *>> s_log_lists_by_tag;

    g_log_lists_by_tag=&s_log_lists_by_tag;
}

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

LogPrinterStd::LogPrinterStd(bool to_stdout,bool to_stderr,bool debugger):
    m_stdout(to_stdout),
    m_stderr(to_stderr),
    m_debugger(debugger)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogPrinterStd::Print(const char *str,size_t str_len) {
    if(m_stdout) {
        fwrite(str,str_len,1,stdout);
    }

    if(m_stderr) {
        fwrite(str,str_len,1,stderr);
    }

    if(m_debugger) {
#if SYSTEM_WINDOWS
        OutputDebugStringA(str);
#else
        // Try to avoid printing the same thing multiple times.
        if(!m_stdout&&!m_stderr) {
            fwrite(str,str_len,1,stderr);
        }
#endif
    }
}

LogPrinterStd log_printer_nowhere(false,false,false);
LogPrinterStd log_printer_stdout(true,false,false);
LogPrinterStd log_printer_stderr(false,true,false);
LogPrinterStd log_printer_debugger(false,false,true);
LogPrinterStd log_printer_stdout_and_debugger(true,false,true);
LogPrinterStd log_printer_stderr_and_debugger(false,true,true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinterString::LogPrinterString(std::string *str):
    m_str(str)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogPrinterString::Print(const char *str,size_t str_len) {
    if(m_str) {
        m_str->append(str,str_len);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Log::Log(const char *prefix,LogPrinter *printer,bool enabled):
    Log(nullptr,prefix,printer,enabled)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Log::Log(const char *tag,const char *prefix,LogPrinter *printer,bool enabled_) {
    this->SetPrefix(prefix);

    m_printer=printer;
    ASSERT(m_printer);

    if(tag) {
        m_tag=tag;
    }

    if(enabled_) {
        m_enable_count=1;
        this->enabled=true;
    } else {
        m_enable_count=0;
        this->enabled=false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Log::Log(const char *prefix,const Log &log):
    Log(prefix,log.m_printer,log.enabled)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Log::~Log() {
    if(m_is_printer_locked) {
        m_printer->unlock();
        m_is_printer_locked=false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Log::f(const char *fmt,...) {
    if(!this->enabled) {
        return 0;
    }

    va_list v;
    va_start(v,fmt);
    int n=this->v(fmt,v);
    va_end(v);

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Log::v(const char *fmt,va_list v_) {
    if(!this->enabled) {
        return 0;
    }

    char tmp[PRINTF_BUFFER_SIZE];

    va_list v;
    va_copy(v,v_);
    int n=vsnprintf(tmp,sizeof tmp,fmt,v);
    va_end(v);

    // It sounds like vsnprintf can't return a negative value.
    if((size_t)n<sizeof tmp-1) {
        this->s(tmp);
    } else {
        char *buf;

        va_copy(v,v_);
        n=vasprintf(&buf,fmt,v);
        va_end(v);

        this->s(buf);

        free(buf);
        buf=NULL;
    }

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::s(const char *str) {
    if(!this->enabled||!str) {
        return;
    }

    for(const char *str_c=str;*str_c!=0;++str_c) {
        int print=1;

        if(m_bol) {
            if(*str_c=='\t') {
                if(m_prefix[0]!=0) {
                    for(const char *prefix_c=m_prefix;
                        *prefix_c!=0;
                        ++prefix_c)
                    {
                        this->c(' ');
                    }

                    this->c(' ');
                    this->c(' ');
                }

                print=0;
            } else {
                if(m_prefix[0]!=0) {
                    for(const char *prefix_c=m_prefix;
                        *prefix_c!=0;
                        ++prefix_c)
                    {
                        this->c(*prefix_c);
                    }

                    this->c(':');
                    this->c(' ');
                }
            }

            /* Columns spent printing the prefix don't count. */
            m_column=0;
            
            for(int i=0;i<m_indent;++i) {
                this->c(' ');
            }
        }

        if(print) {
            c(*str_c);
        }

        m_bol=*str_c=='\n';
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::c(char c) {
    m_buffer[m_buffer_size++]=c;

    if(c=='\n'||m_buffer_size==MAX_BUFFER_SIZE-1) {
        this->Flush(c);
    }

    if(c=='\n'||c=='\r') {
        m_column=0;
    } else {
        ++m_column;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::Enable() {
    ++m_enable_count;

    this->enabled=m_enable_count>0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::Disable() {
    --m_enable_count;

    this->enabled=m_enable_count>0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::PushIndent() {
    if(m_indent_stack_depth<MAX_INDENT_STACK_DEPTH) {
        m_indent_stack[m_indent_stack_depth]=m_indent;
        m_indent=m_column;
    }

    ++m_indent_stack_depth;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::PopIndent() {
    if(m_indent_stack_depth>0) {
        --m_indent_stack_depth;

        if(m_indent_stack_depth<MAX_INDENT_STACK_DEPTH) {
            m_indent=m_indent_stack[m_indent_stack_depth];
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

void Log::Flush() {
    this->Flush(0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::EnsureBOL(const char *fmt,...) {
    if(!m_bol) {
        if(fmt) {
            va_list v;

            va_start(v,fmt);
            this->v(fmt,v);
            va_end(v);
        }

        if(!m_bol) {
            this->s("\n");
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::LockPrinterForNextLine() {
    if(!m_is_printer_locked) {
        m_printer->lock();

        m_is_printer_locked=true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinter *Log::GetPrinter() const {
    return m_printer;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::SetPrefix(const char *prefix) {
    strlcpy(m_prefix,prefix,MAX_PREFIX_SIZE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &Log::GetTag() const {
    return m_tag;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Log::Flush(char c) {
    if(m_buffer_size==0) {
        return;
    }

    ASSERT(m_buffer_size<MAX_BUFFER_SIZE);
    m_buffer[m_buffer_size]=0;

    if(!m_is_printer_locked) {
        m_printer->lock();
    }

    m_printer->Print(m_buffer,m_buffer_size);

    if(!m_is_printer_locked||c=='\n') {
        m_printer->unlock();
        m_is_printer_locked=false;
    }

    m_buffer_size=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogIndenter::LogIndenter(Log *log):
    m_log(log)
{
    if(m_log) {
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

LogIndenter::LogIndenter(LogIndenter &&oth):
    m_log(oth.m_log)
{
    oth.m_log=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogIndenter &LogIndenter::operator=(LogIndenter &&oth) {
    if(this!=&oth) {
        m_log=oth.m_log;
        oth.m_log=nullptr;
    }

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogIndenter::PopIndent() {
    if(m_log) {
        m_log->PopIndent();
        m_log=nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogRegister::LogRegister(Log *log):
    m_log(log)
{
    InitLogList();

    (*g_log_lists_by_tag)[m_log->GetTag()].push_back(log);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogRegister::~LogRegister() {
    const std::string &tag=m_log->GetTag();

    std::vector<Log *> *logs=&(*g_log_lists_by_tag)[tag];
    auto &&it=std::find(logs->begin(),logs->end(),m_log);
    ASSERT(it!=logs->end());
    logs->erase(it);
    if(logs->empty()) {
        g_log_lists_by_tag->erase(tag);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::map<std::string,std::vector<Log *>> &GetLogListsByTag() {
    InitLogList();

    return *g_log_lists_by_tag;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<Log *> &GetLogListByTag(const std::string &tag) {
    InitLogList();

    auto &&it=g_log_lists_by_tag->find(tag);
    if(it==g_log_lists_by_tag->end()) {
        return g_empty_log_list;
    } else {
        return it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogDumpBytes(Log *log,const void *begin,size_t size) {
    LogDumpBytesEx(log,begin,size,NULL);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum {
    MAX_NUM_DUMP_COLUMNS=32,
};

static const LogDumpBytesExData DEFAULT_DUMP_BYTES_EX_DATA={16};

void LogDumpBytesEx(Log *log,
                    const void *begin,
                    size_t size,
                    const LogDumpBytesExData *ex_data)
{
    if(size==0) {
        log->f("<<no data>>\n");
        return;
    }
    
    auto p=(const uint8_t *)begin;
    size_t offset=0;
    char cs[MAX_NUM_DUMP_COLUMNS+1];

    if(!ex_data) {
        ex_data=&DEFAULT_DUMP_BYTES_EX_DATA;
    }

    size_t num_dump_columns=ex_data->num_dump_columns;
    if(num_dump_columns==0) {
        num_dump_columns=DEFAULT_DUMP_BYTES_EX_DATA.num_dump_columns;
    } else if(num_dump_columns>MAX_NUM_DUMP_COLUMNS) {
        num_dump_columns=MAX_NUM_DUMP_COLUMNS;
    }
    
    cs[num_dump_columns]=0;

    int offset_width;
    if(ex_data->first_address&0xFF00000000000000ULL) {
        offset_width=16;
    } else if(ex_data->first_address&0x00FF000000000000ULL) {
        offset_width=14;
    } else if(ex_data->first_address&0x0000FF0000000000ULL) {
        offset_width=12;
    } else if(ex_data->first_address&0x000000FF00000000ULL) {
        offset_width=10;
    } else {
        offset_width=8;
    }

    while(offset<size) {
        log->LockPrinterForNextLine();

        log->f("0x%0*" PRIX64 ": ",offset_width,ex_data->first_address+offset);

        char sep[2]={0,0};

        for(size_t i=0;i<num_dump_columns;++i) {
            if(offset+i<size) {
                log->f("%02X",p[offset+i]);
            } else {
                log->s("  ");
            }

            sep[0]=' ';
            if(ex_data->highlight_fn) {
                if((*ex_data->highlight_fn)(offset+i,ex_data->highlight_data)) {
                    sep[0]='*';
                }
            }
            log->s(sep);
        }

        log->s(" ");

        for(size_t i=0;i<num_dump_columns;++i) {
            if(offset+i<size) {
                char c=(char)p[offset+i];
                
                if(c>=32&&c<=126) {
                    cs[i]=c;
                } else {
                    cs[i]='.';
                }
            } else {
                cs[i]=' ';
            }
        }

        log->s(cs);

        log->s("\n");

        offset+=num_dump_columns;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogStringPrintable(Log *log,const char *str) {
    if(!log) {
        return;
    }
    
    if(!str) {
        log->s("<<NULL>>");
    } else {
        for(const char *c=str;*c!=0;++c) {
            uint8_t x=(uint8_t)*c;

            if(x<32) {
                switch(x) {
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
            } else if(x>=32&&x<=126) {
                log->c((char)x);
            } else {
            hex:
                /* The \x notation is no good, because it isn't restricted
                 * to 2 chars. (So \177F can't be written \x7fF, because
                 * that's something else, assuming it's even valid.)
                 */
                log->f("\\%03o",x);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogStackTrace(Log *log) {
    if(!log) {
        return;
    }
    
    void *buffer[100];
    int n=backtrace(buffer,sizeof buffer/sizeof buffer[0]);

    char **symbols=GetBacktraceSymbols(buffer,n);
    
    log->f("Stack trace:");

    if(!symbols) {
        log->f(" (no symbols available)");
    }

    log->f("\n");
    

    for(int i=0;i<n;++i) {
        log->f("    %d. ",i);

        if(symbols) {
            log->f("%s",symbols[i]);
        } else {
            log->f("%p",buffer[i]);
        }

        log->f("\n");
    }

    free(symbols);
    symbols=NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
