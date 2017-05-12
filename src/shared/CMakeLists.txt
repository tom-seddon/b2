project(shared)
cmake_minimum_required(VERSION 3.0)

if(UNIX)
  find_package(Threads REQUIRED)
endif()

##########################################################################
##########################################################################

set(H h/shared)

set(SRCS
  c/debug.c ${H}/debug.h
  c/log.cpp ${H}/log.h
  c/system.c ${H}/system.h
  c/CommandLineParser.cpp ${H}/CommandLineParser.h
  c/testing.cpp ${H}/testing.h
  c/sha1.cpp ${H}/sha1.h
  c/path.cpp ${H}/path.h
  c/load_store.c ${H}/load_store.h
  ${H}/enum_decl.h  ${H}/enum_def.h  ${H}/enum_end.h  ${H}/system_specific.h
  ${H}/popwarn.h ${H}/pushwarn_bitfields.h ${H}/pushwarn_flexible_struct_member.h ${H}/pushwarn_padded_struct.h
  c/relacy.cpp ${H}/relacy.h
  )

if(APPLE)
  set(SRCS ${SRCS}
    c/system_osx.cpp c/system_posix.c
    c/path_posix.cpp
    c/path_osx.cpp
    )
elseif(UNIX)
  # Of course, Linux is the only possible Unix.
  set(SRCS ${SRCS}
    c/system_posix.c c/system_linux.cpp
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
