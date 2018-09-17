# Install script for directory: /home/benjamin/Documents/b2/submodules/SDL

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/libSDL2d.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/libSDL2maind.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets.cmake"
         "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/CMakeFiles/Export/lib/cmake/SDL2/SDL2Targets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2/SDL2Targets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2" TYPE FILE FILES "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/CMakeFiles/Export/lib/cmake/SDL2/SDL2Targets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2" TYPE FILE FILES "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/CMakeFiles/Export/lib/cmake/SDL2/SDL2Targets-debug.cmake")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xDevelx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDL2" TYPE FILE FILES
    "/home/benjamin/Documents/b2/submodules/SDL/SDL2Config.cmake"
    "/home/benjamin/Documents/b2/cmake-build-debug/SDL2ConfigVersion.cmake"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SDL2" TYPE FILE FILES
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_assert.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_atomic.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_audio.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_bits.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_blendmode.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_clipboard.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_android.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_iphoneos.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_macosx.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_minimal.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_pandora.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_psp.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_windows.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_winrt.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_config_wiz.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_copying.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_cpuinfo.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_egl.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_endian.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_error.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_events.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_filesystem.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_gamecontroller.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_gesture.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_haptic.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_hints.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_joystick.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_keyboard.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_keycode.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_loadso.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_log.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_main.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_messagebox.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_mouse.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_mutex.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_name.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengl.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengl_glext.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengles.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengles2.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengles2_gl2.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengles2_gl2ext.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengles2_gl2platform.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_opengles2_khrplatform.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_pixels.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_platform.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_power.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_quit.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_rect.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_render.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_revision.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_rwops.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_scancode.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_shape.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_stdinc.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_surface.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_system.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_syswm.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_assert.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_common.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_compare.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_crc32.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_font.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_fuzzer.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_harness.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_images.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_log.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_md5.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_memory.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_test_random.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_thread.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_timer.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_touch.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_types.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_version.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_vertex.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_video.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/SDL_vulkan.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/begin_code.h"
    "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/include/close_code.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/sdl2.pc")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "/home/benjamin/Documents/b2/cmake-build-debug/submodules/SDL/sdl2-config")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/local/share/aclocal/sdl2.m4")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/usr/local/share/aclocal" TYPE FILE FILES "/home/benjamin/Documents/b2/submodules/SDL/sdl2.m4")
endif()

