cmake_minimum_required(VERSION 3.5)

include("../common.cmake")

add_subdirectory(6502)
add_subdirectory(beeb)
add_subdirectory(shared)

add_subdirectory(dear_imgui_hex_editor)

if(MSVC)
  add_subdirectory(visual_studio_test_runner)
endif()

add_subdirectory(b2)
