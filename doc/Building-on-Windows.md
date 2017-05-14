# Building on Windows

Prerequisites:

- Visual Studio 2015
- [cmake](https://cmake.org/) version 3.5+ on the PATH (I used
  3.7.0-rc2)
- [Python 2.x](https://www.python.org/download/releases/2.7/) on the
  PATH
  
Optional, but recommended:

- [Microsoft Child Process Debugging Power Tool](https://marketplace.visualstudio.com/items?itemName=GreggMiskelly.MicrosoftChildProcessDebuggingPowerTool)

Initial build steps:

1. Open command prompt in working copy folder 

2. Run `snmake init` (`snmake.exe` is supplied) - you should get a
   bunch of output. There may be the odd warning but there should be
   no obvious errors and it should finish with an exit code of 0
   
3. Load `0.win64\b2.sln` into Visual Studio 2015

4. Set `b2` as the startup project

General day-to-day build steps:

1. Build

2. Run

Because of the way cmake works, there are 40+ projects in the
solution. Even though most are never used, they're still there
cluttering the place up. You just have to put up with this.

# Running the automated tests

Set the startup project to be `visual_studio_test_runner`, build, and
run. This runs the full set of tests, such as it is, and takes about 4
minutes on my laptop.

You can exclude the slow tests with the
`visual_studio_test_runner_subset` project, which finishes in about 5
seconds.

If you have the child process debugging tool installed, the debugger
will attach to each test case as it is executed. This does add
overhead but makes debugging a lot easier.

Each test case has its own project in the solution, so you can also
run them independently. Some do require command line options - consult
the corresponding CMakeLists.txt for details.
