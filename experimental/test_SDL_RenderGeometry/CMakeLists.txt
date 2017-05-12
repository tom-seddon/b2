cmake_minimum_required(VERSION 3.5)
project(test_SDL_RenderGeometry)

add_executable(test_SDL_RenderGeometry test_SDL_RenderGeometry.cpp)

target_include_directories(test_SDL_RenderGeometry PRIVATE ${SDL2_INCLUDE_DIR})
target_link_libraries(test_SDL_RenderGeometry PRIVATE ${SDL2_LIBRARY})
target_compile_definitions(test_SDL_RenderGeometry PRIVATE ${SDL2_DEFINITIONS})
