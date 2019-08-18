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

add_library(test_common_lib test_common.h test_common.cpp)
target_compile_definitions(test_common_lib PUBLIC
  -DROMS_FOLDER="${b2_SOURCE_DIR}/etc/roms"
  -DBBC_TESTS_FOLDER="${b2_SOURCE_DIR}/etc/b2_tests/0"
  -DBBC_TESTS_OUTPUT_FOLDER="${CMAKE_BINARY_DIR}/b2_tests_output"
  )
target_boilerplate(test_common_lib)
target_link_libraries(test_common_lib PUBLIC shared_lib beeb_lib)

##########################################################################
##########################################################################

function(add_standard_test stem)
  # TODO - test_common should be a static library.
  add_executable(${stem} ${stem}.cpp)
  target_boilerplate(${stem})

  target_link_libraries(${stem} PRIVATE test_common_lib)
  add_test(NAME ${stem} COMMAND $<TARGET_FILE:${stem}>)
endfunction()

add_standard_test(test_new_tests)

