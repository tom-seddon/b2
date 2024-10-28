cmake_minimum_required(VERSION 3.5)

##########################################################################
##########################################################################

add_executable(b2 MACOSX_BUNDLE
  BeebConfig.cpp BeebConfig.h BeebConfig.inl
  BeebKeymap.cpp BeebKeymap.h
  BeebLinkHTTPHandler.cpp BeebLinkHTTPHandler.h
  BeebLinkUI.cpp BeebLinkUI.h
  BeebState.cpp BeebState.h
  BeebThread.cpp BeebThread.h BeebThread.inl BeebThread_private.inl
  BeebWindow.cpp BeebWindow.h BeebWindow.inl
  BeebWindows.cpp BeebWindows.h 
  CommandKeymapsUI.cpp CommandKeymapsUI.h
  ConfigsUI.cpp ConfigsUI.h ConfigsUI_private.inl
  DataRateUI.cpp DataRateUI.h
  DearImguiTestUI.cpp DearImguiTestUI.h
  DirectDiscImage.cpp DirectDiscImage.h
  DiscGeometry.cpp DiscGeometry.h
  GenerateThumbnailJob.cpp GenerateThumbnailJob.h
  HTTPMethodsHandler.cpp HTTPMethodsHandler.h
  HTTPServer.cpp HTTPServer.h
  JobQueue.cpp JobQueue.h
  KeymapsUI.cpp KeymapsUI.h
  MemoryDiscImage.cpp MemoryDiscImage.h
  MessageQueue.cpp MessageQueue.h
  Messages.cpp Messages.h
  MessagesUI.cpp MessagesUI.h
  Remapper.cpp Remapper.h
  SavedStatesUI.cpp SavedStatesUI.h
  SettingsUI.cpp SettingsUI.h
  ThumbnailsUI.cpp ThumbnailsUI.h ThumbnailsUI_private.inl
  TimelineUI.cpp TimelineUI.h
  TraceUI.cpp TraceUI.h TraceUI.inl
  VBlankMonitor.cpp VBlankMonitor.h
  VBlankMonitorDefault.cpp VBlankMonitorDefault.h
  VideoWriter.cpp VideoWriter.h
  WriteVideoJob.cpp WriteVideoJob.h
  b2.cpp b2.h b2.inl
  commands.cpp commands.h
  conf.cpp conf.h
  dear_imgui.cpp dear_imgui.h
  debugger.cpp debugger.h
  discs.cpp discs.h
  filters.cpp filters.h
  keymap.cpp keymap.h
  keys.cpp keys.h keys.inl
  load_save.cpp load_save.h load_save.inl
  misc.cpp misc.h misc.inl
  native_ui.cpp native_ui.h
  roms.cpp roms.h
  profiler.cpp profiler.h
  joysticks.cpp joysticks.h
  )

if(OSX)
  target_sources(b2 PRIVATE load_save.mm)
endif()

##########################################################################
##########################################################################

add_config_define(b2)

##########################################################################
##########################################################################

if(NOT DEFINED RELEASE_NAME)
  set(RELEASE_NAME local_build)
endif()

target_compile_definitions(b2 PRIVATE RELEASE_NAME=${RELEASE_NAME})

if(HAVE_SDL_SOFTSTRETCHLINEAR)
  target_compile_definitions(b2 PRIVATE HAVE_SDL_SOFTSTRETCHLINEAR=1)
endif()

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

  # The icons could be different per configuration. The Final build
  # icon is the one used for the DMG.
  add_custom_command(
    OUTPUT b2.icns ${ICONS_TMP}/icon_16x16.png ${ICONS_TMP}/icon_32x32.png ${ICONS_TMP}/icon_64x64.png ${ICONS_TMP}/icon_128x128.png ${ICONS_TMP}/icon_256x256.png ${ICONS_TMP}/icon_512x512.png ${ICONS_TMP}/icon_512x512@2x.png ${ICONS_TMP}/icon_16x16@2x.png ${ICONS_TMP}/icon_32x32@2x.png ${ICONS_TMP}/icon_128x128@2x.png ${ICONS_TMP}/icon_256x256@2x.png
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ICONS_TMP}
    COMMAND sips -z 16 16 ${ICON_PNG} --out ${ICONS_TMP}/icon_16x16.png
    COMMAND sips -z 32 32 ${ICON_PNG} --out ${ICONS_TMP}/icon_32x32.png
    COMMAND sips -z 64 64 ${ICON_PNG} --out ${ICONS_TMP}/icon_64x64.png
    COMMAND sips -z 128 128 ${ICON_PNG} --out ${ICONS_TMP}/icon_128x128.png
    COMMAND sips -z 256 256 ${ICON_PNG} --out ${ICONS_TMP}/icon_256x256.png
    COMMAND sips -z 512 512 ${ICON_PNG} --out ${ICONS_TMP}/icon_512x512.png
    COMMAND sips -z 1024 1024 ${ICON_PNG} --out ${ICONS_TMP}/icon_512x512@2x.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICONS_TMP}/icon_32x32.png ${ICONS_TMP}/icon_16x16@2x.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICONS_TMP}/icon_64x64.png ${ICONS_TMP}/icon_32x32@2x.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICONS_TMP}/icon_256x256.png ${ICONS_TMP}/icon_128x128@2x.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ICONS_TMP}/icon_512x512.png ${ICONS_TMP}/icon_256x256@2x.png
    COMMAND iconutil -c icns ${ICONS_TMP}
    DEPENDS ${ICON_PNG}
    MAIN_DEPENDENCY ${ICON_PNG}
    )

  set(BUNDLE_IDENTIFIER "com.tom-seddon.b2")
  
  set_target_properties(b2
    PROPERTIES
    MACOSX_BUNDLE_NAME "b2"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${BUNDLE_IDENTIFIER}"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_LIST_DIR}/macos/template.Info.plist
    MACOSX_BUNDLE_ICON_FILE b2.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${RELEASE_NAME}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${RELEASE_NAME}
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${BUNDLE_IDENTIFIER}")
  
  target_sources(b2 PRIVATE
    b2.icns)

  set_source_files_properties(b2.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
endif()

##########################################################################
##########################################################################

# Windows-specific general stuff.

if(WIN32)
  add_dependencies(b2 rcedit)
  
  set(MAGICK ${CMAKE_SOURCE_DIR}/etc/ImageMagick-7.0.5-4-portable-Q16-x64/magick.exe)
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
    COMMAND $<TARGET_FILE:rcedit> $<TARGET_FILE:b2> --set-icon ${ICON_ICO} --set-version-string ProductVersion ${RELEASE_NAME})
  
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

  # The debug and RelWithDebInfo versions want to be console apps.
  # 
  # http://stackoverflow.com/questions/8054734/cmake-how-to-use-different-add-executable-for-debug-build
  # http://public.kitware.com/Bug/view.php?id=11171
  # http://public.kitware.com/Bug/view.php?id=12566
  #
  # TL;DR - bleargh. So this is done with a #pragma in b2.cpp.

  target_link_libraries(b2 PRIVATE pix_lib)
  add_copy_pix_dlls_command(b2)

  target_link_libraries(b2 PRIVATE Dwmapi.lib)
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

# Native UI stuff.

if(OSX)
  target_sources(b2 PRIVATE native_ui_osx.mm native_ui_osx.h)
elseif(WIN32)
  target_sources(b2 PRIVATE
    native_ui_windows.cpp native_ui_windows.h)
elseif(LINUX)
  target_compile_definitions(b2 PRIVATE ${GTK2_DEFINITIONS})
  target_include_directories(b2 PRIVATE ${GTK2_INCLUDE_DIRS})
  target_link_libraries(b2 PRIVATE ${GTK2_LIBRARIES})
  target_sources(b2 PRIVATE native_ui_gtk.cpp native_ui_gtk.h)
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

if(FFMPEG_FOUND)
  target_sources(b2 PRIVATE VideoWriterFFmpeg.cpp VideoWriterFFmpeg.h)
  target_link_libraries(b2 PRIVATE ${FFMPEG_LINK_LIBRARIES})
  # target_link_directories(b2 PRIVATE ${FFMPEG_LIBRARY_DIRS})
  target_include_directories(b2 PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_compile_definitions(b2 PRIVATE ${FFMPEG_DEFINITIONS} -DHAVE_FFMPEG=1)
endif()

##########################################################################
##########################################################################

# AVFoundation video writer.
if(OSX)
  target_sources(b2 PRIVATE VideoWriterAVFoundation.mm VideoWriterAVFoundation.h)
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
  imgui_memory_editor_lib
  rapidjson_lib
  IconFontCppHeaders_lib
  dear_imgui_hex_editor_lib
  stb_image_lib
  )

##########################################################################
##########################################################################

find_package(OpenGL REQUIRED)
target_link_libraries(b2 PRIVATE ${OPENGL_LIBRARIES})

##########################################################################
##########################################################################

target_link_libraries(b2 PRIVATE ${LIBCURL_TARGET} ${LibUV_LIBRARIES} llhttp_static ${SDL2_LIBRARY})

##########################################################################
##########################################################################

# Post build step - copy assets.
#
# The release process copies the asset folder from an unspecified
# configuration. The assets must be the same for every configuration.

if(APPLE)
# Agh, set this up slightly wrongly originally...
set(ASSETS ../Resources/assets)
else()
set(ASSETS assets)
endif()

set(ROMS ${ASSETS}/roms)
set(DISCS ${ASSETS}/discs)
set(FONTS ${ASSETS}/fonts)
set(SAMPLES ${ASSETS}/samples)

# B/B+/General ROMs
add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${ROMS}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different OS12.ROM B+MOS.rom BASIC2.ROM acorn/DFS-2.26.rom watford/DDFS-1.54T.rom watford/DDFS-1.53.rom opus/OPUS-DDOS-3.45.rom opus/challenger-1.01.rom MasterTurboParasite.rom TUBE110.rom $<TARGET_FILE_DIR:b2>/${ROMS}
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

# Compact ROMs
function(do_compact_ROMs VERSION)
  add_custom_command(
    TARGET b2 POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${ROMS}/mcompact/${VERSION}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different adfs.rom basic4.rom utils.rom mos.rom $<TARGET_FILE_DIR:b2>/${ROMS}/mcompact/${VERSION}/
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../etc/roms/MCompact/${VERSION}
	VERBATIM)
endfunction()

do_compact_ROMs(5.00)
do_compact_ROMs(5.10)
do_compact_ROMs(I5.10C)

# Discs
add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${DISCS}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different 80.ssd 80.dsd 40.ssd 40.dsd Welcome.ssd adl.adl adm.adm ads.ads MasterWelcome.adl CompactWelcome.adl PC128SWelcome.adl $<TARGET_FILE_DIR:b2>/${DISCS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../etc/discs
  VERBATIM)

# Fonts

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${FONTS}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different fa-solid-900.ttf $<TARGET_FILE_DIR:b2>/${FONTS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../submodules/Font-Awesome/web-fonts-with-css/webfonts
  VERBATIM)

# Floppy disc sounds

file(GLOB FLOPPY_SAMPLES ${CMAKE_CURRENT_LIST_DIR}/../../etc/samples_floppy/*.wav)

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${SAMPLES}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FLOPPY_SAMPLES} $<TARGET_FILE_DIR:b2>/${SAMPLES}
  VERBATIM)

if(OSX)
  add_custom_command(
    TARGET b2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_LIST_DIR}/../../etc/release/LICENCE.txt ../Resources
    VERBATIM)
endif()

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_LIST_DIR}/../../submodules/SDL_GameControllerDB/gamecontrollerdb.txt $<TARGET_FILE_DIR:b2>/${ASSETS}
  VERBATIM)

##########################################################################
##########################################################################

# Job queue unit tests.

add_executable(test_JobQueue
  test_JobQueue.cpp
  JobQueue.cpp JobQueue.h
  )
add_sanitizers(test_JobQueue)
target_link_libraries(test_JobQueue PRIVATE shared_lib)
add_test(
  NAME b2/test_JobQueue
  COMMAND $<TARGET_FILE:test_JobQueue>)

##########################################################################
##########################################################################

# HTTP unit tests.

add_executable(test_http
  test_http.cpp
  HTTPServer.cpp HTTPServer.h
  Messages.cpp Messages.h
  )
add_sanitizers(test_http)
target_link_libraries(test_http PRIVATE shared_lib ${LibUV_LIBRARIES} llhttp_static ${LIBCURL_TARGET})
add_test(
  NAME b2/test_http
  COMMAND $<TARGET_FILE:test_http>)

##########################################################################
##########################################################################

# Don't add misc.cpp. There doesn't seem to be any way to set the
# HEADER_FILE_ONLY source file property on a per-target basis.
add_executable(test_UTF8
  test_UTF8.cpp
  misc.inl misc.h
  Messages.cpp Messages.h
  )
add_sanitizers(test_UTF8)
add_config_define(test_UTF8)
target_link_libraries(test_UTF8 PRIVATE shared_lib beeb_lib ${SDL2_LIBRARY})
add_test(
  NAME b2/test_UTF8
  COMMAND $<TARGET_FILE:test_UTF8>)
