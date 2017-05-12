cmake_minimum_required(VERSION 3.5)
project(test_dear_imgui)

add_executable(test_dear_imgui 
  test_dear_imgui.cpp
  imgui_node_graph_test_github.cpp imgui_node_graph_test_github.h
  emoon_nodes.cpp emoon_nodes.h
  )

target_include_directories(test_dear_imgui PRIVATE ${SDL2_INCLUDE_DIR})
target_compile_definitions(test_dear_imgui PRIVATE ${SDL2_DEFINITIONS})
target_link_libraries(test_dear_imgui PRIVATE ${SDL2_LIBRARY} imgui_lib)
