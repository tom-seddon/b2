# -*- mode:cmake -*-
set(CMAKE_CXX_STANDARD 17)

##########################################################################
##########################################################################

# Specify additional arguments to add specific boilerplate bits:

# SANITIZERS - set up sanitizers for that target
function(b2_target_boilerplate TARGET)
  set(options SANITIZERS)
  cmake_parse_arguments(PARSE_ARGV 1 arg_b2_target_boilerplate "${options}" "" "")

  if(DEFINED arg_b2_target_boilerplate_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "b2_target_boilerplate: ${TARGET}: unparsed arguments: ${arg_b2_target_boilerplate_UNPARSED_ARGUMENTS}")
  endif()
  
  if(${arg_b2_target_boilerplate_SANITIZERS})
    add_sanitizers(${TARGET})
  endif()
endfunction()

##########################################################################
##########################################################################

include(${CMAKE_CURRENT_LIST_DIR}/src/shared/shared_common.cmake)
