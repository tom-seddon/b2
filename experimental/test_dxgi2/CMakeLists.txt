project(test_dxgi2)

add_executable(test_dxgi2 test_dxgi2.cpp)
target_link_libraries(test_dxgi2 shared_lib dxgi.lib)
