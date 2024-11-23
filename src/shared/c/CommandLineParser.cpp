#include <shared/system.h>
#include <shared/CommandLineParser.h>
#include <shared/debug.h>
#include <shared/log.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <vector>
#include <string>
#include <limits.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LOG_DEFINE(CLO_HELP, "", &log_printer_stdout);
static LOG_DEFINE(CLO_ERROR, "", &log_printer_stderr);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::Help(std::string help_) {
    this->help = std::move(help_);

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::Meta(std::string meta_) {
    this->meta = std::move(meta_);

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::ShowDefault() {
    this->show_default = true;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::SetIfPresent(bool *set_if_present_ptr_) {
    this->set_if_present_ptr = set_if_present_ptr_;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::ResetIfPresent(bool *reset_if_present_ptr_) {
    this->reset_if_present_ptr = reset_if_present_ptr_;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::Arg(std::string *str_ptr_) {
    this->str_ptr = str_ptr_;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::AddArgToList(std::vector<std::string> *strv_ptr_) {
    this->strv_ptr = strv_ptr_;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::Option::Arg(int *int_ptr_) {
    this->int_ptr = int_ptr_;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::CommandLineParser(std::string description, std::string summary)
    : m_description(std::move(description))
    , m_summary(std::move(summary)) {
    this->SetLogs(nullptr, nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::AddOption(char short_option) {
    return *this->AddOption(short_option, nullptr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::AddOption(char short_option, std::string long_option) {
    return *this->AddOption(short_option, &long_option);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandLineParser::Option &CommandLineParser::AddOption(std::string long_option) {
    return *this->AddOption(0, &long_option);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandLineParser::AddHelpOption(bool *flag_ptr) {
    this->AddOption(0, "help").SetIfPresent(&m_help).Help("display help");

    m_help_flag_ptr = flag_ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandLineParser::SetLogs(Log *help_log, Log *error_log) {
    if (help_log) {
        m_help_log = help_log;
    } else {
        m_help_log = &LOG(CLO_HELP);
    }

    if (error_log) {
        m_error_log = error_log;
    } else {
        m_error_log = &LOG(CLO_ERROR);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandLineParser::Parse(int argc,
                              const char *const argv[],
                              std::vector<std::string> *other_args) const {
    int i = 1;

    m_help = false;

    if (m_help_flag_ptr) {
        *m_help_flag_ptr = false;
    }

    while (i < argc) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                // GNU-style long arg
                std::string long_option = argv[i] + 2;
                std::string arg;
                bool got_arg = false;

                std::string::size_type eq = long_option.find_first_of("=");
                if (eq != std::string::npos) {
                    got_arg = true;
                    arg = long_option.substr(eq + 1);
                    long_option = long_option.substr(0, eq);
                }

                std::shared_ptr<Option> option = this->FindByLongOption(long_option);
                if (!option) {
                    m_error_log->f("unknown long option: --%s\n",
                                   long_option.c_str());
                    return false;
                }

                if (this->HandleOption(option)) {
                    if (!got_arg) {
                        m_error_log->f("missing argument for option: --%s\n",
                                       long_option.c_str());
                        return false;
                    }

                    if (!this->DoArgument(option, arg)) {
                        return false;
                    }
                } else {
                    if (got_arg) {
                        m_error_log->f("unexpected argument for option: --%s\n",
                                       long_option.c_str());
                        return false;
                    }
                }
            } else {
                for (const char *c = argv[i] + 1; *c != 0; ++c) {
                    std::shared_ptr<Option> option = this->FindByShortOption(*c);
                    if (!option) {
                        m_error_log->f("unknown short option: -%c\n", *c);
                        return false;
                    }

                    if (this->HandleOption(option)) {
                        if (c[1] != 0 || i + 1 == argc) {
                            m_error_log->f("missing argument for option: -%c\n", *c);
                            return false;
                        }

                        ++i;

                        if (!this->DoArgument(option, argv[i])) {
                            return false;
                        }
                    }
                }
            }
        } else {
            if (other_args) {
                other_args->push_back(argv[i]);
            } else {
                m_error_log->f("unexpected other arg: %s\n", argv[i]);
                return false;
            }
        }

        ++i;
    }

    if (m_help) {
        this->Help(argv[0]);

        if (m_help_flag_ptr) {
            *m_help_flag_ptr = true;
        }

        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandLineParser::Parse(int argc,
                              char *argv[],
                              std::vector<std::string> *other_args) const {
    return this->Parse(argc, (const char **)argv, other_args);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void PrintWrappedLines(Log *log,
                              const std::string &str) {
    int wrap_column = GetTerminalWidth() - 1;
    if (log->GetColumn() > wrap_column) {
        // Terminal is probably super-narrow, so it's going to make a
        // mess anyway.
        wrap_column = 80;
    }

    bool first = true;
    const char *a = str.c_str();
    while (*a != 0) {
        /* Skip current spaces. */
        while (*a == ' ') {
            ++a;
        }
        if (*a == 0) {
            continue;
        }

        /* Find end of word. */
        const char *b = a;
        while (*b != ' ' && *b != 0) {
            ++b;
        }

        int n = (int)(b - a);

        if (log->GetColumn() + n >= wrap_column) {
            log->s("\n");
        }

        if (first) {
            first = 0;
        } else {
            if (!log->IsAtBOL()) {
                log->s(" ");
            }
        }

        log->f("%.*s", n, a);

        a = b;
    }
}

static void MoveToColumn(Log *log, int col) {
    if (log->GetColumn() >= col) {
        log->s("\n");
    }

    while (log->GetColumn() < col) {
        log->s(" ");
    }
}

void CommandLineParser::Help(const char *argv0) const {
    static const int INDENT = 2;
    static const int BRIEF_COLUMN_WIDTH = 20;

    bool neat = true;
    if (GetTerminalWidth() == INT_MAX) {
        neat = false;
    }

    if (argv0 && !m_summary.empty()) {
        m_help_log->f("Syntax: %s %s\n\n", argv0, m_summary.c_str());
    }

    if (!m_description.empty()) {
        PrintWrappedLines(m_help_log, m_description);

        m_help_log->EnsureBOL();
        m_help_log->s("\n");
    }

    if (!m_options.empty()) {
        m_help_log->f("Options:\n\n");

        if (neat) {
            m_help_log->f("%*s", INDENT, "");
        }

        LogIndenter indenter(m_help_log);

        for (auto &&option : m_options) {
            if (option->short_option != 0) {
                m_help_log->f("-%c", option->short_option);

                if (this->NeedsArgument(option)) {
                    m_help_log->f(" %s", this->GetMetaName(option));
                }
            }

            if (option->short_option != 0 && !option->long_option.empty()) {
                m_help_log->f(", ");
            }

            if (!option->long_option.empty()) {
                m_help_log->f("--%s", option->long_option.c_str());

                if (this->NeedsArgument(option)) {
                    m_help_log->f("=%s", this->GetMetaName(option));
                }
            }

            if (!option->help.empty() || option->show_default) {
                if (neat) {
                    MoveToColumn(m_help_log, BRIEF_COLUMN_WIDTH);
                }

                LogIndenter option_indenter(m_help_log);

                if (!neat) {
                    option_indenter.PopIndent();
                    m_help_log->f(": ");
                }

                std::string msg = option->help;

                if (option->show_default) {
                    if (!msg.empty()) {
                        msg += " ";
                    }

                    if (option->int_ptr) {
                        msg += "(Default: " + std::to_string(*option->int_ptr) + ")";
                    } else if (option->str_ptr) {
                        msg += "(Default: ``" + *option->str_ptr + "'')";
                    }
                }

                if (!msg.empty()) {
                    PrintWrappedLines(m_help_log, msg);
                }
            }

            m_help_log->EnsureBOL();
        }
    }

    m_help_log->EnsureBOL();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<CommandLineParser::Option> CommandLineParser::AddOption(char short_option, std::string *long_option_ptr) {
    auto o = std::make_shared<Option>();

    o->short_option = short_option;

    if (long_option_ptr) {
        o->long_option = *long_option_ptr;
    }

    m_options.push_back(o);
    return o;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<CommandLineParser::Option> CommandLineParser::FindByLongOption(const std::string &long_option) const {
    for (auto &&option : m_options) {
        if (option->long_option == long_option) {
            return option;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<CommandLineParser::Option> CommandLineParser::FindByShortOption(char short_option) const {
    for (auto &&option : m_options) {
        if (option->short_option == short_option) {
            return option;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandLineParser::HandleOption(const std::shared_ptr<Option> &option) const {
    if (option->set_if_present_ptr) {
        *option->set_if_present_ptr = true;
    }

    if (option->reset_if_present_ptr) {
        *option->reset_if_present_ptr = false;
    }

    return this->NeedsArgument(option);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandLineParser::DoArgument(const std::shared_ptr<Option> &option,
                                   const std::string &arg) const {
    if (option->str_ptr) {
        *option->str_ptr = arg;
    }

    if (option->strv_ptr) {
        option->strv_ptr->push_back(arg);
    }

    if (option->int_ptr) {
        char *ep;
        long l = strtol(arg.c_str(), &ep, 0);
        if (*ep != 0) {
            m_error_log->f("invalid number: %s\n", arg.c_str());
            return false;
        }

        if (l < INT_MIN || l > INT_MAX) {
            m_error_log->f("number out of range: %ld\n", l);
            return false;
        }

        *option->int_ptr = (int)l;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandLineParser::NeedsArgument(const std::shared_ptr<Option> &option) const {
    return option->str_ptr || option->int_ptr || option->strv_ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *CommandLineParser::GetMetaName(const std::shared_ptr<Option> &option) const {
    if (option->meta.empty()) {
        return "ARG";
    } else {
        return option->meta.c_str();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
