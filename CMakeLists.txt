cmake_minimum_required(VERSION 3.5)
set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Final)
project(b2)
enable_testing()

##########################################################################
##########################################################################

# https://cmake.org/Wiki/CMake_FAQ#How_can_I_extend_the_build_modes_with_a_custom_made_one_.3F

set(CMAKE_CXX_FLAGS_FINAL ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
set(CMAKE_C_FLAGS_FINAL ${CMAKE_C_FLAGS_RELWITHDEBINFO})
set(CMAKE_EXE_LINKER_FLAGS_FINAL ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
set(CMAKE_SHARED_LINKER_FLAGS_FINAL ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})

##########################################################################
##########################################################################

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/etc/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/submodules/sanitizers-cmake/cmake")

##########################################################################
##########################################################################

if(WIN32)
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
  set(OSX 1)
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX 1)
else()
  message(FATAL_ERROR "unsupported platform:" ${CMAKE_HOST_SYSTEM_NAME})
endif()

##########################################################################
##########################################################################

set(WANTED_SANITIZE_THREAD ${SANITIZE_THREAD})
set(WANTED_SANITIZE_UNDEFINED ${SANITIZE_UNDEFINED})
set(WANTED_SANITIZE_MEMORY ${SANITIZE_MEMORY})
set(WANTED_SANITIZE_ADDRESS ${SANITIZE_ADDRESS})

find_package(Sanitizers)

function(check_sanitizer NAME PREFIX)
  if(WANTED_SANITIZE_${NAME})
    if("${${PREFIX}_${CMAKE_C_COMPILER_ID}_FLAGS}" STREQUAL "")
      message(FATAL_ERROR ${NAME} " sanitizer not available")
    endif()
  endif()
endfunction()

check_sanitizer(THREAD TSan)
check_sanitizer(UNDEFINED UBSan)
check_sanitizer(MEMORY MSan)
check_sanitizer(ADDRESS ASan)

##########################################################################
##########################################################################

if(OSX)
  # Adding options with add_definitions and CMAKE_EXE_LINKER_FLAGS
  # doesn't affect the SDL project. So use the
  # MAXOSX_DEPLOYMENT_TARGET environment variable instead (see
  # http://stackoverflow.com/questions/25352389/), which seems to have
  # the desired effect.
  set(ENV{MACOSX_DEPLOYMENT_TARGET} 10.7)
  
  add_definitions("-mmacosx-version-min=10.7 -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mmacosx-version-min=10.7 -stdlib=libc++")
endif()

##########################################################################
##########################################################################

# Use the SDL submodule, but set the variables up so it looks like it
# was found by FindSDL2. It doesn't look like the SDL2 cmake build
# process is designed for this, so this is a bit of a hack.

set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)

set(SDL__SOURCE_DIR ${CMAKE_SOURCE_DIR}/submodules/SDL)

if(NOT EXISTS ${SDL__SOURCE_DIR}/CMakeLists.txt)
  message(FATAL_ERROR "SDL not found in folder: " ${SDL__SOURCE_DIR})
endif()

add_subdirectory(${SDL__SOURCE_DIR})

set(SDL2_FOUND ON)
set(SDL2_LIBRARY SDL2-static SDL2main)
set(SDL2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/submodules/SDL/include)

# The build process generates an SDL_config.h file that goes in the
# binary folder, holding all the correct #defines for whatever. The
# idea seems to be that you then install this (rather than the source
# SDL_config.h) alongside the other headers in the install folder.
#
# You're then supposed to define USING_GENERATED_CONFIG_H, to ensure
# that the code uses the generated SDL_config.h rather than the
# original placeholder one in the source tree.
#
# But when using the source in-place, the generated SDL_config.h
# appears to be impossible to access, because all the SDL headers just
# do ``#include "SDL_config.h"''. So they get the original header.
#
# (This doesn't cause much of a problem of Windows on OS X, because
# the set of libraries is known and there aren't any significant
# options. But this means a couple of things go missing from the
# public API on Linux , because it doesn't have the
# SDL_VIDEO_DRIVER_xxx defines that the generated SDL_config.h
# includes.)
#
# To fix this copy all the relevant SDL include files into the binary
# folder, as a kind of fake install step, and use that as the include
# path. Use configure_file so that the source headers are noted as
# inputs to the build without having to find a project to attach a
# post-build step to.
#
# (cmake re-runs when these files change, but it doesn't necessarily
# regenerate the Visual Studio projects. So it takes a bit longer than
# it might, but there are at least no message boxes to click through.)

set(SDL__HEADERS_SRC ${SDL__SOURCE_DIR}/include)
set(SDL__HEADERS_DEST ${CMAKE_BINARY_DIR}/submodules/SDL/include)

file(GLOB SDL__HEADERS
  LIST_DIRECTORIES false
  RELATIVE ${SDL__HEADERS_SRC}
  ${SDL__HEADERS_SRC}/*.h)

foreach(SDL__HEADER ${SDL__HEADERS})
  if(NOT SDL__HEADER STREQUAL "SDL_config.h")
    configure_file(
      ${SDL__HEADERS_SRC}/${SDL__HEADER}
      ${SDL__HEADERS_DEST}/${SDL__HEADER}
      COPYONLY)
  endif()
endforeach()

set(SDL2_DEFINITIONS -DUSING_GENERATED_CONFIG_H)

##########################################################################
##########################################################################

if(LINUX)
  # set(FFmpeg_FIND_COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE PARENT_SCOPE)
  find_package(FFmpeg
    COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)

  if(AVCODEC_VERSION VERSION_LESS 57)
    set(HAVE_FFMPEG OFF)
  else()
    set(HAVE_FFMPEG ON)
  endif()
endif()

##########################################################################
##########################################################################

option(LIBUV_BUILD_TESTS "" ON)
set(LIBUVDIR ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libuv)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/etc/libuv-cmake)

# Don't add libuv's tests as a ctest test. The shutdown_eof test fails, for some reason.

# add_test(
#   NAME uv
#   COMMAND $<TARGET_FILE:run-tests>)
# set_tests_properties(uv PROPERTIES LABELS slow)

set(HAVE_LIBUV ON)

##########################################################################
##########################################################################

add_subdirectory(src)
if(INCLUDE_EXPERIMENTAL)
  add_subdirectory(experimental)
endif()
add_subdirectory(submodules)
