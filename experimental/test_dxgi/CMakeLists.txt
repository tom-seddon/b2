project(test_dxgi)

add_executable(test_dxgi test_dxgi.cpp)
target_link_libraries(test_dxgi shared_lib dxgi.lib)
