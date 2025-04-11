cmake_minimum_required(VERSION 3.5)
project(test_locks)

add_executable(test_locks test_locks.cpp)
b2_target_boilerplate(test_locks SANITIZERS)
target_link_libraries(test_locks shared_lib)
