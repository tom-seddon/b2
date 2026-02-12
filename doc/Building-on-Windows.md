# Building on Windows

Prerequisites:

- Visual Studio 2022 (ensure C++ CMake tools for Windows is included)
- Python 3.x (the version that comes with Visual Studio 2022 is fine)
  
Optional, but recommended:

- Microsoft Child Process Debugging Power Tool: [VS2022](https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool2022)

Initial setup, for use after cloning or updating the repo:

1. Open command prompt in working copy folder 

2. Run `make init_vs2022` to generate a solution for Visual Studio
   2022
   
   You should get a bunch of output - there may be the odd warning,
   but there should be no obvious errors, and it should finish with an
   exit code of 0

General day-to-day build steps:

1. Load solution into Visual Studio:

   - `build\vs2022\b2.sln` if using VS2022

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

To run from the command line, run the following (depending on Visual
Studio version), replacing `<<config>>` with the config of interest:
`Debug`, `RelWithDebInfo`, or `Final`.

- `make run_tests_vs2022 CONFIG=<<config>>` if using VS2022

This will run the tests in parallel according to PC core count.

(Running individual tests in the debugger is a DIY job. Consult
`CMakeLists.txt`. The command line setup for most tests is not onerous
and they will run from any working folder.)
