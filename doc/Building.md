# Building

This repo has submodules, so you have to clone it to build it - the
source code archives in the releases page won't work. (This is a
GitHub bug and there doesn't appear to be any way around it.)

To clone the repo:

    git clone --recursive https://github.com/tom-seddon/b2.git
	
If you're reading this after already cloning it:

    git submodule init
	git submodule update

Regarding branches, `master` should always build, and shouldn't
contain anything outrageously half-baked - it's the branch used by the
CI servers to prepare releases.

`wip/*` is stuff that's being worked on.

Once you're all set up:

[Instructions for building on Windows](./Building-on-Windows.md).

[Instructions for building on macOS](./Building-on-OSX.md).

[Instructions for building on Linux/macOS](./Building-on-Unix.md).

# Submodule URLs

The submodules are referred to by https. Before cloning, you can use
`git config --global url.ssh://git@github.com/.insteadOf
https://github.com/` to have them cloned over SSH instead, if you have
a GitHub login.

# 6502 tests

Some info about the 3rd party 6502 tests that run as part of the full
test set.

## lorenz ##

Some info: http://visual6502.org/wiki/index.php?title=6502TestPrograms

### rebuilding the 6502 code yourself ###

Everything you need to do this on Windows is included in the repo; for
macOS and Linux, you'll need GNU make, and
[64tass](http://tass64.sourceforge.net/) on the path.

Change to `etc/testsuite-2.15` and run `..\..\snmake` (Windows) or
`make` (macOS/Linux). This generates bin files in
`etc/testsuite-2.15/ascii-bin` and listing files in
`etc/testsuite-2.15/ascii-lst`.

## klaus

Original repo: https://github.com/Klaus2m5/6502_65C02_functional_tests

(I don't remember why this isn't a submodule...)

### rebuilding the 6502 code yourself (Windows) ###

Change to `etc/6502_65C02_functional_tests` and run `..\..\snmake`.
You get bin and listing files in `etc/6502_65C02_functional_tests`.
