add_executable(test_http test_http.cpp)
b2_target_boilerplate(test_http SANITIZERS)
target_link_libraries(test_http PRIVATE shared_lib http_lib)
add_test(
  NAME http/test_http
  COMMAND $<TARGET_FILE:test_http>)
