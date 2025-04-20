project(test_nlohmann_json)
add_executable(test_nlohmann_json
  test_nlohmann_json.cpp
  test_nlohmann_json.inl)
b2_target_boilerplate(test_nlohmann_json SANITIZERS)
target_link_libraries(test_nlohmann_json PRIVATE shared_lib nlohmann_json::nlohmann_json)
target_compile_definitions(test_nlohmann_json PRIVATE
  SRC_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}"
  DEST_FOLDER="${CMAKE_BINARY_DIR}/test_nlohmann_json")
target_link_libraries(test_nlohmann_json PRIVATE ${SDL2_LIBRARY})
