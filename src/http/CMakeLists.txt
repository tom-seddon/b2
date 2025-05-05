set(S src)
set(I include/http)

add_library(http_lib
  ${S}/http.cpp ${I}/http.h
  ${S}/HTTPClient.cpp ${I}/HTTPClient.h
  ${S}/HTTPServer.cpp ${I}/HTTPServer.h)
b2_target_boilerplate(http_lib SANITIZERS)
target_include_directories(http_lib PUBLIC include)
target_link_libraries(http_lib PUBLIC shared_lib)
target_link_libraries(http_lib PRIVATE ${LIBCURL_TARGET} ${LibUV_LIBRARIES} llhttp_static)

add_subdirectory(tests)
