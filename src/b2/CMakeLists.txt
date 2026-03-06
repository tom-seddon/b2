##########################################################################
##########################################################################

add_library(b2_lib STATIC
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
  KeymapsUI.cpp KeymapsUI.h KeymapsUI_private.inl
  MessageQueue.cpp MessageQueue.h
  Messages.cpp Messages.h Messages.inl
  MessagesUI.cpp MessagesUI.h
  Remapper.cpp Remapper.h
  SavedStatesUI.cpp SavedStatesUI.h
  SettingsUI.cpp SettingsUI.h
  SymbolTable.cpp SymbolTable.h SymbolTable.inl
  ThumbnailsUI.cpp ThumbnailsUI.h ThumbnailsUI_private.inl
  TimelineUI.cpp TimelineUI.h
  TraceUI.cpp TraceUI.h TraceUI.inl
  VBlankMonitor.cpp VBlankMonitor.h
  VBlankMonitorDefault.cpp VBlankMonitorDefault.h
  VideoWriter.cpp VideoWriter.h
  VideoWriterTGA.cpp VideoWriterTGA.h
  WriteVideoJob.cpp WriteVideoJob.h
  b2.cpp b2.h b2_private.inl
  commands.cpp commands.h
  conf.cpp conf.h
  dear_imgui.cpp dear_imgui.h dear_imgui.inl dear_imgui.imconfig.h
  debugger.cpp debugger.h
  discs.cpp discs.h
  keymap.cpp keymap.h
  keys.cpp keys.h keys.inl
  load_save.cpp load_save.h
  load_save_config_rapidjson.cpp load_save_config_rapidjson.h
  load_save_config_nlohmann_json.cpp load_save_config_nlohmann_json.h
  misc.cpp misc.h misc.inl
  native_ui.cpp native_ui.h native_ui_private.h native_ui_private.cpp native_ui_private.inl
  roms.cpp roms.h
  profiler.cpp profiler.h profiler.inl
  joysticks.cpp joysticks.h
  LoadMemoryDiscImage.cpp LoadMemoryDiscImage.h
  json.h json.cpp
  miniz.cpp
  )

if(OSX)
  target_sources(b2_lib PRIVATE load_save.mm)
endif()

if(WIN32)
  target_sources(b2_lib PRIVATE guids_win32.c)
  target_sources(b2_lib PRIVATE b2.natvis)
endif()

##########################################################################
##########################################################################

b2_target_boilerplate(b2_lib SANITIZERS)

##########################################################################
##########################################################################

if(NOT DEFINED RELEASE_NAME)
  set(RELEASE_NAME local_build)
endif()

target_compile_definitions(b2_lib PUBLIC RELEASE_NAME=${RELEASE_NAME})

if(HAVE_SDL_SOFTSTRETCHLINEAR)
  target_compile_definitions(b2_lib PRIVATE HAVE_SDL_SOFTSTRETCHLINEAR=1)
endif()

##########################################################################
##########################################################################

# since Dear ImGui is an interface library, this'll apply to b2's
# copy when built and used.
target_compile_definitions(b2_lib PUBLIC IMGUI_USER_CONFIG="${CMAKE_CURRENT_LIST_DIR}/dear_imgui.imconfig.h")

# imgui_te_imconfig.h defines this, but having it specified externally
# affords additional options: headers that don't themselves include
# imgui headers can still test for it, and the custom imgui user
# config file can skip including imgui_te_imconfig.h if it isn't
# defined, to simplify disabling it.
target_compile_definitions(b2_lib PUBLIC IMGUI_ENABLE_TEST_ENGINE)

target_link_libraries(b2_lib PUBLIC
  imgui_lib
  imgui_test_engine_lib)

##########################################################################
##########################################################################

# Windows-specific general stuff.

if(APPLE)
  # Default bundle identifier for b2, used to form paths for config
  # files.
  #
  # (The release process may change the actual bundle identifier, so
  # that debug and non-debug versions can coexist sensibly, so this
  # value is hardcoded rather than being picked up at runtime.)
  set(BUNDLE_IDENTIFIER "com.tom-seddon.b2")

  # TODO: this needs sorting out properly...
  target_compile_definitions(b2_lib PUBLIC
    MACOS_BUNDLE_IDENTIFIER="${BUNDLE_IDENTIFIER}")
endif()

##########################################################################
##########################################################################

# Windows-specific general stuff.

if(WIN32)
  target_link_libraries(b2_lib PRIVATE pix_lib)
  target_link_libraries(b2_lib PRIVATE Dwmapi.lib)
endif()

##########################################################################
##########################################################################

# Linux-specific general stuff.

if(UNIX)
  target_link_libraries(b2_lib PRIVATE m)
endif()

##########################################################################
##########################################################################

# VBlank monitor.

if(WIN32)
  target_sources(b2_lib PRIVATE VBlankMonitorWindows.cpp VBlankMonitorWindows.h)
  target_link_libraries(b2_lib PRIVATE dxgi.lib)
elseif(OSX)
  target_sources(b2_lib PRIVATE VBlankMonitorOSX.cpp VBlankMonitorOSX.h)
  target_link_libraries(b2_lib PRIVATE "-framework QuartzCore")
  target_link_libraries(b2_lib PRIVATE "-framework CoreVideo")
endif()  

##########################################################################
##########################################################################

# Native UI stuff.

if(OSX)
  target_sources(b2_lib PRIVATE native_ui_osx.mm native_ui_osx.h)
elseif(WIN32)
  target_sources(b2_lib PRIVATE
    native_ui_windows.cpp native_ui_windows.h)
elseif(LINUX)
  target_compile_definitions(b2_lib PRIVATE ${GTK4_DEFINITIONS})
  target_include_directories(b2_lib PRIVATE ${GTK4_INCLUDE_DIRS})
  target_link_libraries(b2_lib PRIVATE ${GTK4_LIBRARIES})
  target_sources(b2_lib PRIVATE native_ui_gtk.cpp native_ui_gtk.h)
endif()

##########################################################################
##########################################################################

# Media Foundation video writer.

if(WIN32)
  target_sources(b2_lib PRIVATE VideoWriterMF.cpp VideoWriterMF.h VideoWriterMF_private.inl)
  target_link_libraries(b2_lib PRIVATE mf.lib mfplat.lib mfreadwrite.lib)
endif()

##########################################################################
##########################################################################

# FFmpeg video writer.

if(FFMPEG_FOUND)
  target_sources(b2_lib PRIVATE VideoWriterFFmpeg.cpp VideoWriterFFmpeg.h)
  target_link_libraries(b2_lib PRIVATE ${FFMPEG_LINK_LIBRARIES})
  target_include_directories(b2_lib PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_compile_definitions(b2_lib PRIVATE ${FFMPEG_DEFINITIONS} -DHAVE_FFMPEG=1)
endif()

##########################################################################
##########################################################################

# AVFoundation video writer.
if(OSX)
  target_sources(b2_lib PRIVATE VideoWriterAVFoundation.mm VideoWriterAVFoundation.h)
endif()

##########################################################################
##########################################################################

target_link_libraries(b2_lib PUBLIC
  beeb_lib
  shared_lib
  6502_lib
  miniz_lib
  remotery_lib
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
target_link_libraries(b2_lib PRIVATE ${OPENGL_LIBRARIES})

##########################################################################
##########################################################################

target_link_libraries(b2_lib PUBLIC ${SDL2_LIBRARY})

##########################################################################
##########################################################################

add_executable(b2 MACOSX_BUNDLE
  b2_app.cpp)
target_link_libraries(b2 PRIVATE b2_lib)
b2_target_boilerplate(b2 SANITIZERS)

if(WIN32)
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

# Apple-specific app stuff.

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

# Windows-specific app stuff.

if(WIN32)
  # The debug and RelWithDebInfo versions want to be console apps.
  # 
  # http://stackoverflow.com/questions/8054734/cmake-how-to-use-different-add-executable-for-debug-build
  # http://public.kitware.com/Bug/view.php?id=11171
  # http://public.kitware.com/Bug/view.php?id=12566
  #
  # TL;DR - bleargh. So this is done with a #pragma in b2.cpp.

  add_dependencies(b2 rcedit)
  
  set(ICON_ICO ${CMAKE_SOURCE_DIR}/etc/icon/windows/b2_icons.ico)
  
  add_custom_command(TARGET b2 POST_BUILD
    COMMAND $<TARGET_FILE:rcedit> $<TARGET_FILE:b2> --set-icon ${ICON_ICO} --set-version-string ProductVersion ${RELEASE_NAME})
  
  target_sources(b2 PRIVATE ${ICON_ICO})

  # # /INCREMENTAL:NO - http://stackoverflow.com/questions/21569478
  # #
  # # /OPT:REF /OPT:ICF - no idea why these have gone missing. Aren't
  # # they supposed to be the defaults?? (This would happen in VC6
  # # sometimes too, but I can't remember why... anyway, specifying them
  # # explicitly was always the solution then too.)
  # set(EXTRA_LINKER_FLAGS "/INCREMENTAL:NO /OPT:REF /OPT:ICF")
  # set_target_properties(b2_lib PROPERTIES LINK_FLAGS_RELWITHDEBINFO ${EXTRA_LINKER_FLAGS})
  # set_target_properties(b2_lib PROPERTIES LINK_FLAGS_FINAL ${EXTRA_LINKER_FLAGS})

  # The debug and RelWithDebInfo versions want to be console apps.
  # 
  # http://stackoverflow.com/questions/8054734/cmake-how-to-use-different-add-executable-for-debug-build
  # http://public.kitware.com/Bug/view.php?id=11171
  # http://public.kitware.com/Bug/view.php?id=12566
  #
  # TL;DR - bleargh. So this is done with a #pragma in b2.cpp.

  add_copy_pix_dlls_command(b2)
endif()

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
  COMMAND ${CMAKE_COMMAND} -E copy_if_different OS12.ROM B+MOS.rom BASIC2.ROM acorn/DFS-2.26.rom watford/DDFS-1.54T.rom watford/DDFS-1.53.rom opus/OPUS-DDOS-3.45.rom opus/challenger-1.01.rom MasterTurboParasite.rom TUBE110.rom ElectronMOS.rom $<TARGET_FILE_DIR:b2>/${ROMS}
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
do_compact_ROMs(5.11i)

# Additional MOS 5.11i ROMs
add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different international.rom arabic.rom $<TARGET_FILE_DIR:b2>/${ROMS}/mcompact/5.11i/
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../etc/roms/MCompact/5.11i)

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
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${FONTS})

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different fa-solid-900.ttf $<TARGET_FILE_DIR:b2>/${FONTS}
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../submodules/Font-Awesome/web-fonts-with-css/webfonts
  VERBATIM)

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_LIST_DIR}/../../etc/liberation-fonts-ttf-2.1.5/LiberationMono-Regular.ttf $<TARGET_FILE_DIR:b2>/${FONTS}
  VERBATIM)

# Floppy disc sounds

file(GLOB FLOPPY_SAMPLES ${CMAKE_CURRENT_LIST_DIR}/../../etc/samples_floppy/*.wav)

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:b2>/${SAMPLES}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FLOPPY_SAMPLES} $<TARGET_FILE_DIR:b2>/${SAMPLES}
  VERBATIM)

# Add the licence to the macOS app bundle

if(OSX)
  add_custom_command(
    TARGET b2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_LIST_DIR}/../../etc/release/LICENCE.txt ../Resources
    VERBATIM)
endif()

# Other stuff

add_custom_command(
  TARGET b2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_LIST_DIR}/../../submodules/SDL_GameControllerDB/gamecontrollerdb.txt $<TARGET_FILE_DIR:b2>/${ASSETS}
  VERBATIM)

##########################################################################
##########################################################################

add_executable(b2client b2client.cpp)
b2_target_boilerplate(b2client SANITIZERS)
target_link_libraries(b2client PRIVATE shared_lib http_lib nlohmann_json::nlohmann_json)

##########################################################################
##########################################################################

add_executable(b2_test
  b2_test.cpp)
# Load assets from wherever b2 was built to. 
target_compile_definitions(b2_test PRIVATE ASSETS_FOLDER="$<TARGET_FILE_DIR:b2>")
# Stick config stuff in some transient place.
target_compile_definitions(b2_test PRIVATE TRANSIENT_DATA_FOLDER="${CMAKE_BINARY_DIR}/b2_test_data.$<CONFIG>")
# Also have b2_SOURCE_DIR, same as test_beeb.
target_compile_definitions(b2_test PRIVATE b2_SOURCE_DIR="${b2_SOURCE_DIR}")

b2_target_boilerplate(b2_test SANITIZERS)
target_link_libraries(b2_test PRIVATE b2_lib)

add_test(NAME b2/b2_test_list_tests
  COMMAND $<TARGET_FILE:b2_test> --list-for-check_ctest_log)

function(add_b2_test_literal_category test_name)
  add_test(NAME b2/${test_name}
    COMMAND $<TARGET_FILE:b2_test> -t "${test_name}")
endfunction()

add_b2_test_literal_category(misc.utf8)
add_b2_test_literal_category(b2.JobQueue)
add_b2_test_literal_category(b2.SymbolTable)
add_b2_test_literal_category(misc.stb_image_utf8)
add_b2_test_literal_category(misc.LoadPossiblyGzippedFile)
add_b2_test_literal_category(b2.LoadUEF)

foreach(DRIVE 0 1)
  foreach(MEM 0 1)
    foreach(DISK 80.ssd 80.dsd ads.ads adm.adm adl.adl Welcome.ssd MasterWelcome.adl CompactWelcome.adl PC128SWelcome.adl)
      add_b2_test_literal_category(b2ui.copy_of_disk.${DRIVE}.${MEM}.${DISK})
    endforeach()
  endforeach()
endforeach()

foreach(MODEL master compact)
  add_b2_test_literal_category(b2ui.nvram_update.${MODEL})
endforeach()

##########################################################################
##########################################################################

# TODO... none of this stuff quite works yet.

# add_test(
#   NAME b2/list_tests
#   COMMAND $<TARGET_FILE:b2> --imgui-list-tests)

# function(add_b2_integration_test test_name)
#   set(config_folder "${CMAKE_BINARY_DIR}/${test_name}")

#   add_test(
#     NAME b2/delete_config_folder/${test_name}
#     COMMAND ${CMAKE_COMMAND} -E rm -Rf "${config_folder}")
  
#   add_test(
#     NAME b2/${test_name}
#     COMMAND $<TARGET_FILE:b2> --headless --imgui-enable-test-engine "--config-folder=${config_folder}" --imgui-run-test=${test_name})

#   set_tests_properties(b2/delete_config_folder/${test_name}
#     PROPERTIES
#     FIXTURES_SETUP fixture_delete_config_folder/${test_name})
#   set_tests_properties(b2/${test_name}
#     PROPERTIES
#     FIXTURES_REQUIRED fixture_delete_config_folder/${test_name})
# endfunction()

# add_b2_integration_test(b2_tests.quit)
