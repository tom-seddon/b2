cmake_minimum_required(VERSION 3.0)

##########################################################################
##########################################################################

function(add_shared_test stem)
  set(options DONT_RUN CPP)
  cmake_parse_arguments(TEST "${options}" "" "ARGS" ${ARGN})
  if(${TEST_CPP})
    add_executable(${stem} ${stem}.cpp)
  else()
    add_executable(${stem} ${stem}.c)
  endif()
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

add_shared_test(test_basic CPP)
add_shared_test(test_CommandLineParser CPP)
add_shared_test(test_log CPP)
add_shared_test(test_sha1 CPP)
add_shared_test(test_enum)
add_shared_test(test_load_store CPP)

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
  COMMAND $<TARGET_FILE:test_path> $<TARGET_FILE:test_path>)

