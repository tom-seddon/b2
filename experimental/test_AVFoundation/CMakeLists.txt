cmake_minimum_required(VERSION 3.9)
project(test_AVFoundation)

add_executable(test_AVFoundation
  test_AVFoundation.mm)
target_link_libraries(test_AVFoundation PRIVATE shared_lib)
