cmake_minimum_required(VERSION 3.5)

##########################################################################
##########################################################################

add_executable(test_6522 test_6522.cpp)
add_config_define(test_6522)
add_sanitizers(test_6522)
target_link_libraries(test_6522 PRIVATE shared_lib 6502_lib beeb_lib)
add_test(
  NAME test_6522
  COMMAND $<TARGET_FILE:test_6522>)

##########################################################################
##########################################################################

add_executable(test_OutputDataBuffer test_OutputDataBuffer.cpp)
add_config_define(test_OutputDataBuffer)
add_sanitizers(test_OutputDataBuffer)
target_link_libraries(test_OutputDataBuffer PRIVATE shared_lib beeb_lib)
add_test(
  NAME test_OutputDataBuffer
  COMMAND $<TARGET_FILE:test_OutputDataBuffer>)

##########################################################################
##########################################################################

if(MSVC)
  add_executable(test_relacy_OutputDataBuffer test_relacy_OutputDataBuffer.cpp)
  add_config_define(test_relacy_OutputDataBuffer)
  target_link_libraries(test_relacy_OutputDataBuffer PRIVATE shared_lib beeb_lib relacy_lib)
  add_test(
    NAME test_relacy_OutputDataBuffer
    COMMAND $<TARGET_FILE:test_relacy_OutputDataBuffer>)
endif()

##########################################################################
##########################################################################

add_library(test_common_lib
  test_common.h test_common.cpp test_common.inl
  test_kevin_edwards.h test_kevin_edwards.cpp)
target_compile_definitions(test_common_lib PUBLIC
  -DROMS_FOLDER="${b2_SOURCE_DIR}/etc/roms"
  -DBBC_TESTS_FOLDER="${b2_SOURCE_DIR}/etc/b2_tests"
  -DBBC_TESTS_OUTPUT_FOLDER="${CMAKE_BINARY_DIR}/b2_tests_output"
  )
target_boilerplate(test_common_lib)
target_link_libraries(test_common_lib PUBLIC shared_lib beeb_lib)

##########################################################################
##########################################################################

function(test_target_boilerplate target)
  target_boilerplate(${target})
  target_link_libraries(${target} PRIVATE test_common_lib)
  add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
endfunction()

add_executable(test_new_tests test_new_tests.cpp)
test_target_boilerplate(test_new_tests)

##########################################################################
##########################################################################

function(add_standard_test bbc_test_name bbc_type)
  string(MAKE_C_IDENTIFIER ${bbc_test_name} bbc_test_name_c_identifier)
  set(target test_${bbc_test_name_c_identifier}_${bbc_type})
  add_executable(${target} test_standard.cpp)
  target_compile_definitions(${target} PRIVATE
    -DBBC_TEST_NAME="${bbc_test_name}"
    -DBBC_TYPE=TestBBCMicroType_${bbc_type})
  test_target_boilerplate(${target})
  set_tests_properties(${target} PROPERTIES LABELS bbc)
endfunction()

add_standard_test(VTIMERS BTape)
add_standard_test(VIA.AC1 BTape)
add_standard_test(VIA.AC2 BTape)
add_standard_test(VIA.AC3 BTape)
add_standard_test(VIA.AC4 BTape)
add_standard_test(VIA.AC5 BTape)
add_standard_test(VIA.AC6 BTape)
add_standard_test(VIA.AC7 BTape)
add_standard_test(VIA.C1 BTape)
add_standard_test(VIA.C2 BTape)
add_standard_test(VIA.C3 BTape)

# VIA.C4 isn't Master-specific, but it uses ASL $xxxx,X, which behaves
# differently on CMOS CPUs, and the output in the repo comes from a
# Master 128.
add_standard_test(VIA.C4 Master128MOS320)

# (VIA.C5 appears to be B only, and I've only got my Master set up at
# time of writing)

# add_standard_test(VIA.C5 BTape)

add_standard_test(VIA.I1 BTape)
add_standard_test(VIA.I2 BTape)
add_standard_test(VIA.PB2 BTape)
add_standard_test(VIA.PB7 BTape)
add_standard_test(VIA.T11 BTape)
add_standard_test(VIA.T21 BTape)
add_standard_test(VIA.T22 BTape)
add_standard_test(VIA.PB6 BTape)

add_standard_test(TIMINGS BTape)

##########################################################################
##########################################################################

function(add_kevin_edwards_test suffix)
  set(target test_kevin_edwards_${suffix})
  add_executable(${target} ${target}.cpp)
  test_target_boilerplate(${target})
  set_tests_properties(${target} PROPERTIES LABELS bbc)

  # worth having a tag for these, as they do take a while.
  set_tests_properties(${target} PROPERTIES LABELS kevin_edwards)
endfunction()

add_kevin_edwards_test(Alien8)
add_kevin_edwards_test(Jetman)
add_kevin_edwards_test(Nightsh)
