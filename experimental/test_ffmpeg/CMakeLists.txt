cmake_minimum_required(VERSION 3.5)
project(test_ffmpeg)

##########################################################################
##########################################################################

add_executable(test_ffmpeg
 test_ffmpeg.cpp
 )

target_include_directories(test_ffmpeg PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_compile_definitions(test_ffmpeg PRIVATE ${FFMPEG_DEFINITIONS})
target_link_libraries(test_ffmpeg PRIVATE shared_lib ${FFMPEG_LIBRARIES})

##########################################################################
##########################################################################

# from ffmpeg doc/examples

add_executable(encode_audio
  encode_audio.c)

target_include_directories(encode_audio PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_compile_definitions(encode_audio PRIVATE ${FFMPEG_DEFINITIONS})
target_link_libraries(encode_audio PRIVATE shared_lib ${FFMPEG_LIBRARIES} m)

##########################################################################
##########################################################################
