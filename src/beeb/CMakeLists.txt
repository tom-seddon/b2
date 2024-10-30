cmake_minimum_required(VERSION 3.5)

set(S src)
set(I include/beeb)

set(SRCS
  ${S}/1770.cpp ${I}/1770.h ${I}/1770.inl
  ${S}/6502.cpp ${I}/6502.h
  ${S}/6522.cpp ${I}/6522.h ${I}/6522.inl
  ${S}/BBCMicro.cpp ${S}/BBCMicro_Update.cpp ${S}/BBCMicro_Update.inl ${S}/BBCMicro_Update0.cpp ${S}/BBCMicro_Update1.cpp ${S}/BBCMicro_Update2.cpp ${S}/BBCMicro_Update3.cpp ${S}/BBCMicro_private.inl ${I}/BBCMicro.h ${I}/BBCMicro.inl
  ${S}/BeebLink.cpp ${I}/BeebLink.h ${I}/BeebLink.inl
  ${S}/DiscImage.cpp ${I}/DiscImage.h
  ${S}/DiscInterface.cpp ${I}/DiscInterface.h ${I}/DiscInterface.inl
  ${S}/ExtMem.cpp ${I}/ExtMem.h
  ${S}/MC146818.cpp ${I}/MC146818.h ${I}/MC146818.inl
  ${S}/OutputData.cpp ${I}/OutputData.h
  ${S}/SN76489.cpp ${I}/SN76489.h
  ${S}/SaveTrace.cpp ${I}/SaveTrace.h ${I}/SaveTrace.inl ${S}/SaveTrace_private.inl
  ${S}/TVOutput.cpp ${I}/TVOutput.h ${I}/TVOutput.inl
  ${S}/Trace.cpp ${I}/Trace.h
  ${S}/VideoULA.cpp ${I}/VideoULA.h
  ${S}/conf.cpp ${I}/conf.h
  ${S}/crtc.cpp ${I}/crtc.h ${I}/crtc.inl
  ${S}/keys.cpp ${I}/keys.h ${I}/keys.inl
  ${S}/roms.cpp ${I}/roms.h ${I}/roms.inl
  ${S}/sound.cpp ${I}/sound.h
  ${S}/teletext.cpp ${I}/teletext.h ${I}/teletext.inl ${S}/teletext_font.inl
  ${S}/type.cpp ${I}/type.h ${I}/type.inl
  ${S}/video.cpp ${I}/video.h ${I}/video.inl
  ${S}/tube.cpp ${I}/tube.h
  ${S}/BBCMicroParasiteType.cpp ${I}/BBCMicroParasiteType.inl ${I}/BBCMicroParasiteType.h
  ${S}/adc.cpp ${I}/adc.h
  ${S}/PCD8572.cpp ${I}/PCD8572.h ${I}/PCD8572.inl
  ${S}/uef.cpp ${I}/uef.h
  ${S}/BBCMicroState.cpp ${I}/BBCMicroState.h ${I}/BBCMicroState.inl
)

if(MSVC)
  set(SRCS ${SRCS} beeb.natvis)
endif()

add_library(beeb_lib ${SRCS})
add_sanitizers(beeb_lib)
add_config_define(beeb_lib)

target_include_directories(beeb_lib PUBLIC include)
target_link_libraries(beeb_lib PUBLIC shared_lib 6502_lib salieri_lib)

add_subdirectory(tests)
