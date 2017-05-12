cmake_minimum_required(VERSION 3.5)
project(test_dxgi3)

add_executable(test_dxgi3 test_dxgi3.cpp)
target_link_libraries(test_dxgi3 shared_lib dxgi.lib winmm.lib)
