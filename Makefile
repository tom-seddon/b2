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

# this is what it ends up called on Ubuntu 22, at any rate.
CLANG_FORMAT:=clang-format-11

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

.PHONY:clang-format
clang-format:
	$(SHELLCMD) cat experimental/clang-format.header.txt src/.clang-format experimental/clang-format.header.txt > experimental/.clang-format

	$(SHELLCMD) mkdir $(BUILD_FOLDER)
	$(PYTHON3) ./bin/make_clang-format_makefile.py -o "$(BUILD_FOLDER)/clang-format.mak" -e "$(CLANG_FORMAT)" src experimental
	$(MAKE) -f "$(BUILD_FOLDER)/clang-format.mak" -j $(NPROC)

##########################################################################
##########################################################################

.PHONY:set_submodule_upstreams
set_submodule_upstreams:
	@$(MAKE) _set_submodule_upstream SUBMODULE=Remotery UPSTREAM=https://github.com/Celtoys/Remotery
	@$(MAKE) _set_submodule_upstream SUBMODULE=SDL_official UPSTREAM=https://github.com/libsdl-org/SDL
	@$(MAKE) _set_submodule_upstream SUBMODULE=curl UPSTREAM=https://github.com/curl/curl
	@$(MAKE) _set_submodule_upstream SUBMODULE=http-parser UPSTREAM=https://github.com/nodejs/http-parser
	@$(MAKE) _set_submodule_upstream SUBMODULE=imgui UPSTREAM=https://github.com/ocornut/imgui
	@$(MAKE) _set_submodule_upstream SUBMODULE=imgui_club UPSTREAM=https://github.com/tom-seddon/imgui_club
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
