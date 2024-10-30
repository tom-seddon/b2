cmake_minimum_required(VERSION 3.5)
project(visual_studio_test_runner)

STRING(REGEX REPLACE "\\\\" "/" DIR ${b2_BINARY_DIR})

##########################################################################
##########################################################################

# Run all tests.

add_executable(visual_studio_test_runner visual_studio_test_runner.cpp)

target_compile_definitions(visual_studio_test_runner
  PRIVATE -DCMAKE_COMMAND=\"${CMAKE_COMMAND}\"
  PRIVATE -DCONFIG=\"$<CONFIG>\"
  PRIVATE -DDIR=\"${DIR}\"
  )

target_link_libraries(visual_studio_test_runner PRIVATE shared_lib)

##########################################################################
##########################################################################

# Run non-slow tests.

add_executable(visual_studio_test_runner_subset visual_studio_test_runner.cpp)

target_compile_definitions(visual_studio_test_runner_subset
  PRIVATE -DCMAKE_COMMAND=\"${CMAKE_COMMAND}\"
  PRIVATE -DCONFIG=\"$<CONFIG>\"
  PRIVATE -DDIR=\"${DIR}\"
  PRIVATE -DSUBSET
  )

target_link_libraries(visual_studio_test_runner_subset PRIVATE shared_lib)
