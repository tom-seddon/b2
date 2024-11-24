# Building on Windows

Prerequisites:

- Visual Studio 2019 (ensure C++ CMake tools for Windows is included)
- Python 3.x (the version that comes with Visual Studio 2019 is fine)
  
Optional, but recommended:

- [Microsoft Child Process Debugging Power Tool](https://marketplace.visualstudio.com/items?itemName=GreggMiskelly.MicrosoftChildProcessDebuggingPowerTool)

Initial setup, for use after cloning or updating the repo:

1. Open command prompt in working copy folder 

2. Run `make init_vs2019` to generate a solution for Visual Studio
   2019.
   
   You should get a bunch of output - there may be the odd warning,
   but there should be no obvious errors, and it should finish with an
   exit code of 0

General day-to-day build steps:

1. Load solution into Visual Studio: `build\vs2019\b2.sln`

2. Build

3. Run

(The day-to-day build steps may also work after updating the repo;
cmake is supposed to sort itself out. But it does cache some
information and the initial build steps ensure everything is rebuilt.)

# Notes

- Because of the way cmake works, there are 50+ projects in the
  solution. Even though most are rarely used, they're still there
  cluttering the place up. You just have to put up with this

# Running the automated tests

To run from the command line, run `make run_tests_vs2019
CONFIG=<<config>>`, supplying the config of interest (`Debug`,
`RelWithDebInfo` or `Final`). This will run the tests in parallel
according to PC core count.

To run in the debugger, set the Visual Studio startup project to be
`visual_studio_test_runner`, build, and run. This runs the full set of
tests, such as it is, one test at a time.

You can exclude the particularly slow tests with the
`visual_studio_test_runner_subset` project, which will finish a bit
more quickly.

If you have the child process debugging tool installed, the debugger
will attach to each test case as it is executed. This does add
overhead but makes debugging a lot easier.

Individual tests can be run by setting each one as the startup
project.
