 .PHONY:default
default:
	$(error Must specify target)

CMAKE_DEFINES:=

ifdef RELEASE_MODE
ifdef RELEASE_NAME
CMAKE_DEFINES:=$(CMAKE_DEFINES) -DRELEASE_NAME=$(RELEASE_NAME)
endif
endif

BUILD_FOLDER:=build

ifeq ($(OS),Windows_NT)

PYTHON3:=py -3

# https://github.com/muttleyxd/clang-tools-static-binaries/releases
CLANG_FORMAT:=bin/clang-format-11_windows-amd64.exe

CAT:=cmd /c type

NPROC:=$(NUMBER_OF_PROCESSORS)

include Makefile.windows.mak

else

# Is this how you're supposed to do it? Works for me on macOS, at
# least...
PYTHON3:=python3


CAT:=cat

UNAME:=$(shell uname -s)

ifeq ($(UNAME),Darwin)
OS:=osx
NPROC:=$(shell sysctl -n hw.ncpu)
INSTALLER:=
ifdef OSX_DEPLOYMENT_TARGET
CMAKE_DEFINES:=$(CMAKE_DEFINES) -DCMAKE_OSX_DEPLOYMENT_TARGET=$(OSX_DEPLOYMENT_TARGET)
endif
# version number roulette.
CLANG_FORMAT:=clang-format

include Makefile.unix.mak
include Makefile.osx.mak

endif

ifeq ($(UNAME),Linux)
OS:=linux
NPROC:=$(shell nproc)
INSTALLER:=1

# clang-format-16 = newest version that doesn't behave meaningfully
# differently from the clang-format that comes with Visual Studio
# 2019.
CLANG_FORMAT:=clang-format-16

include Makefile.unix.mak
endif

endif

SHELLCMD:=$(PYTHON3) ./submodules/shellcmd.py/shellcmd.py

##########################################################################
##########################################################################

.PHONY:rel
rel:
	$(PYTHON3) ./etc/release/release.py --make=$(MAKE)

##########################################################################
##########################################################################

.PHONY:rel_tests
rel_tests:
	$(PYTHON3) ./etc/b2_tests/rel_tests.py

##########################################################################
##########################################################################

.PHONY: clang-format
clang-format:
	$(SHELLCMD) cat experimental/clang-format.header.txt src/.clang-format experimental/clang-format.header.txt > experimental/.clang-format

	$(SHELLCMD) mkdir $(BUILD_FOLDER)
	$(PYTHON3) ./bin/make_clang-format_makefile.py -o "$(BUILD_FOLDER)/clang-format.mak" -e "$(CLANG_FORMAT)" --ignore "src/beeb/generated/*" src experimental submodules/shared_lib
	$(MAKE) -f "$(BUILD_FOLDER)/clang-format.mak" -j $(NPROC)

##########################################################################
##########################################################################

.PHONY: set_submodule_upstreams
set_submodule_upstreams:
	@$(MAKE) _set_submodule_upstream SUBMODULE=Remotery UPSTREAM=https://github.com/Celtoys/Remotery
	@$(MAKE) _set_submodule_upstream SUBMODULE=SDL_official UPSTREAM=https://github.com/libsdl-org/SDL
	@$(MAKE) _set_submodule_upstream SUBMODULE=curl UPSTREAM=https://github.com/curl/curl
	@$(MAKE) _set_submodule_upstream SUBMODULE=http-parser UPSTREAM=https://github.com/nodejs/http-parser
	@$(MAKE) _set_submodule_upstream SUBMODULE=imgui UPSTREAM=https://github.com/ocornut/imgui
	@$(MAKE) _set_submodule_upstream SUBMODULE=imgui_club UPSTREAM=https://github.com/ocornut/imgui_club
	@$(MAKE) _set_submodule_upstream SUBMODULE=libuv UPSTREAM=https://github.com/libuv/libuv
	@$(MAKE) _set_submodule_upstream SUBMODULE=macdylibbundler UPSTREAM=https://github.com/auriamg/macdylibbundler
	@$(MAKE) _set_submodule_upstream SUBMODULE=perfect6502 UPSTREAM=https://github.com/mist64/perfect6502
	@$(MAKE) _set_submodule_upstream SUBMODULE=rapidjson UPSTREAM=https://github.com/Tencent/rapidjson
	@$(MAKE) _set_submodule_upstream SUBMODULE=relacy UPSTREAM=https://github.com/dvyukov/relacy
	@$(MAKE) _set_submodule_upstream SUBMODULE=salieri UPSTREAM=https://github.com/nemequ/salieri
	@$(MAKE) _set_submodule_upstream SUBMODULE=visual6502 UPSTREAM=https://github.com/trebonian/visual6502

.PHONY:_set_submodule_upstream
_set_submodule_upstream: SUBMODULE=$(error must supply SUBMODULE)
_set_submodule_upstream: UPSTREAM=$(error must supply UPSTREAM)
_set_submodule_upstream:
	-cd "submodules/$(SUBMODULE)" && git remote remove upstream
	cd "submodules/$(SUBMODULE)" && git remote add upstream "$(UPSTREAM)"

##########################################################################
##########################################################################

.PHONY: backup_b2_config
backup_b2_config: B2_JSON_FOLDER?=$(error B2_JSON_FOLDER not set!)
backup_b2_config: _TIMESTAMP:=$(shell $(SHELLCMD) strftime --UTC -d _ _Y_m_dT_H_M_SZ)
backup_b2_config: _DEST:=$(BUILD_FOLDER)/configs/$(_TIMESTAMP)$(SUFFIX)
backup_b2_config:
	$(SHELLCMD) mkdir "$(_DEST)"
	$(SHELLCMD) copy-file "$(B2_JSON_FOLDER)/b2.json" "$(_DEST)/b2.json"
	-$(SHELLCMD) copy-file "$(B2_JSON_FOLDER)/imgui.ini" "$(_DEST)/"
	-$(SHELLCMD) copy-file "$(B2_JSON_FOLDER)/imgui.1.92+.ini" "$(_DEST)/"
# copy/paste fodder
	$(SHELLCMD) realpath "$(B2_JSON_FOLDER)"
	$(SHELLCMD) realpath "$(_DEST)"

##########################################################################
##########################################################################

ifeq ($(OS),Windows_NT)
MAGICK:=./etc/ImageMagick-7.0.5-4-portable-Q16-x64/magick.exe
else
MAGICK:=magick
endif

ICON_FOLDER:=./etc/icon
ICON_PNG:=$(ICON_FOLDER)/icon.png
ICON_DEST_WINDOWS:=$(ICON_FOLDER)/windows
ICON_DEST_MACOS:=$(ICON_FOLDER)/macos/b2.iconset

.PHONY:icons
icons: _ICON_TEMP:=$(BUILD_FOLDER)/icons
icons:
	$(SHELLCMD) rm-tree "$(_ICON_TEMP)"
	$(SHELLCMD) mkdir "$(_ICON_TEMP)"
	$(SHELLCMD) mkdir "$(ICON_DEST_WINDOWS)"
	$(SHELLCMD) mkdir "$(ICON_DEST_MACOS)"

# Windows
	"${MAGICK}" "${ICON_PNG}" -resize 256x256 "$(_ICON_TEMP)/icon_256x256_32bpp.png"
	"${MAGICK}" "${ICON_PNG}" -resize 16x16 "$(_ICON_TEMP)/icon_16x16_32bpp.png"
	"${MAGICK}" "${ICON_PNG}" -resize 24x24 "$(_ICON_TEMP)/icon_24x24_32bpp.png"
	"${MAGICK}" "${ICON_PNG}" -resize 32x32 "$(_ICON_TEMP)/icon_32x32_32bpp.png"
	"${MAGICK}" "${ICON_PNG}" -resize 48x48 "$(_ICON_TEMP)/icon_48x48_32bpp.png"
	"${MAGICK}" "${ICON_PNG}" -resize 64x64 "$(_ICON_TEMP)/icon_64x64_32bpp.png"
	"${MAGICK}" \
"$(_ICON_TEMP)/icon_256x256_32bpp.png" \
"$(_ICON_TEMP)/icon_64x64_32bpp.png" \
"$(_ICON_TEMP)/icon_48x48_32bpp.png" \
"$(_ICON_TEMP)/icon_32x32_32bpp.png" \
"$(_ICON_TEMP)/icon_24x24_32bpp.png" \
"$(_ICON_TEMP)/icon_16x16_32bpp.png" \
"$(ICON_DEST_WINDOWS)/b2_icons.ico"

# macOS
#
# If updating this list, update the dependencies in
# src/b2/CMakeLists.txt as well.
	$(MAKE) _icon_macos SIZE=16
	$(MAKE) _icon_macos SIZE=32
	$(MAKE) _icon_macos SIZE=64
	$(MAKE) _icon_macos SIZE=128
	$(MAKE) _icon_macos SIZE=256
	$(MAKE) _icon_macos SIZE=512
	$(MAKE) _icon_macos SIZE=1024
	$(MAKE) _icon_macos_x2 SRC=32 DEST=16
	$(MAKE) _icon_macos_x2 SRC=64 DEST=32
	$(MAKE) _icon_macos_x2 SRC=128 DEST=64
	$(MAKE) _icon_macos_x2 SRC=256 DEST=128

.PHONY:_icon_macos
_icon_macos: SIZE=$(error Must specify SIZE)
_icon_macos:
	"$(MAGICK)" "$(ICON_PNG)" -resize $(SIZE)x$(SIZE) "$(ICON_DEST_MACOS)/icon_$(SIZE)x$(SIZE).png"

.PHONY:_icon_macos_x2
_icon_macos_x2: SRC=$(error Must specify SRC)
_icon_macos_x2: DEST=$(error Must specify DEST)
_icon_macos_x2:
	$(SHELLCMD) copy-file "$(ICON_DEST_MACOS)/icon_$(SRC)x$(SRC).png" "$(ICON_DEST_MACOS)/icon_$(DEST)x$(DEST)@2x.png"

##########################################################################
##########################################################################

.PHONY:update_mfns
update_mfns: NUM_GROUPS:=16
update_mfns:
	$(PYTHON3) "src/beeb/bin/make_update_fn_stuff.py" -n "$(NUM_GROUPS)" .
