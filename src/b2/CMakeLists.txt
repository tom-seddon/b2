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
  GenerateThumbnailJob.cpp GenerateThumbnailJob.h
  HTTPMethodsHandler.cpp HTTPMethodsHandler.h
  JobQueue.cpp JobQueue.h
  KeymapsUI.cpp KeymapsUI.h
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
  VideoWriterTGA.cpp VideoWriterTGA.h
  WriteVideoJob.cpp WriteVideoJob.h
  b2.cpp b2.h b2.inl
  commands.cpp commands.h
  conf.cpp conf.h
  dear_imgui.cpp dear_imgui.h dear_imgui.inl
  debugger.cpp debugger.h
  discs.cpp discs.h
  keymap.cpp keymap.h
  keys.cpp keys.h keys.inl
  load_save.cpp load_save.h
  load_save_config_rapidjson.cpp load_save_config_rapidjson.h
  load_save_config_nlohmann_json.cpp load_save_config_nlohmann_json.h
  misc.cpp misc.h misc.inl
  native_ui.cpp native_ui.h
  roms.cpp roms.h
  profiler.cpp profiler.h
  joysticks.cpp joysticks.h
  LoadMemoryDiscImage.cpp LoadMemoryDiscImage.h
  )

if(OSX)
  target_sources(b2 PRIVATE load_save.mm)
endif()

##########################################################################
##########################################################################

b2_target_boilerplate(b2 SANITIZERS)

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
  set(ICONS_MACOS ${CMAKE_SOURCE_DIR})
  set(ICONSET ${CMAKE_SOURCE_DIR}/etc/icon/macos/b2.iconset)

  # https://developer.apple.com/library/content/documentation/UserExperience/Conceptual/OSXHIGuidelines/Designing.html#//apple_ref/doc/uid/20000957-CH87-SW1

  # The icons could be different per configuration. The Final build
  # icon is the one used for the DMG.
  add_custom_command(
    OUTPUT b2.icns
    COMMAND iconutil -c icns -o b2.icns ${ICONSET}
    DEPENDS ${ICONSET}/icon_1024x1024.png ${ICONSET}/icon_128x128.png ${ICONSET}/icon_128x128@2x.png ${ICONSET}/icon_16x16.png ${ICONSET}/icon_16x16@2x.png ${ICONSET}/icon_256x256.png ${ICONSET}/icon_32x32.png ${ICONSET}/icon_32x32@2x.png ${ICONSET}/icon_512x512.png ${ICONSET}/icon_64x64.png ${ICONSET}/icon_64x64@2x.png)

  set(BUNDLE_IDENTIFIER "com.tom-seddon.b2")

  target_compile_definitions(b2 PRIVATE
    MACOS_DEFAULT_BUNDLE_IDENTIFIER="${BUNDLE_IDENTIFIER}")
  
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
  
  set(ICON_ICO ${CMAKE_SOURCE_DIR}/etc/icon/windows/b2_icons.ico)
  
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
  nlohmann_json::nlohmann_json
  http_lib
  )

##########################################################################
##########################################################################

find_package(OpenGL REQUIRED)
target_link_libraries(b2 PRIVATE ${OPENGL_LIBRARIES})

##########################################################################
##########################################################################

target_link_libraries(b2 PRIVATE ${LIBCURL_TARGET} ${SDL2_LIBRARY})

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
  COMMAND ${CMAKE_COMMAND} -E copy_if_different 80.ssd 80.dsd 40.ssd 40.dsd Welcome.ssd adl.adl adm.adm ads.ads MasterWelcome.adl CompactWelcome.adl PC128SWelcome.adl 10MB.dat 10MB.dsc $<TARGET_FILE_DIR:b2>/${DISCS}
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
b2_target_boilerplate(test_JobQueue SANITIZERS)
target_link_libraries(test_JobQueue PRIVATE shared_lib)
add_test(
  NAME b2/test_JobQueue
  COMMAND $<TARGET_FILE:test_JobQueue>)

##########################################################################
##########################################################################

# Don't add misc.cpp. There doesn't seem to be any way to set the
# HEADER_FILE_ONLY source file property on a per-target basis.
add_executable(test_UTF8
  test_UTF8.cpp
  misc.inl misc.h
  Messages.cpp Messages.h
  )
b2_target_boilerplate(test_UTF8 SANITIZERS)
target_link_libraries(test_UTF8 PRIVATE shared_lib beeb_lib ${SDL2_LIBRARY})
add_test(
  NAME b2/test_UTF8
  COMMAND $<TARGET_FILE:test_UTF8>)

##########################################################################
##########################################################################

add_executable(b2client b2client.cpp)
b2_target_boilerplate(b2client,SANITIZERS)
target_link_libraries(b2client PRIVATE shared_lib http_lib nlohmann_json::nlohmann_json)

##########################################################################
##########################################################################
