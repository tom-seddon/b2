# -*- mode:cmake -*-
set(CMAKE_CXX_STANDARD 17)

##########################################################################
##########################################################################

# Use this to add a BUILD_TYPE_Debug=1 (etc.) to the project's
# defines. Seems that generator expressions only operate in the
# context of a particular project.
function(add_config_define TARGET)
  target_compile_definitions(${TARGET} PRIVATE -DBUILD_TYPE_$<CONFIG>=1)
endfunction()

##########################################################################
##########################################################################

function(target_boilerplate TARGET)
  add_config_define(${TARGET})
  add_sanitizers(${TARGET})
endfunction()

##########################################################################
##########################################################################

# It's not very neat to just set the various CMake globals, but this
# stuff ideally wants to apply to all projects without requiring any
# project-specific action.

##########################################################################
##########################################################################

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DASSERT_ENABLED=1")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DASSERT_ENABLED=1")

##########################################################################
##########################################################################

# This will just make a huge mess if it's inconsistent. Rather than add
# BUILD_TYPE_$<CONFIG>=1 to every project, which thanks to the generator
# expression appears to be basically rocket science, just set the define
# for Final build only and have a #ifdef in the code.
#
# The mutex debugging stuff only ever affects C++.

set(CMAKE_CXX_FLAGS_FINAL "${CMAKE_CXX_FLAGS_FINAL} -DMUTEX_DEBUGGING=0")

if(OSX)
  # At some point I introduced something that's only present in 10.12
  # and later.
  message(STATUS "OS X deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
  if(${CMAKE_OSX_DEPLOYMENT_TARGET} VERSION_LESS 10.12)
    message(STATUS "Mutex debugging disabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMUTEX_DEBUGGING=0")
  endif()
endif()

##########################################################################
##########################################################################

if(${CMAKE_C_COMPILER_ID} MATCHES "GNU|Clang")
  # Warnings.
  add_definitions("-Wall -Wuninitialized -Winit-self -pedantic")
  add_definitions("-Wsign-conversion -Wunused-result")

  # Not very nice, but some gcc versions complain about this flag when
  # building C++.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-function-declaration")

  # if(${CMAKE_C_COMPILER_ID} MATCHES "GNU")
  #   # http://stackoverflow.com/questions/4058565/check-gcc-minor-in-cmake
  #   execute_process(
  #     COMMAND ${CMAKE_C_COMPILER} -dumpversion
  #     OUTPUT_VARIABLE GCC_VERSION)
  #   if(NOT (GCC_VERSION VERSION_LESS 4.8))
  #     add_definitions("-Werror=int-conversion")
  #   endif()
  # endif()

  if(${CMAKE_C_COMPILER_ID} MATCHES "Clang")
    add_definitions("-Wno-invalid-offsetof")
    add_definitions("-Werror=incompatible-pointer-types")
    add_definitions("-Werror=int-conversion")
    add_definitions("-Werror=return-type")

    # This is a potentially useful warning, but there seems to be no
    # way to inhibit it just for poppack.h.
    #
    # See also, e.g.,
    # https://github.com/dotnet/coreclr/pull/16855/commits/f7640e3b1310c9d1e1aa20c7d81fd7a6aa08a32e
    add_definitions("-Wno-pragma-pack")

    # gcc 7.4 seems to have these by default, and they're pretty
    # useful.
    add_definitions("-Wsign-compare")
    add_definitions("-Wsign-conversion")

    add_definitions("-Wsometimes-uninitialized")
  endif()

  # There is test data longer than the ISO-mandated 4095 chars.
  add_definitions("-Wno-overlength-strings")
  add_definitions("-Wunused-parameter")

  # set(CMAKE_C_FLAGS_RELWITHDEBINFO "-save-temps=obj ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  # set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-save-temps=obj ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

  # C brand. (-std=c1x is compatible with gcc 4.6)
  
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c1x")
  #add_definitions("-std=c99")
  set(GCC ON)
elseif(MSVC)
  # Force /W4.
  #
  # http://stackoverflow.com/questions/2368811/how-to-set-warning-level-in-cmake
  if(CMAKE_C_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()

  # See https://twitter.com/rasmusbonnedal/status/1685574422046879747
  #
  # In practice /Ob2 doesn't seem to make a big difference.
  
  # string(REPLACE "/Ob1" "/Ob2" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  # string(REPLACE "/Ob1" "/Ob2" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

  # /O2 = max speed (default), /O1 = min size
  #
  # /O2 seems usefully better.
  
  # string(REPLACE "/O2" "/O1" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  # string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

  # Warnings as errors:
  # 
  # C4013: undefined; assuming extern returning int
  # C4022: pointer mismatch for actual parameter
  # C4028: formal parameter different from declaration
  # C4716: must return a value
  # C4047: XXX differs in levels of indirection from XXX
  # C4020: too many actual parameters
  # C4133: incompatible types
  # C4098: void function returning a value
  # C4113: <function pointer type> differs in parameter lists from <function pointer>
  # C4715: not all control paths return a value
  
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /we4013 /we4022 /we4028 /we4716 /we4047 /we4020 /we4133 /we4096 /we4113 /we4715 /we4800")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4013 /we4022 /we4028 /we4716 /we4047 /we4020 /we4133 /we4098 /we4113 /we4715 /we4800")
  
  # Warnings not relevant for C99:
  #
  # C4200: zero-sized array in struct/union
  # C4204: non-constant aggregate initializer
  # C4221: <aggregate member> cannot be initialized using address of automatic variable
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4200 /wd4204 /wd4221")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4200 /wd4204 /wd4221")
  
  # Warnings I don't care about and/or aren't an issue with gcc+clang:
  # 
  # C4214: nonstandard extension used: bit field types other than int
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4214")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4214")

  # Remove annoying deprecation nonsense.
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")

  # Enable multithreaded compiles. (There's an actual msbuild flag for
  # this, but CMake doesn't seem to support it.)
  #
  # https://randomascii.wordpress.com/2014/03/22/make-vc-compiles-fast-through-parallel-compilation/
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

  # Fix extended aligned storage error. The extended align storage is desired.
  add_definitions("-D_ENABLE_EXTENDED_ALIGNED_STORAGE")

  # Remove basic runtime checks. Really kills debug build performance.
  #
  # https://stackoverflow.com/questions/8587764/remove-runtime-checks-compiler-flag-per-project-in-cmake
  string(REGEX REPLACE "[/-]RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
  string(REGEX REPLACE "[/-]RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})

  message(STATUS "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
  message(STATUS "CMAKE_C_FLAGS = ${CMAKE_C_FLAGS}")
  message(STATUS "CMAKE_CXX_FLAGS_DEBUG = ${CMAKE_CXX_FLAGS_DEBUG}")
  message(STATUS "CMAKE_C_FLAGS_DEBUG = ${CMAKE_C_FLAGS_DEBUG}")
  message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO = ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  message(STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO = ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
endif()

##########################################################################
##########################################################################

# Linux libc wants this.
if(UNIX)
  add_definitions("-D_GNU_SOURCE")
endif()

##########################################################################
##########################################################################
