cmake_minimum_required(VERSION 3.0)

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
target_link_libraries(lorenz PRIVATE shared_lib 6502_lib)
add_sanitizers(lorenz)

set(LORENZ_ARGS ${CMAKE_SOURCE_DIR}/etc/testsuite-2.15/ascii-bin/)

add_custom_command(TARGET lorenz POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "lorenz command line:"
  COMMAND ${CMAKE_COMMAND} -E echo '$<TARGET_FILE:lorenz>' ${LORENZ_ARGS}
  VERBATIM)

# Don't run this as part of the build. It takes way too long.

# doesn't seem to be a way to get a path to the root CMakeLists.txt...
add_test(
  NAME lorenz
  COMMAND $<TARGET_FILE:lorenz> ${LORENZ_ARGS})
set_tests_properties(lorenz PROPERTIES LABELS slow)

##########################################################################
##########################################################################

add_executable(klaus klaus.cpp)
target_link_libraries(klaus PRIVATE shared_lib 6502_lib)
add_sanitizers(klaus)

set(KLAUS_ARGS ${CMAKE_SOURCE_DIR}/etc/6502_65C02_functional_tests/)

add_custom_command(TARGET klaus POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "klaus command line:"
  COMMAND ${CMAKE_COMMAND} -E echo '$<TARGET_FILE:klaus>' ${KLAUS_ARGS}
  VERBATIM)

add_test(
  NAME klaus
  COMMAND $<TARGET_FILE:klaus> ${KLAUS_ARGS})
set_tests_properties(klaus PROPERTIES LABELS slow)

##########################################################################
##########################################################################

add_executable(visual6502 visual6502.cpp perfect6502.h perfect6502.c nodenames.h segdefs.h transdefs.h)
target_link_libraries(visual6502 PRIVATE shared_lib 6502_lib)
add_sanitizers(visual6502)

add_test(
 NAME visual6502
 COMMAND $<TARGET_FILE:visual6502>)
set_tests_properties(visual6502 PROPERTIES LABELS slow)

##########################################################################
##########################################################################
