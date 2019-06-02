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
