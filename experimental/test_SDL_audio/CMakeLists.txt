project(test_SDL_audio)

add_executable(test_SDL_audio test_SDL_audio.cpp)
b2_target_boilerplate(test_SDL_audio SANITIZERS)

target_link_libraries(test_SDL_audio PRIVATE ${SDL2_LIBRARY} shared_lib)
