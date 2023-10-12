cmake_minimum_required(VERSION 3.20)
set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Final)
project(b2)
set(CMAKE_SKIP_INSTALL_RULES YES)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT b2)
enable_testing()
include(FindPkgConfig)
include(CheckCXXSourceCompiles)

##########################################################################
##########################################################################

# Prefer newer GL libraries.
# 
# https://cmake.org/cmake/help/latest/policy/CMP0072.html
# cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE GLVND)

##########################################################################
##########################################################################

if(DEFINED RELEASE_NAME)
  set(INCLUDE_EXPERIMENTAL OFF)
  option(LIBUV_BUILD_TESTS "" OFF)
else()
  set(INCLUDE_EXPERIMENTAL ON)
  option(LIBUV_BUILD_TESTS "" ON)
endif()

##########################################################################
##########################################################################

# https://cmake.org/Wiki/CMake_FAQ#How_can_I_extend_the_build_modes_with_a_custom_made_one_.3F

set(CMAKE_CXX_FLAGS_FINAL ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
set(CMAKE_C_FLAGS_FINAL ${CMAKE_C_FLAGS_RELWITHDEBINFO})
set(CMAKE_EXE_LINKER_FLAGS_FINAL ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
set(CMAKE_SHARED_LINKER_FLAGS_FINAL ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})

# Use statically linked CRT, to reduce the chances of needing to
# install the VC++ runtime redist. Adds <300 KB to b2.exe.
#
# This only needs to apply to b2.exe, but it's a pain to set this
# seting up on a case by case basis with VC++, and easiest to just set
# it for everything. This does noticeably increase the total size of
# the build byproducts though :( - 3.07 GB when building absolutely
# everything with DLL CRT, and 5.50 GB when building absolutely
# everything with .lib CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

##########################################################################
##########################################################################

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/etc/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/submodules/sanitizers-cmake/cmake")

##########################################################################
##########################################################################

if(WIN32)
  set(USE_SYSTEM_SDL_DEFAULT OFF)
  set(USE_SYSTEM_LIBUV_DEFAULT OFF)
  set(USE_SYSTEM_CURL_DEFAULT OFF)
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
  set(OSX 1)

  set(USE_SYSTEM_SDL_DEFAULT OFF)
  set(USE_SYSTEM_LIBUV_DEFAULT OFF)

  # The OS X version works fine when built with the repo's copy of
  # libcurl, and the result is easily stepped through in Xcode - but the
  # libcurl CMakeLists.txt takes, like, 15 minutes to run. Might as well
  # use the system libcurl, given that it ships with OS X and is
  # probably tuned for the system SSL and so on.
  set(USE_SYSTEM_CURL_DEFAULT ON)
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX 1)

  set(USE_SYSTEM_SDL_DEFAULT ON)
  set(USE_SYSTEM_LIBUV_DEFAULT ON)
  set(USE_SYSTEM_CURL_DEFAULT ON)
else()
  message(FATAL_ERROR "unsupported platform:" ${CMAKE_HOST_SYSTEM_NAME})
endif()

option(USE_SYSTEM_SDL "Use system copy of SDL rather than building the submodule copy from source" ${USE_SYSTEM_SDL_DEFAULT})
option(USE_SYSTEM_LIBUV "Use system copy of libuv rather than building the submodule copy from source" ${USE_SYSTEM_LIBUV_DEFAULT})
option(USE_SYSTEM_CURL "Use system copy of curl rather than building the submodule copy from source" ${USE_SYSTEM_CURL_DEFAULT})

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

if(USE_SYSTEM_CURL)
  find_package(CURL 7 REQUIRED
    COMPONENTS HTTP)
  set(LIBCURL_TARGET CURL::libcurl)
else()
  # (Even though Windows libcurl is part of submodules, the
  # configuration process sets some variables, and find_package (as used
  # on OS X/Linux) doesn't seem to have any analogue of PARENT_SCOPE. So
  # it's massively easier to do all this stuff in the top-level
  # CMakeLists.txt.)

  option(BUILD_CURL_EXE "" OFF)
  option(BUILD_SHARED_LIBS "" OFF)
  option(BUILD_TESTING "" OFF)

  # libcurl finds zlib using find_package, which is annoying:
  #
  # 1. it'll just find some random copy of zlib, if you have one (on
  # my laptop it found Anaconda's copy)
  #
  # 2. it can find some useless copy of zlib (on my laptop, it finds
  # Anaconda's copy when building for x86, even though it's an x64
  # library, and the link fails)
  #
  # 3. the obvious solution to this sort of problem is to build zlib
  # as part of the b2 project, then arrange for libcurl's find_package
  # to find that copy. But this appears to be literally impossible
  #
  # So (pending a better solution), just disable curl's use of zlib.
  #
  # (Curl is currently only used for BeebLink, so it's a minority
  # thing at best, and if it turns out the BeebLink server is emitting
  # gzip'd data that can just be disabled. It's pointless for Beeb
  # stuff anyway!)
  option(CURL_ZLIB "" OFF)

  add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/curl)
  set(LIBCURL_TARGET libcurl)
endif()

message(STATUS "libcurl found: ${HAVE_LIBCURL}")


##########################################################################
##########################################################################

if(USE_SYSTEM_SDL)

  find_package(SDL2 REQUIRED)

else()

  # (Even though SDL is part of submodules, the configuration process
  # sets some variables, so it's massively easier to do it in the
  # top-level CMakeLists.txt.)
  # 
  # Use the SDL submodule, but set the variables up so it looks like it
  # was found by FindSDL2. It doesn't look like the SDL2 cmake build
  # process is designed for this, so this is a bit of a hack.

  # can't remember what this is for...
  option(LIBC "" ON)

  set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)

  set(SDL__NAME SDL_official)
  set(SDL__SOURCE_DIR ${CMAKE_SOURCE_DIR}/submodules/${SDL__NAME})

  if(NOT EXISTS ${SDL__SOURCE_DIR}/CMakeLists.txt)
    message(FATAL_ERROR "SDL not found in folder: " ${SDL__SOURCE_DIR})
  endif()

  if(LINUX)
    # https://github.com/tom-seddon/b2/issues/36
    pkg_check_modules(B2_PULSEAUDIO libpulse-simple)
    if(NOT B2_PULSEAUDIO_FOUND)
      message(FATAL_ERROR "PulseAudio libraries not found - the appropriate package on Ubuntu is libpulse-dev")
    endif()
  endif()

  add_subdirectory(${SDL__SOURCE_DIR})

  set(SDL2_FOUND ON)
  set(SDL2_LIBRARY SDL2-static SDL2main)
  set(SDL2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/submodules/${SDL__NAME}/include)

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
  # (This doesn't cause much of a problem of Windows or OS X, because
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
  set(SDL__HEADERS_DEST ${CMAKE_BINARY_DIR}/submodules/${SDL__NAME}/include)

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

endif()

set(CMAKE_REQUIRED_INCLUDES ${SDL2_INCLUDE_DIR})
set(CMAKE_REQUIRED_DEFINITIONS ${SDL2_DEFINITIONS} -DSDL_MAIN_HANDLED)
# check_cxx_source_compiles has to compile to something linkable, but
# linking with SDL might not be possible at this point as it isn't
# necessarily actually compiled yet.
check_cxx_source_compiles("#include <SDL.h>
typedef decltype(&SDL_SoftStretchLinear) T;int main(void){return 0;}" HAVE_SDL_SOFTSTRETCHLINEAR)
message(STATUS "SDL_SoftStretchLinear exists: ``${HAVE_SDL_SOFTSTRETCHLINEAR}''")
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_DEFINITIONS)

##########################################################################
##########################################################################

if(LINUX OR OSX)
  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG libavcodec libavformat libavutil libswresample libswscale)
    if(NOT ${FFMPEG_FOUND})
      message(STATUS "FFmpeg not found - video writing not available")
    else()
      set(FFMPEG__GOOD ON)

      set(FFMPEG__AVCODEC_VERSION 58)
      set(FFMPEG__AVFORMAT_VERSION 58)
      set(FFMPEG__AVUTIL_VERSION 56)
      set(FFMPEG__SWRESAMPLE_VERSION 3)
      set(FFMPEG__SWSCALE_VERSION 5)

      # https://gitlab.kitware.com/cmake/cmake/-/issues/18067 - ridiculous
      if(NOT FFMPEG_libavcodec_VERSION MATCHES "^58\.")
	message(WARNING "Unsupported libavcodec major version - need 58")
	set(FFMPEG__GOOD OFF)
      endif()

      if(NOT FFMPEG_libavformat_VERSION MATCHES "^58\.")
	message(WARNING "Unsupported libavformat major version - need 58")
	set(FFMPEG__GOOD OFF)
      endif()

      if(NOT FFMPEG_libavutil_VERSION MATCHES "^56\.")
	message(WARNING "Unsupported libavutil major version - need 56")
	set(FFMPEG__GOOD OFF)
      endif()

      if(NOT FFMPEG_libswresample_VERSION MATCHES "^3\.")
	message(WARNING "Unsupported libswreasmple major version - need 3")
	set(FFMPEG__GOOD OFF)
      endif()
      
      if(NOT FFMPEG_libswscale_VERSION MATCHES "^5\.")
	message(WARNING "Unsupported libswscale major version - need 5")
	set(FFMPEG__GOOD OFF)
      endif()

      if(FFMPEG__GOOD)
	message(STATUS "FFmpeg 4 found")
	message(STATUS "  Libraries: ${FFMPEG_LIBRARIES}")
	message(STATUS "  Include dirs: ${FFMPEG_INCLUDE_DIRS}")
	message(STATUS "  Definitions: ${FFMPEG_DEFINITIONS}")
      else()
        message(WARNING "FFmpeg 4 required specifically - video writing not available")
	set(FFMPEG_FOUND OFF)
      endif()
    endif()
  endif()
endif()

##########################################################################
##########################################################################

if(LINUX)
  find_package(GTK2 2.0 REQUIRED gtk)
  message(STATUS "GTK2 include folder: " ${GTK2_INCLUDE_DIRS})
endif()

##########################################################################
##########################################################################

if(USE_SYSTEM_LIBUV)
  find_package(LibUV REQUIRED)
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/submodules/libuv)

  # Don't add libuv's tests as a ctest test. The shutdown_eof test fails, for some reason.

  # add_test(
  #   NAME uv
  #   COMMAND $<TARGET_FILE:run-tests>)
  # set_tests_properties(uv PROPERTIES LABELS slow)

  set(LibUV_FOUND ON)
  set(LibUV_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libuv)
  set(LibUV_LIBRARIES uv_a)
endif()

##########################################################################
##########################################################################

add_subdirectory(submodules)
add_subdirectory(src)
add_subdirectory(dependencies)

target_sources(b2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt ${CMAKE_CURRENT_LIST_DIR}/common.cmake)
target_sources(b2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/submodules/CMakeLists.txt)

if(INCLUDE_EXPERIMENTAL)
  add_subdirectory(experimental)
  target_sources(b2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/experimental/CMakeLists.txt)
endif()

##########################################################################
##########################################################################

# It's handy to have the non-project CMakeLists.txt files somewhere in the VS
# solution.
#
# There's nowhere particularly good for them, so just put them in b2.
#
# This doesn't interact brilliantly with DPack, which strips off the paths
# in its files list, but Ctrl+, works OK.

target_sources(b2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt ${CMAKE_CURRENT_LIST_DIR}/common.cmake)

target_sources(b2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/submodules/CMakeLists.txt)

if(INCLUDE_EXPERIMENTAL)
  target_sources(b2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/experimental/CMakeLists.txt)
endif()

if(OSX)
  target_sources(b2 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/b2/macos/template.Info.plist)
endif()
