Various tests for running on the BBC. Some are run as part of the
automated tests.

This folder is a BeebLink volume. For more about BeebLink, see
https://github.com/tom-seddon/beeblink.

# BBC files

The files are categorised by BBC dir:

* `$.*` - non-test files and supporting tools
* `T.*` - BASIC test programs. Use `CHAN` to run.
* `S.*` - source for supporting tools (output files are in the repo
  too)
* `B.*`, `M.*` ,`+.*`, with name part matching a file in the `T` dir -
  output from running that test on a model B, Master 128 or B+
  respectively

# Test creation note

The intention is for test output to be produced by a * command that
somehow creates a file. When run on a real BBC, the test runs and the
output is saved; when run on the emulator, the test system watches
OSCLI for that * command, stepping in to handle the comparison of the
emulated results with those previously saved from the real system. No
need for an emulated filing system.

To ensure the files have the correct names, wrappers are provided that
will figure out an appropriate file name when generating initial
output. The file name is taken from an initial `REM>` in the BASIC V
style (as supported on the 8-bit systems by
[The BASIC Editor](https://github.com/tom-seddon/basic_editor/blob/master/docs/doc.md#rem-zsave-and-zrun)),
with the first char replaced by `B`, `M` or `+` according to system.
The wrappers just assume the name has a dir already.

Wrappers are, so far:

* `*TSPOOL` - does a `*SPOOL <name>`, where `<name>` is taken from the
  `REM>` as above
* `*TSAVE <args>` - does a `*SAVE <name> <args>`, where `<name>` is
  taken from the `REM>` as above and `<args>` are a copy of the
  `*TSAVE` arguments

A test's options for producing results are then:

* `*TSPOOL` - test result is all OSWRCH output until the next `*SPOOL`
* `*TSAVE` - test result is whatever was saved

More options to follow, and hand-coded special cases are also always
an option.

# Test runner notes

* the * command used determines the test type, so it's all automatic

* the test runner knows the name stem to use, because it's called that
  way from the C++ code
  
* many tests will (hopefully?) run the same on B vs B+ vs Master, so
  the test runner will load whichever file is available if the
  specific one isn't present. This will probably want improving a bit
  
