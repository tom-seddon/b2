#ifndef HEADER_972977066E3F43DFB67CF75FB8F7AC67 // -*- mode:c++ -*-
#define HEADER_972977066E3F43DFB67CF75FB8F7AC67

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <memory>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Log;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandLineParser {
public:
    struct Option {
        char short_option=0;
        std::string long_option;
        std::string help;
        std::string meta;
        bool show_default=false;

        bool *set_if_present_ptr=nullptr;
        bool *reset_if_present_ptr=nullptr;
        std::string *str_ptr=nullptr;
        std::vector<std::string> *strv_ptr=nullptr;
        int *int_ptr=nullptr;

        Option &Help(std::string help);
        Option &Meta(std::string meta);
        Option &ShowDefault();

        Option &SetIfPresent(bool *present_ptr);
        Option &ResetIfPresent(bool *present_ptr);
        Option &Arg(std::string *str_ptr);
        Option &AddArgToList(std::vector<std::string> *strv_ptr);
        Option &Arg(int *int_ptr);
    };

    explicit CommandLineParser(std::string description="",std::string summary="");
    
    Option &AddOption(char short_option);
    Option &AddOption(char short_option,std::string long_option);
    Option &AddOption(std::string long_option);

    void AddHelpOption(bool *flag_ptr=nullptr);

    void SetLogs(Log *help_log,Log *error_log);

    bool Parse(int argc,const char *const argv[],std::vector<std::string> *other_args=nullptr) const;
    bool Parse(int argc,char *argv[],std::vector<std::string> *other_args=nullptr) const;
    void Help(const char *argv0) const;
private:
    std::string m_description;
    std::string m_summary;
    
    Log *m_help_log=nullptr;
    Log *m_error_log=nullptr;
    mutable bool m_help=false;
    bool *m_help_flag_ptr=nullptr;
    
    // shared_ptr<Option> isn't terribly clever, but it means that the
    // result of AddOption never becomes invalid.
    std::vector<std::shared_ptr<Option>> m_options;

    std::shared_ptr<Option> AddOption(char short_option,std::string *long_option_ptr);
    std::shared_ptr<Option> FindByLongOption(const std::string &long_option) const;
    std::shared_ptr<Option> FindByShortOption(char short_option) const;
    bool HandleOption(const std::shared_ptr<Option> &option) const;
    bool DoArgument(const std::shared_ptr<Option> &option,const std::string &arg) const;
    bool NeedsArgument(const std::shared_ptr<Option> &option) const;
    const char *GetMetaName(const std::shared_ptr<Option> &option) const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif//HEADER_972977066E3F43DFB67CF75FB8F7AC67
