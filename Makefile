.PHONY:default
default:
	$(error Must specify target. One of: init, rel)

INCLUDE_EXPERIMENTAL?=ON

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
