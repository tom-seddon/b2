cmake_minimum_required(VERSION 3.5)

##########################################################################
##########################################################################

add_executable(6502_basic_tests 6502_basic_tests.cpp)
target_link_libraries(6502_basic_tests PRIVATE shared_lib 6502_lib)
add_sanitizers(6502_basic_tests)

add_test(
  NAME 6502_basic_tests
  COMMAND $<TARGET_FILE:6502_basic_tests> ${CMAKE_CURRENT_LIST_DIR}/../../../etc/cmos_adc_sbc/)

##########################################################################
##########################################################################

add_executable(lorenz lorenz.cpp)
target_compile_definitions(lorenz PRIVATE -DLORENZ_FOLDER_NAME="${CMAKE_SOURCE_DIR}/etc/testsuite-2.15/ascii-bin/")
target_link_libraries(lorenz PRIVATE shared_lib 6502_lib)
add_sanitizers(lorenz)

add_test(
  NAME lorenz
  COMMAND $<TARGET_FILE:lorenz>)

# Don't run this as part of the build. It takes way too long.
set_tests_properties(lorenz PROPERTIES LABELS slow)

##########################################################################
##########################################################################

add_executable(klaus klaus.cpp)
target_compile_definitions(klaus PRIVATE -DKLAUS_FOLDER_NAME="${CMAKE_SOURCE_DIR}/etc/6502_65C02_functional_tests/")
target_link_libraries(klaus PRIVATE shared_lib 6502_lib)
add_sanitizers(klaus)

add_test(
  NAME klaus
  COMMAND $<TARGET_FILE:klaus>)

# Don't run this as part of the build. It takes way too long.
set_tests_properties(klaus PROPERTIES LABELS slow)

##########################################################################
##########################################################################

add_executable(visual6502 visual6502.cpp)
target_compile_definitions(visual6502 PRIVATE -DVISUAL6502_PATH="${b2_SOURCE_DIR}/submodules/visual6502")
target_link_libraries(visual6502 PRIVATE shared_lib 6502_lib perfect6502_lib)
add_sanitizers(visual6502)

add_test(
 NAME visual6502
 COMMAND $<TARGET_FILE:visual6502>)
set_tests_properties(visual6502 PROPERTIES LABELS slow)

##########################################################################
##########################################################################

add_executable(tom_seddon_6502_tests tom_seddon_6502_tests.cpp)
target_compile_definitions(tom_seddon_6502_tests PRIVATE -DSRC_PATH="${b2_SOURCE_DIR}/submodules/6502-tests/releases/6502-tests-generic.20221022T231937.dat")
target_link_libraries(tom_seddon_6502_tests PRIVATE shared_lib 6502_lib)
add_sanitizers(tom_seddon_6502_tests)

add_test(
  NAME tom_seddon_6502_tests
  COMMAND $<TARGET_FILE:tom_seddon_6502_tests>)
set_tests_properties(tom_seddon_6502_tests PROPERTIES LABELS slow)
