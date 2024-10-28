cmake_minimum_required(VERSION 3.5)

##########################################################################
##########################################################################

add_library(miniz_lib INTERFACE)
target_include_directories(miniz_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/miniz)

##########################################################################
##########################################################################

set(IMGUI__FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

add_library(imgui_lib STATIC
  ${IMGUI__FOLDER}/imconfig.h
  ${IMGUI__FOLDER}/imgui.cpp
  ${IMGUI__FOLDER}/imgui.h
  ${IMGUI__FOLDER}/imgui_demo.cpp
  ${IMGUI__FOLDER}/imgui_draw.cpp
  ${IMGUI__FOLDER}/imgui_internal.h
  ${IMGUI__FOLDER}/imgui_tables.cpp
  ${IMGUI__FOLDER}/imgui_widgets.cpp
  ${IMGUI__FOLDER}/imstb_rectpack.h
  ${IMGUI__FOLDER}/imstb_textedit.h
  ${IMGUI__FOLDER}/imstb_truetype.h)

target_include_directories(imgui_lib PUBLIC ${IMGUI__FOLDER})
target_compile_definitions(imgui_lib PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_OBSOLETE_KEYIO)
target_compile_features(imgui_lib PUBLIC cxx_constexpr)

if(MSVC)
  target_compile_definitions(imgui_lib PRIVATE _CRT_SECURE_NO_WARNINGS)

  # C4305: 'identifier' : truncation from 'type1' to 'type2'
  # C4267: 'var' : conversion from 'size_t' to 'type', possible loss of data
  # C4800: 'type' : forcing value to bool 'true' or 'false' (performance warning)
  # C4244: 'conversion' conversion from 'type1' to 'type2', possible loss of data
  # C4312: 'operation' : conversion from 'type1' to 'type2' of greater size 
  target_compile_options(imgui_lib PRIVATE /wd4305 /wd4267 /wd4800 /wd4244 /wd4312 /wd4458)
endif()

if(WIN32)
  # Add the minimal-dependency standalone example, for emergency
  # testing purposes.
  add_executable(imgui_example_win32_directx11
    ${IMGUI__FOLDER}/examples/example_win32_directx11/main.cpp
    ${IMGUI__FOLDER}/backends/imgui_impl_dx11.cpp
    ${IMGUI__FOLDER}/backends/imgui_impl_win32.cpp)
  target_include_directories(imgui_example_win32_directx11 PRIVATE ${IMGUI__FOLDER}/backends)
  target_link_libraries(imgui_example_win32_directx11 PRIVATE d3d11.lib imgui_lib)
endif()

##########################################################################
##########################################################################

add_library(imgui_memory_editor_lib INTERFACE)
target_include_directories(imgui_memory_editor_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/imgui_club/imgui_memory_editor)

target_sources(imgui_memory_editor_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui_club/imgui_memory_editor/imgui_memory_editor.h)

##########################################################################
##########################################################################

add_library(remotery_lib STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/Remotery/lib/Remotery.c
	${CMAKE_CURRENT_SOURCE_DIR}/Remotery/lib/Remotery.h
	)
target_include_directories(remotery_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Remotery/lib)
if(WIN32)
  # got PIX on Windows.
  target_compile_definitions(remotery_lib PUBLIC RMT_ENABLED=0)
else()
  # exclude from Final build.
  target_compile_definitions(remotery_lib PUBLIC RMT_ENABLED=$<NOT:$<BOOL:$<CONFIG:Final>>>)
endif()
target_compile_definitions(remotery_lib PUBLIC RMT_USE_D3D11=0 RMT_USE_OPENGL=0)


##########################################################################
##########################################################################

add_library(rapidjson_lib INTERFACE)

set(RAPIDJSON_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/include)

target_sources(rapidjson_lib INTERFACE
  ${RAPIDJSON_INCLUDE}/rapidjson/fwd.h
  ${RAPIDJSON_INCLUDE}/rapidjson/memorystream.h
  ${RAPIDJSON_INCLUDE}/rapidjson/reader.h
  ${RAPIDJSON_INCLUDE}/rapidjson/prettywriter.h
  ${RAPIDJSON_INCLUDE}/rapidjson/ostreamwrapper.h
  ${RAPIDJSON_INCLUDE}/rapidjson/encodedstream.h
  ${RAPIDJSON_INCLUDE}/rapidjson/filereadstream.h
  ${RAPIDJSON_INCLUDE}/rapidjson/cursorstreamwrapper.h
  ${RAPIDJSON_INCLUDE}/rapidjson/istreamwrapper.h
  ${RAPIDJSON_INCLUDE}/rapidjson/stringbuffer.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/ieee754.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/strtod.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/swap.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/regex.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/diyfp.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/biginteger.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/strfunc.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/itoa.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/stack.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/dtoa.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/meta.h
  ${RAPIDJSON_INCLUDE}/rapidjson/internal/pow10.h
  ${RAPIDJSON_INCLUDE}/rapidjson/encodings.h
  ${RAPIDJSON_INCLUDE}/rapidjson/schema.h
  ${RAPIDJSON_INCLUDE}/rapidjson/stream.h
  ${RAPIDJSON_INCLUDE}/rapidjson/filewritestream.h
  ${RAPIDJSON_INCLUDE}/rapidjson/rapidjson.h
  ${RAPIDJSON_INCLUDE}/rapidjson/document.h
  ${RAPIDJSON_INCLUDE}/rapidjson/allocators.h
  ${RAPIDJSON_INCLUDE}/rapidjson/writer.h
  ${RAPIDJSON_INCLUDE}/rapidjson/error/error.h
  ${RAPIDJSON_INCLUDE}/rapidjson/error/en.h
  ${RAPIDJSON_INCLUDE}/rapidjson/memorybuffer.h
  ${RAPIDJSON_INCLUDE}/rapidjson/pointer.h
  ${RAPIDJSON_INCLUDE}/rapidjson/msinttypes/inttypes.h
  ${RAPIDJSON_INCLUDE}/rapidjson/msinttypes/stdint.h)

target_include_directories(rapidjson_lib INTERFACE ${RAPIDJSON_INCLUDE})

##########################################################################
##########################################################################

add_library(IconFontCppHeaders_lib INTERFACE)

target_include_directories(IconFontCppHeaders_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/IconFontCppHeaders)

##########################################################################
##########################################################################

if(OSX)
  add_subdirectory(macdylibbundler)

  # I'm gambling that these are benign.
  target_compile_options(dylibbundler PRIVATE "-Wno-shorten-64-to-32")
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

add_library(salieri_lib INTERFACE)

target_include_directories(salieri_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/salieri)
target_sources(salieri_lib INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/salieri/salieri.h)

##########################################################################
##########################################################################

if(WIN32)
  set(RCEDIT_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/rcedit/src)
  add_executable(rcedit
    ${RCEDIT_SRC_FOLDER}/main.cc
    ${RCEDIT_SRC_FOLDER}/rescle.cc
    ${RCEDIT_SRC_FOLDER}/rescle.h)
  
  # C4267: 'var' : conversion from 'size_t' to 'type', possible loss of data
  # C4244: 'conversion' conversion from 'type1' to 'type2', possible loss of data
  # C4104: 'identifier' : unreferenced local variable
  target_compile_options(rcedit PRIVATE /wd4267 /wd4244 /wd4101)
endif()

##########################################################################
##########################################################################

set(PERFECT6502_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/perfect6502)

add_library(perfect6502_lib
  ${PERFECT6502_SRC_FOLDER}/netlist_6502.h
  ${PERFECT6502_SRC_FOLDER}/netlist_sim.c
  ${PERFECT6502_SRC_FOLDER}/netlist_sim.h
  ${PERFECT6502_SRC_FOLDER}/perfect6502.c
  ${PERFECT6502_SRC_FOLDER}/perfect6502.h)
target_include_directories(perfect6502_lib INTERFACE ${PERFECT6502_SRC_FOLDER})

##########################################################################
##########################################################################

set(STB_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/stb)

add_library(stb_image_lib INTERFACE)

target_include_directories(stb_image_lib INTERFACE
  ${STB_SRC_FOLDER})

target_sources(stb_image_lib INTERFACE
  ${STB_SRC_FOLDER}/stb_image.h
  ${STB_SRC_FOLDER}/stb_image_write.h)
