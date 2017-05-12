ifeq ($(OS),Windows_NT)

include Makefile.windows

else

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
