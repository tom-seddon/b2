cmake_minimum_required(VERSION 3.5)
project(test_media_foundation)

add_executable(test_media_foundation
 test_media_foundation.cpp
 test_media_foundation_msdn.cpp 
 test_media_foundation.inl
 testing_windows.h
 testing_windows.cpp
 )

target_link_libraries(test_media_foundation shared_lib mf.lib mfplat.lib mfreadwrite.lib)
