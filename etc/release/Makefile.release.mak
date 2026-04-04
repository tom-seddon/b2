MAKEFLAGS+=--no-print-directory

##########################################################################
##########################################################################

ifeq ($(OS),Windows_NT)
$(error Source distributions are not intended for use on Windows)
else
CMAKE_CONFIGURE_OPTIONS:=-G Ninja
UNAME:=$(shell uname -s)
ifeq ($(UNAME),Darwin)
# Source distributions are not intended for use on macOS either, but
# it hangs together just enough to let me use it for testing purposes.
OS:=osx
NPROC:=$(shell sysctl -n hw.ncpu) #older ctest has poor -j default

# work around some CMake versions choosing to build against the latest
# macOS SDK available, rather than the latest one the local system can
# run.
CMAKE_CONFIGURE_OPTIONS:=$(CMAKE_CONFIGURE_OPTIONS) -DCMAKE_OSX_DEPLOYMENT_TARGET=$(shell sw_vers -productVersion)
else
# By a process of elimination...
OS:=linux
NPROC:=$(shell nproc)		#older ctest has poor -j default
endif
endif

##########################################################################
##########################################################################

PYTHON:=/usr/bin/python3
_V:=$(if $(VERBOSE),,@)
__VERBOSE:=$(if $(VERBOSE),--verbose,)
BUILD:=build

##########################################################################
##########################################################################

B2_WITH_DEBUGGER?=1
ifeq ($(B2_WITH_DEBUGGER),1)
_B2_WITH_DEBUGGER:=1
else
ifeq ($(B2_WITH_DEBUGGER),0)
_B2_WITH_DEBUGGER:=
else
$(error Invalid B2_WITH_DEBUGGER setting)
endif
endif

B2_STANDARD?=1
ifeq ($(B2_STANDARD),1)
_B2_STANDARD:=1
else
ifeq ($(B2_STANDARD),0)
_B2_STANDARD:=
else
$(error Invalid B2_STANDARD setting)
endif
endif

RUN_TESTS?=1
ifeq ($(RUN_TESTS),1)
_RUN_TESTS:=1
else
ifeq ($(RUN_TESTS),0)
_RUN_TESTS:=
else
$(error Invalid RUN_TESTS setting)
endif
endif

##########################################################################
##########################################################################

export VERBOSE
export NPROC
export RUN_TESTS

##########################################################################
##########################################################################

.PHONY:default
default:
	$(error Specify target)

##########################################################################
##########################################################################

.PHONY:configure
configure:
	$(if $(_B2_WITH_DEBUGGER),$(_V)cmake -S "." -B "build/r.$(OS)" $(CMAKE_CONFIGURE_OPTIONS) -DCMAKE_BUILD_TYPE=RelWithDebInfo,rm -Rf "build/r.$(OS)")
	$(if $(_B2_STANDARD),$(_V)cmake -S "." -B "build/f.$(OS)" $(CMAKE_CONFIGURE_OPTIONS) -DCMAKE_BUILD_TYPE=Final,rm -Rf "build/f.$(OS)")

##########################################################################
##########################################################################

.PHONY:build
build:
	$(_V)$(MAKE) _build2 CONFIG=r
	$(_V)$(MAKE) _build2 CONFIG=f

.PHONY:_build2
_build2: CONFIG=$(error Must specify CONFIG)
_build2:
	$(_V)(test -d "$(BUILD)/$(CONFIG).$(OS)" && $(MAKE) _build3 CONFIG=$(CONFIG)) || (test ! -d "$(BUILD)/$(CONFIG).$(OS)")

.PHONY:_build3
_build3: CONFIG=$(error Must specify CONFIG)
_build3: _PATH=$(BUILD)/$(CONFIG).$(OS)
_build3:
# Verbose output is only really intended for use with CI. It produces
# quite a lot of stuff (but doesn't seem to actually add too much to
# the build time).
	$(_V)cd "$(_PATH)" && ninja $(__VERBOSE) -j $(NPROC)
	$(if $(_RUN_TESTS),$(_V)cd "$(_PATH)" && ctest $(__VERBOSE) --progress -j $(NPROC))

##########################################################################
##########################################################################

ifeq ($(UNAME),Darwin)

.PHONY:install
install:
	$(_V)echo Installation is a no-op on macOS

else

.PHONY:install
install: PREFIX=$(error Must specify PREFIX)
install: _B2_PATH:=src/b2/b2
install: _AVAILABLE=$(shell (test -f "$(BUILD)/r.linux/$(_B2_PATH)" && echo r)||(test -d "$(BUILD)/f.linux/$(_B2_PATH)" && echo f))
install:
	$(if $(_AVAILABLE),,$(_V)echo No builds available && false)
	$(_V)mkdir -p "$(PREFIX)/bin"
	$(_V)test -f "$(BUILD)/r.linux/$(_B2_PATH)" && cp -v "$(BUILD)/r.linux/$(_B2_PATH)" "$(PREFIX)/bin/b2-debug" || true
	$(_V)test -f "$(BUILD)/f.linux/$(_B2_PATH)" && cp -v "$(BUILD)/f.linux/$(_B2_PATH)" "$(PREFIX)/bin/b2" || true
	$(_V)mkdir -p "$(PREFIX)/share/b2"
	$(_V)cp -Rv "$(BUILD)/$(_AVAILABLE).linux/src/b2/assets/"* "$(PREFIX)/share/b2/"

endif
