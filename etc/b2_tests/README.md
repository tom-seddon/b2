Various tests for running on the BBC, Most are run as part of the
automated tests you get when running `ctest`.

This folder is a BeebLink volume. (For more about BeebLink, see
https://github.com/tom-seddon/beeblink.)

Licence: GPL v3

# Test notes

## Text-based tests

The intention is for test output to be produced by a * command that
somehow creates a file. When run on a real BBC from the BeebLink
volume, the test runs, and the output is saved, ready for a test to
find it. Then when the test is run in the emulator, the test system
watches OSCLI for that * command, stepping in to handle the comparison
of the emulated results with those previously saved from the real
system. No need for an emulated filing system.

* commands supported so far:

* `*SPOOL` - test output is whatever gets spooled

And more to follow, perhaps? Hand-coded special cases are also always
an option.

## Image-based tests

Each one produces a single image, then arranges for an OSWORD 0 call
(either using `INPUT`, or by returning to the BASIC prompt). The test
runner saves the image at this point and compares it to a known good
one.

The known good Images are initially created by running the test
program with `--infer-wanted-images` - if it doesn't find a particular
known good image, it assumes the saved image is correct, and uses it
to create the known good image in the right folder.

Once all the known good images are actually looking good (this bit has
to be done by eye...), they can be committed to the repo.

# Volume layout

## Drive 0

Mostly thanks to [Chris Evans](
https://stardot.org.uk/forums/memberlist.php?mode=viewprofile&u=11307).

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

### Producing SSDs

Run `make rel_tests` in the root of the working copy to produce SSD
files for this set of tests. The SSDs will be created in
`build/b2_tests`, one SSD per test.

## Drive 1

WIP CMOS stuff.

## Drive 2

Kevin Edwards protection tests.

## Drive 3

WIP 1770 stuff.

## Drive 4

Mostly by [David
Banks](https://www.stardot.org.uk/forums/memberlist.php?mode=viewprofile&u=9657).

Tube stuff.

## Drive 5

Image-based Video ULA/Video NuLA tests.

All can be run in a single-test version, as above, which produces one
image and then does an OSWORD 0. The tests is configured by setting
resident integer variables before running - instructions in each test.

Some also have an interactive mode, designed for use on a real Beeb,
where the program somehow cycles through all the possible outputs. I
use this when first putting the test together. To activate this
option, store a specific value in zero page using `!`, then run - then
press Return to see the next image. (See the code for the magic value
to use.)

