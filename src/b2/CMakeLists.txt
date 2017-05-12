cmake_minimum_required(VERSION 3.5)
include(BundleUtilities)

##########################################################################
##########################################################################

add_executable(b2 MACOSX_BUNDLE
  b2.cpp b2.h
  65link.cpp 65link.h
  BeebThread.cpp BeebThread.h BeebThread.inl
  BeebWindows.cpp BeebWindows.h 
  dear_imgui.cpp dear_imgui.h
  download.h
  filters.cpp filters.h
  MemoryDiscImage.cpp MemoryDiscImage.h
  misc.cpp misc.h
  native_ui.cpp native_ui.h
  Remapper.cpp Remapper.h
  TVOutput.cpp TVOutput.h TVOutput.inl
  keymap.cpp keymap.h
  conf.cpp conf.h
  keys.cpp keys.h keys.inl
  BeebState.cpp BeebState.h
  native_ui_noc.h
  JobQueue.cpp JobQueue.h
  GenerateThumbnailJob.cpp GenerateThumbnailJob.h
  BeebWindow.cpp BeebWindow.h BeebWindow.inl
  BeebWindowInitArguments.cpp BeebWindowInitArguments.h
  TimelineUI.cpp TimelineUI.h
  beeb_events.cpp beeb_events.h beeb_events.inl beeb_events_types.inl
  load_save.cpp load_save.h
  Messages.cpp Messages.h
  ConfigsUI.cpp ConfigsUI.h
  BeebConfig.cpp BeebConfig.h
  KeymapsUI.cpp KeymapsUI.h
  MessagesUI.cpp MessagesUI.h
  TraceUI.cpp TraceUI.h TraceUI.inl
  NVRAMUI.cpp NVRAMUI.h NVRAMUI_private.inl
  AudioCallbackUI.cpp AudioCallbackUI.h
  VideoWriter.cpp VideoWriter.h
  WriteVideoJob.cpp WriteVideoJob.h
  Timeline.cpp Timeline.h
  VBlankMonitor.cpp VBlankMonitor.h
  VBlankMonitorDefault.cpp VBlankMonitorDefault.h
  MessageQueue.cpp MessageQueue.h
  )

##########################################################################
##########################################################################

add_config_define(b2)

##########################################################################
##########################################################################

# Apple-specific general stuff.

if(APPLE)
  # There doesn't appear to be any non-hacky way of getting this.
  #
  # (BundleUtilities has a GET_DOTAPP_DIR function, but it doesn't
  # work with generator expressions. Presumably it was written to work
  # with the pre-cmake 3.0 target property that has now disappeared.)
  set(BUNDLE_CONTENTS_FOLDER $<TARGET_FILE_DIR:b2>/..)
  set(ICON_PNG ${CMAKE_SOURCE_DIR}/etc/icon/icon.png)
  set(ICONS_TMP ${CMAKE_CURRENT_BINARY_DIR}/b2.iconset)

  # https://developer.apple.com/library/content/documentation/UserExperience/Conceptual/OSXHIGuidelines/Designing.html#//apple_ref/doc/uid/20000957-CH87-SW1

  add_custom_command(
    OUTPUT b2.icns ${ICONS_TMP}/icon_16x16.png ${ICONS_TMP}/icon_32x32.png ${ICONS_TMP}/icon_64x64.png ${ICONS_TMP}/icon_128x128.png ${ICONS_TMP}/icon_256x256.png ${ICONS_TMP}/icon_512x512.png ${ICONS_TMP}/icon_512x512@2x.png ${ICONS_TMP}/icon_16x16@2x.png ${ICONS_TMP}/icon_32x32@2x.png ${ICONS_TMP}/icon_128x128@2x.png ${ICONS_TMP}/icon_256x256@2x.png
    COMMAND cmake -E make_directory ${ICONS_TMP}
    COMMAND sips -z 16 16 ${ICON_PNG} --out ${ICONS_TMP}/icon_16x16.png
    COMMAND sips -z 32 32 ${ICON_PNG} --out ${ICONS_TMP}/icon_32x32.png
    COMMAND sips -z 64 64 ${ICON_PNG} --out ${ICONS_TMP}/icon_64x64.png
    COMMAND sips -z 128 128 ${ICON_PNG} --out ${ICONS_TMP}/icon_128x128.png
    COMMAND sips -z 256 256 ${ICON_PNG} --out ${ICONS_TMP}/icon_256x256.png
    COMMAND sips -z 512 512 ${ICON_PNG} --out ${ICONS_TMP}/icon_512x512.png
    COMMAND sips -z 1024 1024 ${ICON_PNG} --out ${ICONS_TMP}/icon_512x512@2x.png
    COMMAND cmake -E copy_if_different ${ICONS_TMP}/icon_32x32.png ${ICONS_TMP}/icon_16x16@2x.png
    COMMAND cmake -E copy_if_different ${ICONS_TMP}/icon_64x64.png ${ICONS_TMP}/icon_32x32@2x.png
    COMMAND cmake -E copy_if_different ${ICONS_TMP}/icon_256x256.png ${ICONS_TMP}/icon_128x128@2x.png
    COMMAND cmake -E copy_if_different ${ICONS_TMP}/icon_512x512.png ${ICONS_TMP}/icon_256x256@2x.png
    COMMAND iconutil -c icns ${ICONS_TMP}
    DEPENDS ${ICON_PNG}
    MAIN_DEPENDENCY ${ICON_PNG}
    )
  
  set_target_properties(b2
    PROPERTIES
    MACOSX_BUNDLE_NAME "b2"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.tom-seddon.b2"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/macos/Info.plist
    MACOSX_BUNDLE_ICON_FILE b2.icns
    )
  
  target_sources(b2 PRIVATE
    b2.icns)

  set_source_files_properties(b2.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
endif()

##########################################################################
##########################################################################

# Windows-specific general stuff.

if(WIN32)
  set(MAGICK ${CMAKE_SOURCE_DIR}/etc/ImageMagick-7.0.5-4-portable-Q16-x64/magick.exe)
  set(RCEDIT ${CMAKE_SOURCE_DIR}/etc/rcedit/rcedit.exe)
  set(ICON_PNG ${CMAKE_SOURCE_DIR}/etc/icon/icon.png)
  set(ICON_ICO b2_icons.ico)
  
  add_custom_command(
    OUTPUT ${ICON_ICO} icon_16x16_32bpp.png icon_16x16_8bpp.bmp icon_32x32_32bpp.png icon_32x32_8bpp.bmp icon_48x48_32bpp.png icon_48x48_8bpp.bmp icon_64x64_32bpp.png icon_256x256_32bpp.png
    COMMAND ${MAGICK} ${ICON_PNG} -resize 256x256 icon_256x256_32bpp.png
    COMMAND ${MAGICK} ${ICON_PNG} -resize 16x16 icon_16x16_32bpp.png
    COMMAND ${MAGICK} ${ICON_PNG} -resize 32x32 icon_32x32_32bpp.png
    COMMAND ${MAGICK} ${ICON_PNG} -resize 48x48 icon_48x48_32bpp.png
    COMMAND ${MAGICK} ${ICON_PNG} -resize 64x64 icon_64x64_32bpp.png
    COMMAND ${MAGICK} ${ICON_PNG} -resize 16x16 -type palette icon_16x16_8bpp.bmp
    COMMAND ${MAGICK} ${ICON_PNG} -resize 32x32 -type palette icon_32x32_8bpp.bmp
    COMMAND ${MAGICK} ${ICON_PNG} -resize 48x48 -type palette icon_48x48_8bpp.bmp
    COMMAND ${MAGICK} icon_256x256_32bpp.png icon_48x48_8bpp.bmp icon_32x32_8bpp.bmp icon_16x16_8bpp.bmp icon_256x256_32bpp.png icon_64x64_32bpp.png icon_48x48_32bpp.png icon_32x32_32bpp.png icon_16x16_32bpp.png ${ICON_ICO}
    DEPENDS ${ICON_PNG}
    MAIN_DEPENDENCY ${ICON_PNG})

  add_custom_command(TARGET b2 POST_BUILD
    COMMAND ${RCEDIT} $<TARGET_FILE:b2> --set-icon ${ICON_ICO})
  
  target_sources(b2 PRIVATE ${ICON_ICO})
  target_sources(b2 PRIVATE guids_win32.c)
  target_sources(b2 PRIVATE b2.natvis)

  # /INCREMENTAL:NO - http://stackoverflow.com/questions/21569478
  #
  # /OPT:REF /OPT:ICF - no idea why these have gone missing. Aren't
  # they supposed to be the defaults?? (This would happen in VC6
  # sometimes too, but I can't remember why... anyway, specifying them
  # explicitly was always the solution then too.)
  set(EXTRA_LINKER_FLAGS "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
  set_target_properties(b2 PROPERTIES LINK_FLAGS_RELWITHDEBINFO ${EXTRA_LINKER_FLAGS})
  set_target_properties(b2 PROPERTIES LINK_FLAGS_FINAL ${EXTRA_LINKER_FLAGS})
endif()

##########################################################################
##########################################################################

# Linux-specific general stuff.

if(UNIX)
  target_link_libraries(b2 PRIVATE m)
endif()

##########################################################################
##########################################################################

# VBlank monitor.

if(WIN32)
  target_sources(b2 PRIVATE VBlankMonitorWindows.cpp VBlankMonitorWindows.h)
  target_link_libraries(b2 PRIVATE dxgi.lib)
elseif(OSX)
  target_sources(b2 PRIVATE VBlankMonitorOSX.cpp VBlankMonitorOSX.h)
  target_link_libraries(b2 PRIVATE "-framework QuartzCore")
  target_link_libraries(b2 PRIVATE "-framework CoreVideo")
endif()  

##########################################################################
##########################################################################

# NOC. Also, on OS X, other UI-related Objective C(++) stuff.

if(OSX)
  target_sources(b2 PRIVATE
    native_ui.mm
    native_ui_noc.m
    load_save.mm)
else()
  target_sources(b2 PRIVATE
    native_ui_noc.c)
endif()

##########################################################################
##########################################################################

# HTTP download stuff.

if(WIN32)
  target_sources(b2 PRIVATE download_windows.cpp)
  target_link_libraries(b2 PRIVATE urlmon.lib)
else()
  target_sources(b2 PRIVATE download_null.c)
endif()

##########################################################################
##########################################################################

# Media Foundation video writer.

if(WIN32)
  target_sources(b2 PRIVATE VideoWriterMF.cpp VideoWriterMF.h VideoWriterMF_private.inl)
  target_link_libraries(b2 PRIVATE mf.lib mfplat.lib mfreadwrite.lib)
endif()

##########################################################################
##########################################################################

# FFmpeg video writer.

if(HAVE_FFMPEG)
  target_sources(b2 PRIVATE VideoWriterFFmpeg.cpp VideoWriterFFmpeg.h)
  target_link_libraries(b2 PRIVATE ${FFMPEG_LIBRARIES})
  target_include_directories(b2 PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_compile_definitions(b2 PRIVATE ${FFMPEG_DEFINITIONS} -DHAVE_FFMPEG=1)
endif()

##########################################################################
##########################################################################

add_sanitizers(b2)

##########################################################################
##########################################################################

target_link_libraries(b2 PRIVATE
  beeb_lib
  shared_lib
  6502_lib
  miniz_lib
  remotery_lib
  imgui_lib
  noc_lib
  rapidjson_lib
  IconFontCppHeaders_lib
  )

##########################################################################
##########################################################################

# SDL2.

target_include_directories(b2 PRIVATE ${SDL2_INCLUDE_DIR})
target_compile_definitions(b2 PRIVATE ${SDL2_DEFINITIONS})
target_link_libraries(b2 PRIVATE ${SDL2_LIBRARY})

##########################################################################
##########################################################################

if(HAVE_LIBCURL)
  target_link_libraries(b2 PRIVATE libcurl)
endif()

##########################################################################
##########################################################################

# Post build step - copy assets.
#
# The release process copies the asset folder from an unspecified
# configuration. The assets must be the same for every configuration.

set(ASSETS assets)
set(ROMS ${ASSETS}/roms)
set(FONTS ${ASSETS}/fonts)
set(SAMPLES ${ASSETS}/samples)

# BBC ROMs
add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${ROMS}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different OS12.ROM B+MOS.rom BASIC2.ROM acorn/DFS-2.26.rom watford/DDFS-1.54T.rom watford/DDFS-1.53.rom opus/OPUS-DDOS-3.45.rom opus/challenger-1.01.rom $<TARGET_FILE_DIR:b2>/${ROMS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../etc/roms
  VERBATIM)

# Master ROMs
function(do_master_ROMs VERSION)
  add_custom_command(
    TARGET b2 POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${ROMS}/m128/${VERSION}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different adfs.rom basic4.rom dfs.rom edit.rom mos.rom terminal.rom view.rom viewsht.rom $<TARGET_FILE_DIR:b2>/${ROMS}/m128/${VERSION}/
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../etc/roms/M128/${VERSION}
	VERBATIM)
endfunction()

do_master_ROMs(3.20)
do_master_ROMs(3.50)

# Fonts

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${FONTS}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different fontawesome-webfont.ttf $<TARGET_FILE_DIR:b2>/${FONTS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../submodules/Font-Awesome/fonts
  VERBATIM)

# Floppy disc sounds

file(GLOB FLOPPY_SAMPLES ${CMAKE_CURRENT_LIST_DIR}/../../etc/samples_floppy/*.wav)

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${SAMPLES}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FLOPPY_SAMPLES} $<TARGET_FILE_DIR:b2>/${SAMPLES}
  VERBATIM)

##########################################################################
##########################################################################

# Job queue unit tests.

add_executable(test_JobQueue
  test_JobQueue.cpp
  JobQueue.cpp JobQueue.h
  )
target_link_libraries(test_JobQueue PRIVATE shared_lib)
add_test(
  NAME b2/test_JobQueue
  COMMAND $<TARGET_FILE:test_JobQueue>)

##########################################################################
##########################################################################
