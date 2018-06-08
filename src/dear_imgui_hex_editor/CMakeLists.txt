cmake_minimum_required(VERSION 3.5)
project(dear_imgui_hex_editor)

add_library(dear_imgui_hex_editor_lib STATIC
	dear_imgui_hex_editor.cpp dear_imgui_hex_editor.h
	)

target_include_directories(dear_imgui_hex_editor_lib PUBLIC .)
target_link_libraries(dear_imgui_hex_editor_lib imgui_lib)
add_sanitizers(dear_imgui_hex_editor_lib)
