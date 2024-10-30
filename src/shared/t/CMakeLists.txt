cmake_minimum_required(VERSION 3.5)

##########################################################################
##########################################################################

function(add_shared_test stem)
  set(options DONT_RUN CPP)
  cmake_parse_arguments(TEST "${options}" "" "ARGS" ${ARGN})
  add_executable(${stem} ${stem}.cpp)
  add_sanitizers(${stem})
  target_link_libraries(${stem} PRIVATE shared_lib)
  if(NOT ${TEST_DONT_RUN})
    add_test(
      NAME ${stem}
      COMMAND $<TARGET_FILE:${stem}> ${TEST_ARGS})
  endif()
endfunction()

##########################################################################
##########################################################################

add_shared_test(test_assert DONT_RUN)
add_shared_test(test_backtrace DONT_RUN)

##########################################################################
##########################################################################

add_shared_test(test_basic)
add_shared_test(test_CommandLineParser)
add_shared_test(test_log)
add_shared_test(test_sha1)
add_shared_test(test_enum)
add_shared_test(test_load_store)

##########################################################################
##########################################################################

if(APPLE OR UNIX)
  add_shared_test(test_posix_sleep)
endif()

##########################################################################
##########################################################################

add_executable(test_path test_path.cpp)
add_sanitizers(test_path)
target_link_libraries(test_path PRIVATE shared_lib)
add_test(
  NAME test_path
  COMMAND $<TARGET_FILE:test_path> $<TARGET_FILE:test_path> ${CMAKE_CURRENT_SOURCE_DIR})
