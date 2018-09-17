#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SDL2::SDL2-static" for configuration "Debug"
set_property(TARGET SDL2::SDL2-static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SDL2::SDL2-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "m;dl;dbus-1;-Wl,--no-undefined;-pthread"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libSDL2d.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS SDL2::SDL2-static )
list(APPEND _IMPORT_CHECK_FILES_FOR_SDL2::SDL2-static "${_IMPORT_PREFIX}/lib/libSDL2d.a" )

# Import target "SDL2::SDL2main" for configuration "Debug"
set_property(TARGET SDL2::SDL2main APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SDL2::SDL2main PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libSDL2maind.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS SDL2::SDL2main )
list(APPEND _IMPORT_CHECK_FILES_FOR_SDL2::SDL2main "${_IMPORT_PREFIX}/lib/libSDL2maind.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
