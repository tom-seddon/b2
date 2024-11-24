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

add_executable(test_beeb
  test_beeb.cpp test_beeb.inl)
target_compile_definitions(test_beeb PRIVATE
  -Db2_SOURCE_DIR="${b2_SOURCE_DIR}"
  -DROMS_FOLDER="${b2_SOURCE_DIR}/etc/roms"
  -DBBC_TESTS_OUTPUT_FOLDER="${CMAKE_BINARY_DIR}/b2_tests_output")
target_link_libraries(test_beeb PRIVATE shared_lib beeb_lib stb_image_lib)
target_boilerplate(test_beeb)

function(add_test_beeb_category category)
  add_test(NAME test_beeb_${category}
    COMMAND $<TARGET_FILE:test_beeb> -T ${category}\..*)
endfunction()

add_test_beeb_category(standard)
add_test_beeb_category(tube)
set_tests_properties(test_beeb_tube PROPERTIES LABELS slow)
add_test_beeb_category(kevin_edwards)
add_test_beeb_category(teletext)
add_test_beeb_category(dp111)
