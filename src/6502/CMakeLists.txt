cmake_minimum_required(VERSION 3.5)

project(6502)

# add_custom_command(
#   OUTPUT 6502_internal.inl 6502_simplified.inl
#   COMMAND python ${CMAKE_CURRENT_LIST_DIR}/gen_6502.py -o 6502_internal.inl -s 6502_simplified.inl
#   DEPENDS gen_6502.py
#   MAIN_DEPENDENCY gen_6502.py)

add_executable(6502_gen
  6502_gen.cpp 6502_gen.inl)
target_link_libraries(6502_gen shared_lib)

add_library(6502_lib STATIC
  c/6502.c h/6502/6502.h
  ${CMAKE_CURRENT_BINARY_DIR}/6502_internal.inl
  )

add_dependencies(6502_lib 6502_gen)

add_custom_command(
  OUTPUT 6502_internal.inl
  COMMAND $<TARGET_FILE:6502_gen> -o 6502_internal.inl
  DEPENDS $<TARGET_FILE:6502_gen>)

target_include_directories(6502_lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(6502_lib PUBLIC h)
add_sanitizers(6502_lib)

add_subdirectory(t)
