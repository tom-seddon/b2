set(S src)
set(I include/beeb)

set(SRCS
  ${S}/1770.cpp ${I}/1770.h ${I}/1770.inl
  ${S}/6502.cpp ${I}/6502.h
  ${S}/6522.cpp ${I}/6522.h ${I}/6522.inl
  ${S}/BBCMicro.cpp ${S}/BBCMicro_Update.cpp ${S}/BBCMicro_Update.inl ${S}/BBCMicro_private.inl ${I}/BBCMicro.h ${I}/BBCMicro.inl
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
  ${S}/VideoULA.cpp ${I}/VideoULA.h ${S}/VideoULA_private.inl
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
  ${S}/DiscGeometry.cpp ${I}/DiscGeometry.h
  ${S}/DirectDiscImage.cpp ${I}/DirectDiscImage.h
  ${S}/MemoryDiscImage.cpp ${I}/MemoryDiscImage.h
  ${S}/scsi.cpp ${I}/scsi.h ${I}/scsi.inl
  ${S}/HardDiskImage.cpp ${I}/HardDiskImage.h
  ${S}/MC6850.cpp ${I}/MC6850.h ${I}/MC6850.inl
  ${S}/serproc.cpp ${I}/serproc.h ${I}/serproc.inl
)

if(MSVC)
  set(SRCS ${SRCS} beeb.natvis)
endif()

set(SRCS ${SRCS}
  generated/BBCMicro.groups.generated.h
  generated/BBCMicro.groups.generated.inl)

# These files are automatically generated, so the set won't change
# from run to run.
#
# The tool that creates them touches this file, so cmake will re-run
# as required.
file(GLOB UPDATE_GENERATED_SRCS
  LIST_DIRECTORIES false
  generated/BBCMicro.group*.generated.cpp)
set(SRCS ${SRCS} ${UPDATE_GENERATED_SRCS})

source_group(generated REGULAR_EXPRESSION .*\.generated\..*)

add_library(beeb_lib ${SRCS})
b2_target_boilerplate(beeb_lib SANITIZERS)

target_include_directories(beeb_lib PUBLIC include)
target_link_libraries(beeb_lib PUBLIC shared_lib 6502_lib salieri_lib)

add_subdirectory(tests)
