cmake_minimum_required(VERSION 3.5)
project(test_SDL_keyboard)

add_executable(test_SDL_keyboard test_SDL_keyboard.cpp)
add_sanitizers(test_SDL_keyboard)

target_link_libraries(test_SDL_keyboard PRIVATE ${SDL2_LIBRARY} shared_lib)
