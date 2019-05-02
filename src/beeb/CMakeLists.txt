cmake_minimum_required(VERSION 3.5)

set(S src)
set(I include/beeb)

set(SRCS
  ${S}/1770.cpp ${I}/1770.h ${I}/1770.inl
  ${S}/6522.cpp ${I}/6522.h ${I}/6522.inl
  ${S}/BBCMicro.cpp ${S}/BBCMicro_private.inl ${I}/BBCMicro.h ${I}/BBCMicro.inl
  ${S}/conf.cpp ${I}/conf.h
  ${S}/crtc.cpp ${I}/crtc.h ${I}/crtc.inl
  ${S}/ExtMem.cpp ${I}/ExtMem.h
  ${S}/keys.cpp ${I}/keys.h ${I}/keys.inl
  ${S}/OutputData.cpp ${I}/OutputData.h
  ${S}/SN76489.cpp ${I}/SN76489.h
  ${S}/sound.cpp ${I}/sound.h
  ${S}/teletext.cpp ${I}/teletext.h ${I}/teletext.inl ${S}/teletext_font.inl
  ${S}/video.cpp ${I}/video.h ${I}/video.inl
  ${S}/VideoULA.cpp ${I}/VideoULA.h
  ${S}/DiscInterface.cpp ${I}/DiscInterface.h ${I}/DiscInterface.inl
  ${S}/Trace.cpp ${I}/Trace.h
  ${S}/MC146818.cpp ${I}/MC146818.h ${I}/MC146818.inl
  ${S}/DiscImage.cpp ${I}/DiscImage.h
  ${S}/BeebLink.cpp ${I}/BeebLink.h ${I}/BeebLink.inl
  ${S}/6502.cpp ${I}/6502.h
  ${S}/type.cpp ${I}/type.h ${I}/type.inl
)

if(MSVC)
  set(SRCS ${SRCS} beeb.natvis)
endif()

add_library(beeb_lib ${SRCS})
add_sanitizers(beeb_lib)
add_config_define(beeb_lib)

target_include_directories(beeb_lib PUBLIC include)
target_link_libraries(beeb_lib PUBLIC shared_lib 6502_lib)

add_subdirectory(tests)
