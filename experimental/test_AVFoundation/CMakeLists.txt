project(test_AVFoundation)

add_executable(test_AVFoundation
  test_AVFoundation.mm
  test_AVFoundation.inl
  )
target_link_libraries(test_AVFoundation PRIVATE shared_lib stb_image_lib)
target_link_libraries(test_AVFoundation PRIVATE "-framework AVFoundation")
target_link_libraries(test_AVFoundation PRIVATE "-framework CoreVideo")
target_link_libraries(test_AVFoundation PRIVATE "-framework CoreMedia")
target_link_libraries(test_AVFoundation PRIVATE "-framework AppKit")
