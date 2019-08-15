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

function(add_beeb_test stem)
  # TODO - test_common should be a static library.
  add_executable(${stem} ${stem}.cpp test_common.h test_common.cpp)
  add_config_define(${stem})

  target_compile_definitions(${stem} PRIVATE -DROMS_FOLDER="${b2_SOURCE_DIR}/etc/roms")

  target_link_libraries(${stem} PRIVATE shared_lib beeb_lib)
  add_test(
    NAME ${stem}
    COMMAND $<TARGET_FILE:${stem}>)
endfunction()

add_beeb_test(test_new_tests)
