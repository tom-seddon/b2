.PHONY:default
default:
	$(error Must specify target. One of: init, rel)

ifdef RELEASE_MODE
INCLUDE_EXPERIMENTAL:=OFF
else
INCLUDE_EXPERIMENTAL:=ON
endif

BUILD_FOLDER:=build

ifeq ($(OS),Windows_NT)

PYTHON:=cmd /c python.exe
include Makefile.windows

else

PYTHON:=/usr/bin/python
UNAME:=$(shell uname -s)

ifeq ($(UNAME),Darwin)
OS:=osx
NPROC:=sysctl -n hw.ncpu
include Makefile.unix

.PHONY:init_xcode
init_xcode: _FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)Xcode
init_xcode:
	rm -Rf "$(_FOLDER)"
	mkdir -p "$(_FOLDER)"
	cd "$(_FOLDER)" && cmake -G Xcode -DINCLUDE_EXPERIMENTAL=$(INCLUDE_EXPERIMENTAL) ../..

endif

ifeq ($(UNAME),Linux)
OS:=linux
NPROC:=nproc
include Makefile.unix
endif

endif

.PHONY:rel
rel:
	$(PYTHON) ./etc/release/release.py --make=$(MAKE)
