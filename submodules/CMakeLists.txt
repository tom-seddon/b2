cmake_minimum_required(VERSION 3.5)

##########################################################################
##########################################################################

# add_subdirectory(curl)

# # Presumably you're *supposed* to use generator expressions for this
# # sort of thing, like $<TARGET_PROPERTY:SOURCE_DIR>. But that didn't
# # work when I tried it.
# target_include_directories(libcurl INTERFACE
#   ${CMAKE_CURRENT_SOURCE_DIR}/curl/include
#   ${CMAKE_CURRENT_BINARY_DIR}/curl/include/curl)

# set(HAVE_LIBCURL ON)

##########################################################################
##########################################################################

add_library(miniz_lib INTERFACE)
target_include_directories(miniz_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/miniz)

##########################################################################
##########################################################################

set(IMGUI_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/Flix01_imgui)

add_library(imgui_lib STATIC
  ${IMGUI_FOLDER}/imconfig.h
  ${IMGUI_FOLDER}/imgui.cpp
  ${IMGUI_FOLDER}/imgui_demo.cpp
  ${IMGUI_FOLDER}/imgui_draw.cpp
  ${IMGUI_FOLDER}/imgui.h
  ${IMGUI_FOLDER}/imgui_internal.h
  ${IMGUI_FOLDER}/stb_rect_pack.h
  ${IMGUI_FOLDER}/stb_textedit.h
  ${IMGUI_FOLDER}/stb_truetype.h
  )
target_include_directories(imgui_lib INTERFACE ${IMGUI_FOLDER})

##########################################################################
##########################################################################

add_library(remotery_lib STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/Remotery/lib/Remotery.c
	${CMAKE_CURRENT_SOURCE_DIR}/Remotery/lib/Remotery.h
	)
target_include_directories(remotery_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Remotery/lib)
target_compile_definitions(remotery_lib PUBLIC RMT_ENABLED=0 RMT_USE_D3D11=0 RMT_USE_OPENGL=0)

##########################################################################
##########################################################################

add_library(noc_lib INTERFACE)
target_include_directories(noc_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/noc)
if(UNIX)
  if(NOT OSX)
    find_package(GTK2 2.0 REQUIRED gtk)
    message(STATUS "GTK2 include folder: " ${GTK2_INCLUDE_DIRS})
    target_compile_definitions(noc_lib INTERFACE ${GTK2_DEFINITIONS})
    target_include_directories(noc_lib INTERFACE ${GTK2_INCLUDE_DIRS})
    target_link_libraries(noc_lib INTERFACE ${GTK2_LIBRARIES})
  endif()
endif()
	
##########################################################################
##########################################################################

add_library(rapidjson_lib INTERFACE)

target_include_directories(rapidjson_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/include)

##########################################################################
##########################################################################

add_library(IconFontCppHeaders_lib INTERFACE)

target_include_directories(IconFontCppHeaders_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/IconFontCppHeaders)

##########################################################################
##########################################################################

if(OSX)
  set(BUNDLER_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/macdylibbundler/src)

  add_executable(macdylibbundler
    ${BUNDLER_SRC_FOLDER}/Settings.cpp
    ${BUNDLER_SRC_FOLDER}/DylibBundler.cpp
    ${BUNDLER_SRC_FOLDER}/Dependency.cpp
    ${BUNDLER_SRC_FOLDER}/main.cpp
    ${BUNDLER_SRC_FOLDER}/Utils.cpp)
  
endif()

##########################################################################
##########################################################################

if(MSVC)
  set(RELACY ${CMAKE_CURRENT_SOURCE_DIR}/relacy)
  set(RELACY_TEST ${RELACY}/test)
  set(RELACY_EXAMPLE ${RELACY}/example)

  add_library(relacy_lib INTERFACE)
  target_include_directories(relacy_lib INTERFACE ${RELACY})

  add_executable(relacy_test
    ${RELACY_TEST}/addr_hash.hpp
    ${RELACY_TEST}/compare_swap.hpp
    ${RELACY_TEST}/condvar.hpp
    ${RELACY_TEST}/data_race.hpp
    ${RELACY_TEST}/dyn_thread.hpp
    ${RELACY_TEST}/event.hpp
    ${RELACY_TEST}/fence.hpp
    ${RELACY_TEST}/futex.hpp
    ${RELACY_TEST}/memory.hpp
    ${RELACY_TEST}/memory_order.hpp
    ${RELACY_TEST}/mutex.hpp
    ${RELACY_TEST}/pthread.hpp
    ${RELACY_TEST}/scheduler.hpp
    ${RELACY_TEST}/semaphore.hpp
    ${RELACY_TEST}/thread_local.hpp
    ${RELACY_TEST}/wfmo.hpp
    ${RELACY_TEST}/windows.hpp
    ${RELACY_TEST}/foo.cpp
    #  ${RELACY_TEST}/iriw.cpp
    ${RELACY_TEST}/main.cpp
    ${RELACY_TEST}/stdafx.cpp)
  # target_link_libraries(relacy_test relacy_lib)
  if(MSVC)
    target_compile_options(relacy_test PRIVATE -bigobj)
  endif()

  # failed to get any of these to build.

  # function(add_relacy_example STEM)
  #   add_executable(relacy_example_${STEM}
  #     ${RELACY_EXAMPLE}/${STEM}/stdafx.h
  #     ${RELACY_EXAMPLE}/${STEM}/stdafx.cpp
  #     ${RELACY_EXAMPLE}/${STEM}/${STEM}.cpp)
  # endfunction()

  # add_relacy_example(cli_ws_deque)
  # add_relacy_example(condvar)
  # add_relacy_example(eventcount)
  # add_relacy_example(java_ws_deque)
  # add_relacy_example(mutex_business_logic)
  # add_relacy_example(peterson)
  # add_relacy_example(proxy_collector)
  # add_relacy_example(ref_counting)
  # add_relacy_example(smr)
  # add_relacy_example(spsc_queue)
  # add_relacy_example(stack)
  # add_relacy_example(ws_deque)

  # add_executable(relacy_example_examples
  #   ${RELACY_EXAMPLE}/examples/amp_condvar.hpp
  #   ${RELACY_EXAMPLE}/examples/examples.cpp
  #   ${RELACY_EXAMPLE}/examples/spsc_overwrite_queue.hpp
  #   ${RELACY_EXAMPLE}/examples/stdafx.cpp
  #   ${RELACY_EXAMPLE}/examples/stdafx.h)

  # add_executable(relacy_example_mpmc
  #   ${RELACY_EXAMPLE}/mpmc/mpmc.cpp
  #   ${RELACY_EXAMPLE}/mpmc/pcx.h
  #   ${RELACY_EXAMPLE}/mpmc/stdafx.cpp
  #   ${RELACY_EXAMPLE}/mpmc/stdafx.h)

  # add_executable(relacy_example_tbb_eventcount
  #   ${RELACY_EXAMPLE}/tbb_eventcount/eventcount.cpp
  #   ${RELACY_EXAMPLE}/tbb_eventcount/stdafx.cpp
  #   ${RELACY_EXAMPLE}/tbb_eventcount/stdafx.h)
  
  # add_executable(relacy_example_ws_deque2
  #   ${RELACY_EXAMPLE}/ws_deque2/ws_deque.cpp
  #   ${RELACY_EXAMPLE}/ws_deque2/stdafx.cpp
  #   ${RELACY_EXAMPLE}/ws_deque2/stdafx.h)

else()

  # The supported Relacy platforms don't include anything recent, but
  # the Relacy tests do build and run on Linux and OS X. But I got a
  # few errors trying to build the OutputDataBuffer tests on Linux
  # that suggest it's not C++11-friendly. Presumably fixable? - but it
  # does feel like life's too short. There's already a few hacks to
  # make it hang together with VS2015, and I don't much care for
  # having to put together 2 more sets of the same again. If it works
  # on one platform, that's fine, especially if that platform is
  # Windows.
  #
  # May revisit later.
  
endif()

##########################################################################
##########################################################################
