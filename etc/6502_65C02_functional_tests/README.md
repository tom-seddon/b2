6502 test code
------

The project has this as a submodule, but the source files are not
batch-friendly. So here are additional copies, edited so they can be
built with a Makefile.

Edits:

- 6502_decimal_test.a65 - set all `chk` flags, comment out `cputype`
  setting
- 6502_functional_test.a65 - set `zero_page` to 0
- 65C02_extended_opcodes_test.a65c - set `zero_page` to 0, comment out
  `wdc_op` and `rkwl_wdc_op` settings
  
To build:

1. Download as65 from http://www.kingswood-consulting.co.uk/assemblers/

2. Unzip into a folder called `as65` in this folder

3. `make`
