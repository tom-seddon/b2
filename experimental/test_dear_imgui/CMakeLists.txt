cmake_minimum_required(VERSION 3.5)
project(test_dear_imgui)

add_executable(test_dear_imgui 
  test_dear_imgui.cpp
  imgui_node_graph_test_github.cpp imgui_node_graph_test_github.h
  emoon_nodes.cpp emoon_nodes.h
  pushwarn_imgui_whatever.h popwarn.h
  test_imgui_memory_editor.cpp test_imgui_memory_editor.h
  )
add_sanitizers(test_dear_imgui)

target_include_directories(test_dear_imgui PRIVATE ${SDL2_INCLUDE_DIR})
target_compile_definitions(test_dear_imgui PRIVATE ${SDL2_DEFINITIONS})
target_link_libraries(test_dear_imgui PRIVATE ${SDL2_LIBRARY} imgui_lib imgui_memory_editor_lib dear_imgui_hex_editor_lib)

find_package(OpenGL REQUIRED)
target_link_libraries(test_dear_imgui PRIVATE ${OPENGL_LIBRARIES})
