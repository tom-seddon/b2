cmake_minimum_required(VERSION 3.20)

##########################################################################
##########################################################################

option(BUILD_STATIC_LIBS "" ON)
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(llhttp-release-v9.1.3)

##########################################################################
##########################################################################

if(WIN32)
  function(get_pix_info)
    # https://devblogs.microsoft.com/pix/winpixeventruntime/
    set(PIX_FOLDER ${CMAKE_SOURCE_DIR}/dependencies/WinPixEventRuntime/1.0.220810001 PARENT_SCOPE)
    set(USE_PIX $<CONFIG:Debug,RelWithDebInfo> PARENT_SCOPE)
  endfunction()

  add_library(pix_lib INTERFACE)

  get_pix_info()
  target_compile_definitions(pix_lib INTERFACE $<${USE_PIX}:USE_PIX>)
  target_include_directories(pix_lib INTERFACE $<${USE_PIX}:${PIX_FOLDER}/include/WinPixEventRuntime>)
  # TODO how to properly select x64 vs ARM64?
  target_link_libraries(pix_lib INTERFACE $<${USE_PIX}:${PIX_FOLDER}/bin/x64/WinPixEventRuntime.lib>)

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
      COMMAND ${CMAKE_COMMAND} -E $<IF:${USE_PIX},copy_if_different,true> ${PIX_FOLDER}/bin/x64/WinPixEventRuntime.dll $<TARGET_FILE_DIR:b2>
      VERBATIM)
  endfunction()
  
endif()
