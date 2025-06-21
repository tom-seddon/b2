project(shared)

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

set(S src)
set(H include/shared)

set(SRCS
  ${S}/debug.cpp ${H}/debug.h
  ${S}/log.cpp ${H}/log.h
  ${S}/system.cpp ${H}/system.h
  ${S}/CommandLineParser.cpp ${H}/CommandLineParser.h
  ${S}/testing.cpp ${H}/testing.h
  ${S}/sha1.cpp ${H}/sha1.h
  ${S}/path.cpp ${H}/path.h
  ${S}/load_store.cpp ${H}/load_store.h
  ${S}/enums.cpp ${H}/enums.h ${H}/enum_decl.h  ${H}/enum_def.h  ${H}/enum_end.h
  ${H}/system_specific.h
  ${H}/popwarn.h ${H}/pushwarn_bitfields.h ${H}/pushwarn_flexible_struct_member.h ${H}/pushwarn_padded_struct.h
  ${S}/relacy.cpp ${H}/relacy.h
  ${S}/mutex.cpp ${H}/mutex.h ${H}/mutex.inl
  ${H}/pshpack1.h ${H}/pshpack4.h ${H}/pshpack8.h ${H}/poppack.h
  ${S}/file_io.cpp ${H}/file_io.h ${H}/file_io.inl
  ${S}/strings.cpp ${H}/strings.h
  )

if(APPLE)
  set(SRCS ${SRCS}
    ${S}/system_osx.cpp ${S}/system_posix.cpp
    ${S}/path_posix.cpp
    ${S}/path_osx.cpp
    )
elseif(UNIX)
  # Of course, Linux is the only possible Unix.
  set(SRCS ${SRCS}
    ${S}/system_linux.cpp ${S}/system_posix.cpp
    ${S}/path_posix.cpp ${S}/path_linux.cpp
    )    
elseif(WIN32)
  set(SRCS ${SRCS}
    ${S}/system_windows.cpp ${H}/system_windows.h
    ${S}/path_windows.cpp
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

target_include_directories(shared_lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(shared_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(UNIX)
  target_link_libraries(shared_lib INTERFACE ${CMAKE_THREAD_LIBS_INIT})
  if(NOT APPLE)
    target_link_libraries(shared_lib INTERFACE rt uuid)
  endif()
endif()

if(WIN32)
  target_link_libraries(shared_lib INTERFACE rpcrt4.lib)
endif()

target_compile_definitions(shared_lib PUBLIC
  BUILD_TYPE_$<CONFIG>=1
  ASSERT_ENABLED=$<NOT:$<CONFIG:Final>>)

set(MUTEX_DEBUGGING_ENABLED 1)
if(OSX)
  # At some point, I introduced something that's only present in 10.12
  # and later. So when explicitly targeting a specific version, switch
  # this stuff off when appropriate.
  #
  # If not explicitly targeting a specific version, you just get
  # whatever you get.
  #
  # Going by the docs, it seems you should be able to test DEFINED
  # CMAKE_OSX_DEPLOYMENT_TARGET, but it seems that can be true when
  # running from release.py even though CMAKE_OSX_DEPLOYMENT_TARGET is
  # actually blank. I haven't figured out why.
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    message(STATUS "OS X deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if(${CMAKE_OSX_DEPLOYMENT_TARGET} VERSION_LESS 10.12)
      message(STATUS "Mutex debugging disabled")
      set(MUTEX_DEBUGGING_ENABLED 0)
    endif()
  endif()
endif()

if(MUTEX_DEBUGGING_ENABLED)
  target_compile_definitions(shared_lib PUBLIC
    MUTEX_DEBUGGING=0)
else()
  target_compile_definitions(shared_lib PUBLIC
    MUTEX_DEBUGGING=$<NOT:$<CONFIG:Final>>)
endif()

add_subdirectory(tests)
