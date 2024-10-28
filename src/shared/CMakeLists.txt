project(shared)
cmake_minimum_required(VERSION 3.5)

if(UNIX)
  find_package(Threads REQUIRED)
endif()

##########################################################################
##########################################################################

check_cxx_source_compiles("#include <string.h>
typedef decltype(&strlcpy) T;int main(void){return 0;}" HAVE_STRLCPY)
message(STATUS "strlcpy exists: ``${HAVE_STRLCPY}''")

##########################################################################
##########################################################################

set(H h/shared)

set(SRCS
  c/debug.cpp ${H}/debug.h
  c/log.cpp ${H}/log.h
  c/system.cpp ${H}/system.h
  c/CommandLineParser.cpp ${H}/CommandLineParser.h
  c/testing.cpp ${H}/testing.h
  c/sha1.cpp ${H}/sha1.h
  c/path.cpp ${H}/path.h
  c/load_store.cpp ${H}/load_store.h
  ${H}/enum_decl.h  ${H}/enum_def.h  ${H}/enum_end.h  ${H}/system_specific.h
  ${H}/popwarn.h ${H}/pushwarn_bitfields.h ${H}/pushwarn_flexible_struct_member.h ${H}/pushwarn_padded_struct.h
  c/relacy.cpp ${H}/relacy.h
  c/mutex.cpp ${H}/mutex.h ${H}/mutex.inl
  ${H}/pshpack1.h ${H}/pshpack4.h ${H}/pshpack8.h ${H}/poppack.h
  c/file_io.cpp ${H}/file_io.h ${H}/file_io.inl
  )

if(APPLE)
  set(SRCS ${SRCS}
    c/system_osx.cpp c/system_posix.cpp
    c/path_posix.cpp
    c/path_osx.cpp
    )
elseif(UNIX)
  # Of course, Linux is the only possible Unix.
  set(SRCS ${SRCS}
    c/system_linux.cpp c/system_posix.cpp
    c/path_posix.cpp c/path_linux.cpp
    )    
elseif(WIN32)
  set(SRCS ${SRCS}
    c/system_windows.cpp ${H}/system_windows.h
    c/path_windows.cpp
    )
endif()

##########################################################################
##########################################################################

add_library(shared_lib STATIC ${SRCS})

if(HAVE_STRLCPY)
  # There are HAVE_STRLCPY defines in SDL and libcurl, so give this
  # one a different name in order not to interfere.
  #
  # It must be public, as this influences system.h.
  target_compile_definitions(shared_lib PUBLIC SYSTEM_HAVE_STRLCPY=1)
endif()

target_include_directories(shared_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/h)
target_include_directories(shared_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/h)

if(UNIX)
  target_link_libraries(shared_lib INTERFACE ${CMAKE_THREAD_LIBS_INIT})
  if(NOT APPLE)
    target_link_libraries(shared_lib INTERFACE rt uuid)
  endif()
endif()

if(WIN32)
  target_link_libraries(shared_lib INTERFACE rpcrt4.lib)
endif()

add_subdirectory("t")
add_sanitizers(shared_lib)
add_config_define(shared_lib)
