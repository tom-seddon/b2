# Building on Windows

Prerequisites:

- Visual Studio 2015/2017/2019
- [cmake](https://cmake.org/) version 3.9+ on the PATH
  
Optional, but recommended:

- [Microsoft Child Process Debugging Power Tool](https://marketplace.visualstudio.com/items?itemName=GreggMiskelly.MicrosoftChildProcessDebuggingPowerTool)

Initial build steps:

1. Open command prompt in working copy folder 

2. Run `snmake`, according to Visual Studio version:

   - Visual Studio 2015: `snmake init_vs2015`
   - Visual Studio 2017: `snmake init_vs2017`
   - Visual Studio 2019: `snmake init_vs2019`

   You should get a bunch of output - there may be the odd warning,
   but there should be no obvious errors, and it should finish with an
   exit code of 0

General day-to-day build steps:

1. Load appropriate solution into Visual Studio:

   - VS2015, 64-bit: `build\win64.vs2015\b2.sln`
   - VS2017, 64-bit: `build\win64.vs2017\b2.sln`
   - VS2019, 64-bit: `build\win64.vs2019\b2.sln`
   - VS2015, 32-bit: `build\win32.vs2015\b2.sln`
   - VS2017, 32-bit: `build\win32.vs2017\b2.sln`
   - VS2019, 32-bit: `build\win32.vs2019\b2.sln`

2. Build

3. Run

# Notes

- Because of the way cmake works, there are 40+ projects in the
  solution. Even though most are never used, they're still there
  cluttering the place up. You just have to put up with this

- The 32-bit version is unsupported. It's included in the GitHub
  releases on the off-chance that somebody might find it useful, but I
  suspect nobody does, and I only try it occasionally myself. It's
  supposed to work, though...

- Building with VS2019 will produce a large number of warnings, mostly
  SAL-related.
  [These will probably get fixed eventually](https://github.com/tom-seddon/b2/issues/42),
  but for the moment a clean(ish) VS2019 build is not something I'm
  actively pursuing

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
