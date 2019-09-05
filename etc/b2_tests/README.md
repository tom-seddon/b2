Various tests for running on the BBC, mostly thanks to
[scarybeasts](https://github.com/scarybeasts). Many are run as part of
the automated tests you get when running `ctest`.

This folder is a BeebLink volume. (For more about BeebLink, see
https://github.com/tom-seddon/beeblink.)

Licence: GPL v3

# Test notes

The intention is for test output to be produced by a * command that
somehow creates a file. When run on a real BBC from the BeebLink
volume, the test runs, and the output is saved, ready for a test to
find it. Then when the test is run in the emulator, the test system
watches OSCLI for that * command, stepping in to handle the comparison
of the emulated results with those previously saved from the real
system. No need for an emulated filing system.

* commands supported so far:

* `*SPOOL` - test output is whatever gets spooled

And in the future:

* `*SAVE` - test output is whatever was saved

And more to follow, perhaps? Hand-coded special cases are also always
an option.

# Volume layout

## Drive 0

The files are categorised by BBC dir:

* `$.*` - non-test files and supporting tools
* `T.*` - BASIC test programs. Use `CHAN` to run.
* `O.*`, name part matching a file in the `T` dir - output from
  running that test on a real BBC Micro.

As the folder is a BeebLink volume, each BBC file has a `.inf` file
containing its true name and the load/exec addresses - but the test
system is rather careless about this, and just assumes that the PC
names and BBC names match (taking the #xx escaping scheme into
account).

## Drive 1

WIP stuff.

Kevin Edwards protection tests.

# Test runner notes

* the test runner knows the name stem to use, because it's called that
  way from the C++ code

* the * command used determines the test output type, so it's all
  automatic

# Producing SSDs

Run `make rel_tests` in the root of the working copy. The SSDs will be
created in `build/b2_tests`, one SSD per test.
