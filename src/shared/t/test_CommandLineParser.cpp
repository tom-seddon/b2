#include <shared/system.h>
#include <shared/CommandLineParser.h>
#include <shared/testing.h>
#include <shared/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(OUT,"",&log_printer_stdout_and_debugger)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<const char *> GetArgv(const std::initializer_list<std::string> &list) {
    std::vector<const char *> argv;
    
    argv.push_back("ARG0");

    for(const std::string &str:list) {
        argv.push_back(str.c_str());
    }
    argv.push_back(nullptr);

    for(const char *arg:argv) {
        LOGF(OUT,"\"%s\" ",arg);
    }
    LOG(OUT).EnsureBOL();

    return argv;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool Test(const CommandLineParser &parser,std::initializer_list<std::string> list,std::vector<std::string> *other_args=nullptr) {
    std::vector<const char *> argv=GetArgv(list);

    return parser.Parse((int)argv.size()-1,argv.data(),other_args);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void TestBasic() {
    CommandLineParser p("Test program for shared/command_line library. Here are some more words in an attempt to test the word wrap. Four score and seven years ago our fathers brought forth on this continent a new nation, conceived in liberty, and dedicated  to the proposition that all men are created equal.","<args summary here>");

    int ai=100,bi=200,ci=300;
    std::string as="StringA",bs="StringB",cs="StringC";
    bool af=false,bf=false,cf=false;
    std::vector<std::string> a_list;
    
    p.AddOption('a',"a-int").Arg(&ai).Help("set integer A. Blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah blah").ShowDefault().Meta("A");
    p.AddOption('b',"b-int").Arg(&bi).Help("set integer B").ShowDefault().Meta("B");
    p.AddOption('c',"c-int").Arg(&ci).Help("set integer C").ShowDefault().Meta("C");
    p.AddOption('A',"a-string").Arg(&as).Help("set string A").ShowDefault();
    p.AddOption('B',"b-string").Arg(&bs).Help("set string A").ShowDefault();
    p.AddOption('C',"c-string").Arg(&cs).Help("set string A").ShowDefault();
    p.AddOption("a-list").AddArgToList(&a_list);
    p.AddOption("af").SetIfPresent(&af);
    p.AddOption("bf").SetIfPresent(&bf);
    p.AddOption("cf").SetIfPresent(&cf);

    // Unknown options.
    TEST_FALSE(Test(p,{"-u"}));
    TEST_FALSE(Test(p,{"--unknown-option"}));

    // Missing arguments.
    TEST_FALSE(Test(p,{"-ab"}));
    TEST_FALSE(Test(p,{"--a-int"}));
    TEST_FALSE(Test(p,{"-a"}));
    TEST_FALSE(Test(p,{"-A"}));

    // Flag set
    TEST_FALSE(af);
    TEST_TRUE(Test(p,{"--af"}));
    TEST_TRUE(af);

    // Int arg
    TEST_EQ_II(ai,100);
    TEST_TRUE(Test(p,{"-a","101"}));
    TEST_EQ_II(ai,101);
    TEST_TRUE(Test(p,{"--a-int=102"}));
    TEST_EQ_II(ai,102);
    TEST_FALSE(Test(p,{"-a","fred"}));
    TEST_FALSE(Test(p,{"--a-int=fred"}));

    // String arg
    TEST_EQ_SS(as,"StringA");
    TEST_TRUE(Test(p,{"-A","fredfred"}));
    TEST_EQ_SS(as,"fredfred");
    TEST_TRUE(Test(p,{"--a-string=fredfredfred"}));
    TEST_EQ_SS(as,"fredfredfred");

    // Add string to list
    TEST_TRUE(a_list.empty());
    TEST_TRUE(Test(p,{"--a-list=fred","--a-list=fred2"}));
    TEST_EQ_UU(a_list.size(),2);
    TEST_EQ_SS(a_list[0],"fred");
    TEST_EQ_SS(a_list[1],"fred2");

    //Other args
    TEST_FALSE(Test(p,{"-a","103","other-arg"}));
    {
        std::vector<std::string> others;
        TEST_TRUE(Test(p,{"-a","104","other1","other2"},&others));
        TEST_EQ_UU(others.size(),2);
        TEST_EQ_SS(others[0],"other1");
        TEST_EQ_SS(others[1],"other2");
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(void) {
    TestBasic();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
