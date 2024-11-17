##########################################################################
##########################################################################

function(progress_message message)
  string(TIMESTAMP time)
  message(STATUS "***************************************************************************")
  message(STATUS "**")
  message(STATUS "** ${message}")
  message(STATUS "** (Time now: ${time})")
  message(STATUS "**")
  message(STATUS "***************************************************************************")
endfunction()

progress_message("Starting...")

##########################################################################
##########################################################################

cmake_minimum_required(VERSION 3.20)
set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Final)
project(b2)
set(CMAKE_SKIP_INSTALL_RULES YES)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT b2)

##########################################################################
##########################################################################

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

# Normal variables take priority over OPTION.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
cmake_policy(SET CMP0077 NEW)

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
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")

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

progress_message("Sanitizers")

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

progress_message("libcurl")

if(USE_SYSTEM_CURL)

  if(OSX)
    # The objective here is to link against the system copy of
    # libcurl, not the MacPorts one.
    #
    # (Nor the Homebrew one! - but I don't use Homebrew, so I don't
    # know which folders to ignore.)
    set(OLD_CMAKE_IGNORE_PATH ${CMAKE_IGNORE_PATH})
    set(CMAKE_IGNORE_PATH
      /opt/local/lib
      /opt/local/include
      /opt/local/bin)
  endif()
  
  find_package(CURL 7 REQUIRED
    COMPONENTS HTTP)
  set(LIBCURL_TARGET CURL::libcurl)

  if(OSX)
    set(CMAKE_IGNORE_PATH ${OLD_CMAKE_IGNORE_PATH})
  endif()
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

message(STATUS "libcurl target: ${LIBCURL_TARGET}")


##########################################################################
##########################################################################

progress_message("SDL")

if(USE_SYSTEM_SDL)

  find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
  find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2main)

  # Find out whether the version is new enough for
  # SDL_SoftStretchLinear.
  get_target_property(CMAKE_REQUIRED_INCLUDES SDL2::SDL2 INCLUDE_DIRECTORIES)
  get_target_property(CMAKE_REQUIRED_DEFINITIONS SDL2::SDL2 COMPILE_DEFINITIONS)
  set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} -DSDL_MAIN_HANDLED)
  check_cxx_source_compiles("#include <SDL.h>
typedef decltype(&SDL_SoftStretchLinear) T;int main(void){return 0;}" HAVE_SDL_SOFTSTRETCHLINEAR)
  message(STATUS "SDL_SoftStretchLinear exists: ``${HAVE_SDL_SOFTSTRETCHLINEAR}''")
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_DEFINITIONS)

  set(SDL2_LIBRARY SDL2::SDL2 SDL2::SDL2main)

else()

  if(LINUX)
    # https://github.com/tom-seddon/b2/issues/36
    pkg_check_modules(B2_PULSEAUDIO libpulse-simple)
    if(NOT B2_PULSEAUDIO_FOUND)
      message(FATAL_ERROR "PulseAudio libraries not found - the appropriate package on Ubuntu is libpulse-dev")
    endif()
  endif()

  set(SDL_STATIC ON)
  set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
  add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/SDL_official)

  if(OSX)
    # Probably unavoidable, as SDL2 will be trying to support older
    # macOS versions.
    target_compile_options(SDL2-static PRIVATE "-Wno-deprecated-declarations")

    # I'm gambling these are benign.
    target_compile_options(SDL2-static PRIVATE "-Wno-shorten-64-to-32")
  endif()

  set(HAVE_SDL_SOFTSTRETCHLINEAR TRUE)

  set(SDL2_LIBRARY SDL2::SDL2-static SDL2::SDL2main)

endif()


##########################################################################
##########################################################################

if(LINUX OR OSX)
  progress_message("ffmpeg")

  find_package(PkgConfig)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG libavcodec libavformat libavutil libswresample libswscale)
    if(FFMPEG_FOUND EQUAL 1)
      message(STATUS "FFmpeg found:")

      message(STATUS "  FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")
      message(STATUS "  FFMPEG_LINK_LIBRARIES: ${FFMPEG_LINK_LIBRARIES}")
      message(STATUS "  FFMPEG_LIBRARY_DIRS: ${FFMPEG_LIBRARY_DIRS}")
      message(STATUS "  FFMPEG_LDFLAGS: ${FFMPEG_LDFLAGS}")
      message(STATUS "  FFMPEG_LDFLAGS_OTHER: ${FFMPEG_LDFLAGS_OTHER}")
      message(STATUS "  FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")
      message(STATUS "  FFMPEG_CFLAGS: ${FFMPEG_CFLAGS}")
      message(STATUS "  FFMPEG_CFLAGS_OTHER: ${FFMPEG_CFLAGS_OTHER}")
      message(STATUS "FFmpeg library versions:")
      message(STATUS "  libavcodec version: ${FFMPEG_libavcodec_VERSION}")
      message(STATUS "  libavformat version: ${FFMPEG_libavformat_VERSION}")
      message(STATUS "  libavutil version: ${FFMPEG_libavutil_VERSION}")
      message(STATUS "  libswresample version: ${FFMPEG_libswresample_VERSION}")
      message(STATUS "  libswscale version: ${FFMPEG_libswscale_VERSION}")
    else()
      message(WARNING "FFmpeg not found. Video writing will not be available")
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

progress_message("libuv")

if(USE_SYSTEM_LIBUV)
  find_package(LibUV REQUIRED)
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/submodules/libuv)

  if(OSX)
    # I'm trusting (possibly wrongly...) that these are benign.
    target_compile_options(uv PRIVATE "-Wno-shorten-64-to-32")
    target_compile_options(uv_a PRIVATE "-Wno-shorten-64-to-32")
  endif()

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

progress_message("submodules; src; dependencies")

add_subdirectory(submodules)
add_subdirectory(dependencies)
add_subdirectory(src)

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

##########################################################################
##########################################################################

progress_message("Done")
