project(test_SDL_keyboard)

add_executable(test_SDL_keyboard test_SDL_keyboard.cpp)
b2_target_boilerplate(test_SDL_keyboard SANITIZERS)

target_link_libraries(test_SDL_keyboard PRIVATE ${SDL2_LIBRARY} shared_lib)
