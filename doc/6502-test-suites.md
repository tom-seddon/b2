# 6502 tests

These run as part of the full test set that you get if you just run
`ctest`.

## visual6502

Runs some timing tests against the C version of visual6502.

## lorenz ##

Some info: http://visual6502.org/wiki/index.php?title=6502TestPrograms

### rebuilding the 6502 code yourself (Windows) ###

Change to `etc/testsuite-2.15` and run `..\..\snmake`. This generates
bin files in `etc/testsuite-2.15/ascii-bin` and listing files in
`etc/testsuite-2.15/ascii-lst`.

## klaus ##

Original repo: https://github.com/Klaus2m5/6502_65C02_functional_tests

### rebuilding the 6502 code yourself (Windows) ###

Change to `etc/6502_65C02_functional_tests` and run `..\..\snmake`.
You get bin and listing files in `etc/6502_65C02_functional_tests`.
