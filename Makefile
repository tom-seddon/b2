.PHONY:default
default:
	$(error Must specify target. One of: init, rel)

ifdef RELEASE_MODE
INCLUDE_EXPERIMENTAL:=OFF
LIBUV_BUILD_TESTS:=OFF
else
INCLUDE_EXPERIMENTAL:=ON
LIBUV_BUILD_TESTS:=ON
endif

CMAKE_DEFINES:=-DINCLUDE_EXPERIMENTAL=$(INCLUDE_EXPERIMENTAL) -DLIBUV_BUILD_TESTS=$(LIBUV_BUILD_TESTS)

ifdef RELEASE_MODE
ifdef RELEASE_NAME
CMAKE_DEFINES:=$(CMAKE_DEFINES) -DRELEASE_NAME=$(RELEASE_NAME)
endif
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
include Makefile.osx

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
