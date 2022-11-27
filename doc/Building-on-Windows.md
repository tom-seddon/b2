# Building on Windows

Prerequisites:

- Visual Studio 2019
- [cmake](https://cmake.org/) version 3.20+ on the PATH
  
Optional, but recommended:

- [Microsoft Child Process Debugging Power Tool](https://marketplace.visualstudio.com/items?itemName=GreggMiskelly.MicrosoftChildProcessDebuggingPowerTool)

Initial build steps:

1. Open command prompt in working copy folder 

2. Run `bin\snmake init_vs2019` to generate a solution for Visual
   Studio 2019.
   
   You can also generate solutions for other Visual Studio versions,
   as follows. These are supposed to work, but I only test them
   occasionally, and I'll remove them if they become too difficult to
   support.
   
   - Visual Studio 2017: `bin\snmake init_vs2017`
   - Visual Studio 2015: `bin\snmake init_vs2015`

   You should get a bunch of output - there may be the odd warning,
   but there should be no obvious errors, and it should finish with an
   exit code of 0

General day-to-day build steps:

1. Load appropriate solution into Visual Studio:

   - VS2019: `build\vs2019\b2.sln`
   - VS2017: `build\vs2017\b2.sln`
   - VS2015: `build\vs2015\b2.sln`

2. Build

3. Run

# Notes

- Because of the way cmake works, there are 40+ projects in the
  solution. Even though most are never used, they're still there
  cluttering the place up. You just have to put up with this

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
run them independently.
