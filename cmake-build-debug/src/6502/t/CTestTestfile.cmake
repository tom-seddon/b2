# CMake generated Testfile for 
# Source directory: /home/benjamin/Documents/b2/src/6502/t
# Build directory: /home/benjamin/Documents/b2/cmake-build-debug/src/6502/t
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(6502_basic_tests "/home/benjamin/Documents/b2/cmake-build-debug/src/6502/t/6502_basic_tests" "/home/benjamin/Documents/b2/src/6502/t/../../../etc/cmos_adc_sbc/")
add_test(lorenz "/home/benjamin/Documents/b2/cmake-build-debug/src/6502/t/lorenz" "/home/benjamin/Documents/b2/etc/testsuite-2.15/ascii-bin/")
set_tests_properties(lorenz PROPERTIES  LABELS "slow")
add_test(klaus "/home/benjamin/Documents/b2/cmake-build-debug/src/6502/t/klaus" "/home/benjamin/Documents/b2/etc/6502_65C02_functional_tests/")
set_tests_properties(klaus PROPERTIES  LABELS "slow")
add_test(visual6502 "/home/benjamin/Documents/b2/cmake-build-debug/src/6502/t/visual6502")
set_tests_properties(visual6502 PROPERTIES  LABELS "slow")
