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
  test_kevin_edwards.h test_kevin_edwards.cpp
  test_tube.h test_tube.cpp)
target_compile_definitions(test_common_lib PUBLIC
  -DROMS_FOLDER="${b2_SOURCE_DIR}/etc/roms"
  -DBBC_TESTS_OUTPUT_FOLDER="${CMAKE_BINARY_DIR}/b2_tests_output"
  )
target_boilerplate(test_common_lib)
target_link_libraries(test_common_lib PUBLIC shared_lib beeb_lib)
target_link_libraries(test_common_lib PRIVATE stb_image_lib)

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

function(add_standard_test bbc_drive bbc_test_name bbc_type)
  string(MAKE_C_IDENTIFIER ${bbc_test_name} bbc_test_name_c_identifier)
  set(target test_${bbc_test_name_c_identifier}_${bbc_type})
  add_executable(${target} test_standard.cpp)
  target_compile_definitions(${target} PRIVATE
    -DBEEBLINK_VOLUME_PATH="${b2_SOURCE_DIR}/etc/b2_tests"
    -DBEEBLINK_DRIVE="${bbc_drive}"
    -DBBC_TEST_NAME="${bbc_test_name}"
    -DBBC_TYPE=TestBBCMicroType_${bbc_type})
  test_target_boilerplate(${target})
  set_tests_properties(${target} PROPERTIES LABELS bbc)
endfunction()

add_standard_test(0 VTIMERS BTape)
add_standard_test(0 VIA.AC1 BTape)
add_standard_test(0 VIA.AC2 BTape)
add_standard_test(0 VIA.AC3 BTape)
add_standard_test(0 VIA.AC4 BTape)
add_standard_test(0 VIA.AC5 BTape)
add_standard_test(0 VIA.AC6 BTape)
add_standard_test(0 VIA.AC7 BTape)
add_standard_test(0 VIA.C1 BTape)
add_standard_test(0 VIA.C2 BTape)
add_standard_test(0 VIA.C3 BTape)

# VIA.C4 isn't Master-specific, but it uses ASL $xxxx,X, which behaves
# differently on CMOS CPUs, and the output in the repo comes from a
# Master 128.
add_standard_test(0 VIA.C4 Master128MOS320)

# (VIA.C5 appears to be B only, and I've only got my Master set up at
# time of writing)

# add_standard_test(0 VIA.C5 BTape)

add_standard_test(0 VIA.I1 BTape)
add_standard_test(0 VIA.I2 BTape)
add_standard_test(0 VIA.PB2 BTape)
add_standard_test(0 VIA.PB7 BTape)
add_standard_test(0 VIA.T11 BTape)
add_standard_test(0 VIA.T21 BTape)
add_standard_test(0 VIA.T22 BTape)
add_standard_test(0 VIA.PB6 BTape)

add_standard_test(0 TIMINGS BTape)
target_compile_definitions(test_TIMINGS_BTape PRIVATE
  -DSET_TRACE_FLAGS=BBCMicroTraceFlag_UserVIAExtra)

# should be the same on a B, but I haven't tested it.
add_standard_test(0 VTIMEOU Master128MOS320)
add_standard_test(0 VPOLL Master128MOS320)

##########################################################################
##########################################################################

function(add_kevin_edwards_test suffix)
  set(target test_kevin_edwards_${suffix})
  add_executable(${target} ${target}.cpp)
  test_target_boilerplate(${target})
  set_tests_properties(${target} PROPERTIES LABELS bbc)
  target_compile_definitions(${target} PRIVATE
    -DBEEBLINK_VOLUME_PATH="${b2_SOURCE_DIR}/etc/b2_tests")
  
  # worth having a tag for these, as they do take a while.
  set_tests_properties(${target} PROPERTIES LABELS kevin_edwards)
endfunction()

add_kevin_edwards_test(Alien8)
add_kevin_edwards_test(Jetman)
add_kevin_edwards_test(Nightsh)

# :(
# set_tests_properties(test_TIMINGS_BTape test_kevin_edwards_Nightsh test_kevin_edwards_Jetman
#   PROPERTIES
#   WILL_FAIL TRUE)

##########################################################################
##########################################################################

function(add_teletest_test suffix)
  set(target test_teletest_${suffix})
  add_executable(${target} ${target}.cpp)
  target_compile_definitions(${target} PRIVATE
    -DBEEBLINK_VOLUME_PATH="${b2_SOURCE_DIR}/etc/teletest_v1")
  test_target_boilerplate(${target})
  set_tests_properties(${target} PROPERTIES LABELS bbc)
endfunction()

add_teletest_test(engtest)
add_teletest_test(parrot)
add_teletest_test(teletst)

##########################################################################
##########################################################################

function(add_dp111_6502Timing_test stem bbc_type)
  set(target test_dp111_${stem})
  add_executable(${target} test_dp111_6502Timing.cpp)
  target_compile_definitions(${target} PRIVATE
    -DSSD_PATH="${b2_SOURCE_DIR}/submodules/6502Timing/${stem}.ssd"
    -DBBC_TYPE=TestBBCMicroType_${bbc_type})
  target_link_libraries(${target} PRIVATE shared_lib 6502_lib beeb_lib)
  test_target_boilerplate(${target})
endfunction()

add_dp111_6502Timing_test(6502timing BAcorn1770DFS)
add_dp111_6502Timing_test(6502timing1M BAcorn1770DFS)
add_dp111_6502Timing_test(65C12timing Master128MOS320)
add_dp111_6502Timing_test(65C12timing1M Master128MOS320)

##########################################################################
##########################################################################

function(tube_test_target_boilerplate target)
  target_compile_definitions(${target} PRIVATE
    -DBEEBLINK_VOLUME_PATH="${b2_SOURCE_DIR}/etc/b2_tests")
  target_link_libraries(${target} PRIVATE shared_lib 6502_lib beeb_lib)
  test_target_boilerplate(${target})
endfunction()

add_executable(test_tube_itu_prst test_tube_itu_prst.cpp)
tube_test_target_boilerplate(test_tube_itu_prst)

add_executable(test_tube_xtu_prst test_tube_xtu_prst.cpp)
tube_test_target_boilerplate(test_tube_xtu_prst)
