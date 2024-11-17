include("../common.cmake")

if(WIN32)
  add_subdirectory(test_dxgi)
  add_subdirectory(test_dxgi2)
  add_subdirectory(test_dxgi3)
  add_subdirectory(test_media_foundation)
endif()

if(FFMPEG_FOUND)
  add_subdirectory(test_ffmpeg)
endif()

if(OSX)
  add_subdirectory(test_AVFoundation)
endif()

add_subdirectory(test_SDL_keyboard)
add_subdirectory(test_locks)
