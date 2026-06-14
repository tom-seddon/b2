##########################################################################
##########################################################################

option(BUILD_STATIC_LIBS "" ON)
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(llhttp-release-v9.3.0)

##########################################################################
##########################################################################

if(WIN32)
  function(get_pix_info)
    # https://devblogs.microsoft.com/pix/winpixeventruntime/
    set(PIX_FOLDER ${CMAKE_SOURCE_DIR}/dependencies/WinPixEventRuntime/1.0.220810001 PARENT_SCOPE)
    set(USE_PIX $<CONFIG:Debug,RelWithDebInfo> PARENT_SCOPE)

    # straightforwardly detecting the actual build target appears to be
    # impossible?
    #
    # TODO: maybe try building some code, like SDL does? Still not
    # exactly straightforward though, c'mon
    if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "")
      set(PIX_PLATFORM ${CMAKE_GENERATOR_PLATFORM} PARENT_SCOPE)
    elseif(NOT CMAKE_VS_PLATFORM_NAME_DEFAULT STREQUAL "")
      # if no platform specified, CMake still picked one, but it's not
      # clear how you figure out which it was. So guess.
      set(PIX_PLATFORM ${CMAKE_VS_PLATFORM_NAME_DEFAULT} PARENT_SCOPE)
    else()
      message(FATAL_ERROR "Couldn't detect platform for PIX")
    endif()
  endfunction()

  add_library(pix_lib INTERFACE)

  get_pix_info()
  target_compile_definitions(pix_lib INTERFACE $<${USE_PIX}:USE_PIX>)
  target_include_directories(pix_lib INTERFACE $<${USE_PIX}:${PIX_FOLDER}/include/WinPixEventRuntime>)
  target_link_libraries(pix_lib INTERFACE $<${USE_PIX}:${PIX_FOLDER}/bin/${PIX_PLATFORM}/WinPixEventRuntime.lib>)

  function(add_copy_pix_dlls_command target)
    # putting the whole custom command in a generator expression based
    # on ${USE_PIX} produced nonsense in the post-build step commands
    # list. So, use the same command line in all builds, just
    # replacing copy_if_different with true when appropriate. cmake -E
    # true ignores all arguments and exits with a 0.
    # 
    # should figure out how to use RUNTIME_DEPENDENCY_SET available
    # with newer cmake, perhaps? - see
    # https://cmake.org/cmake/help/latest/command/install.html#installing-runtime-dependencies
    #
    # Or perhaps TARGET_RUNTIME_DLLS:
    # https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html#genex:TARGET_RUNTIME_DLLS
    # (cmake 3.21 or newer only though! VS2019 comes with 3.20...)
    get_pix_info()
    add_custom_command(
      TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E $<IF:${USE_PIX},copy_if_different,true> ${PIX_FOLDER}/bin/${PIX_PLATFORM}/WinPixEventRuntime.dll $<TARGET_FILE_DIR:${TARGET}>
      VERBATIM)
  endfunction()

endif()
