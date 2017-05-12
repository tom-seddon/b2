# 6502 tests

These run as part of the full test set that you get if you just run
`ctest`.

## visual6502

Runs some timing tests against the C version of visual6502.

No command line required.

## lorenz ##

Some info: http://visual6502.org/wiki/index.php?title=6502TestPrograms

Requires command line arguments. If you want to run it solo, search
the full build output for the line `lorenz command line:`; the
following line shows you the full command line required to run it.
(For Visual Studio, you only need the arguments part.)

(Quickest way to find this is probably to do a `Project Only` >
`Rebuild Only lorenz` in Visual Studio. On OS X/Linux, do a full
rebuild.)

### rebuilding the 6502 code yourself (Windows) ###

Change to `etc/testsuite-2.15` and run `..\..\snmake`. This generates
bin files in `etc/testsuite-2.15/ascii-bin` and listing files in
`etc/testsuite-2.15/ascii-lst`.

## klaus ##

Original repo: https://github.com/Klaus2m5/6502_65C02_functional_tests

Requires command line arguments. Follow the same process as for
`lorenz`, but this time search for `klaus command line`.

### rebuilding the 6502 code yourself (Windows) ###

Change to `etc/6502_65C02_functional_tests` and run `..\..\snmake`.
You get bin and listing files in `etc/6502_65C02_functional_tests`.
