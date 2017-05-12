#include <shared/system.h>
#include <shared/testing.h>
#include <shared/path.h>
#include <stdlib.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main(int argc,char *argv[]) {
    (void)argc;

    std::string exe=PathGetEXEFileName();
    TEST_FALSE(exe.empty());
    
    printf("argv[0]: \"%s\"\n",argv[0]);
    printf("exe:     \"%s\"\n",exe.c_str());
    
    TEST_EQ_II(PathCompare(exe,argv[0]),0);

#if SYSTEM_WINDOWS
    TEST_EQ_II(PathCompare("C:\\fred","C:/FRED"),0);
    TEST_TRUE(PathCompare("C:\\fred","C:/FRED/FRED")<0);
    TEST_TRUE(PathCompare("C:/FRED/FRED","C:\\fred")>0);
#endif
    
    TEST_EQ_SS2(PathGetName("test.txt"),"test.txt");
    TEST_EQ_SS2(PathGetName("/test.txt"),"test.txt");
    TEST_EQ_SS2(PathGetName("a/test.txt"),"test.txt");
    TEST_EQ_SS2(PathGetName("/a/test.txt"),"test.txt");
    TEST_EQ_SS2(PathGetName("/a/b/c/d/test.txt"),"test.txt");

    TEST_EQ_SS2(PathWithoutExtension("fred"),"fred");
    TEST_EQ_SS2(PathWithoutExtension("fred.txt"),"fred");
    TEST_EQ_SS2(PathWithoutExtension("folder/fred.txt"),"folder/fred");
    TEST_EQ_SS2(PathWithoutExtension("folder.xyz/fred.txt"),"folder.xyz/fred");
    TEST_EQ_SS2(PathWithoutExtension("folder.xyz/fred"),"folder.xyz/fred");
    TEST_EQ_SS2(PathWithoutExtension("folder.xyz/"),"folder.xyz/");

    TEST_EQ_SS2(PathGetFolder("fred"),"");
    TEST_EQ_SS2(PathGetFolder("/fred"),"/");
    TEST_EQ_SS2(PathGetFolder("folder/fred"),"folder/");
    TEST_EQ_SS2(PathGetFolder("folder/"),"folder/");

    TEST_EQ_SS2(PathJoined("a","b"),"a/b");
    TEST_EQ_SS2(PathJoined("a/","b"),"a/b");
    TEST_EQ_SS2(PathJoined("a","/b"),"/b");
    TEST_EQ_SS2(PathJoined("a/","/b"),"/b");

    TEST_EQ_SS2(PathGetExtension("a.b"),".b");
    TEST_EQ_SS2(PathGetExtension("a.b/c.d"),".d");
    TEST_EQ_SS2(PathGetExtension("a.b/c.d.e"),".e");
    TEST_EQ_SS2(PathGetExtension("a.b/c"),"");
    TEST_EQ_SS2(PathGetExtension("a.b/"),"");
    TEST_EQ_SS2(PathGetExtension("a"),"");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
