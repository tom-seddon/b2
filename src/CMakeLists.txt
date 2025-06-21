include("../common.cmake")

add_subdirectory(6502)
add_subdirectory(beeb)

add_subdirectory(shared)

# add sanitizer stuff
get_directory_property(shared_targets
  DIRECTORY shared
  BUILDSYSTEM_TARGETS)
get_directory_property(shared_tests_targets
  DIRECTORY shared/tests
  BUILDSYSTEM_TARGETS)
foreach(target IN LISTS shared_targets shared_tests_targets)
  b2_target_boilerplate(${target} SANITIZERS)
endforeach()

add_subdirectory(http)

add_subdirectory(dear_imgui_hex_editor)

if(MSVC)
  add_subdirectory(visual_studio_test_runner)
endif()

add_subdirectory(b2)  
