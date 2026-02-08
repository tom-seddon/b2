# -*- mode:makefile-gmake; -*-
ifndef OS
$(error must specify OS on command line)
endif

CMAKE_TYPE?=Ninja

.PHONY:init
init:
	$(MAKE) _unix SANITIZER= SUFFIX=
	$(if $(RELEASE_MODE)$(NO_SANITIZERS),,$(MAKE) -j$(NPROC) _sanitizers)
	@echo
	@echo "make init has succeeded. (It's normal for CMake to print some warnings and error messages as it goes. If you can see this message, it finished successfully and nothing unexpected happened.)"
	@echo

.PHONY:init_parallel
init_parallel:
	$(MAKE) _unix $(if $(RELEASE_MODE)$(NO_SANITIZERS),,_sanitizers) -j $(NPROC)

.PHONY:_usan
_usan:
	$(MAKE) _unix SANITIZER=UNDEFINED SUFFIX=u

.PHONY:_tsan
_tsan:
	$(MAKE) _unix SANITIZER=THREAD SUFFIX=t

.PHONY:_msan
_msan:
	$(MAKE) _unix SANITIZER=MEMORY SUFFIX=m

.PHONY:_asan
_asan:
	$(MAKE) _unix SANITIZER=ADDRESS SUFFIX=a

.PHONY:_sanitizers
_sanitizers: _usan _tsan _msan _asan

.PHONY:_unixd
_unixd:
	$(MAKE) _unix2 SANITIZER=$(SANITIZER) FOLDER=d$(SUFFIX) BUILD=Debug
.PHONY:_unixr
_unixr:
	$(MAKE) _unix2 SANITIZER=$(SANITIZER) FOLDER=r$(SUFFIX) BUILD=RelWithDebInfo
.PHONY:_unixf
_unixf:
	$(MAKE) _unix2 SANITIZER=$(SANITIZER) FOLDER=f$(SUFFIX) BUILD=Final

.PHONY:_unix
_unix: _unixd _unixr _unixf

.PHONY:_unix2
_unix2: _FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)$(FOLDER).$(OS)
_unix2:
	@echo ---------------------------------------------------------------------------
	@echo ---------------------------------------------------------------------------
	@echo ---------------------------------------------------------------------------
	rm -Rf "$(_FOLDER)"
	mkdir -p "$(_FOLDER)"
	(cd "$(_FOLDER)" && cmake -G "$(CMAKE_TYPE)" $(CMAKE_DEFINES) -DCMAKE_BUILD_TYPE=$(BUILD) $(if $(SANITIZER),-DSANITIZE_$(SANITIZER)=On) ../..) || $(if $(SANITIZER),rm -Rf "$(_FOLDER)",false)

##########################################################################
##########################################################################

TIME_JOBS:=$(PYTHON3) "bin/time_jobs.py" -f "$(BUILD_FOLDER)/time_jobs.txt"

##########################################################################
##########################################################################

.PHONY: precommit
precommit:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format VERBOSE=$(VERBOSE)
	$(_V)$(TIME_JOBS) init
	$(_V)$(MAKE) precommit_main
	$(_V)$(TIME_JOBS) print -s Config -s Compiler  $(if $(JOB_TIMES_FILE),| tee "$(JOB_TIMES_FILE)")

.PHONY:precommit_body
precommit_main:
	$(_V)$(if $(REINIT),$(MAKE) -j $(NPROC) init_parallel NO_SANITIZERS=$(NO_SANITIZERS))
# TODO: is there a way to figure out which compiler cmake picked??
	$(_V)$(TIME_JOBS) push Compiler Default
# the init step can be rather slow on macOS, so it's worth having a
# separate option for just the clean.
	$(_V)$(if $(CLEAN),$(MAKE) _precommit ACTION=clean) COMPILER_NAME=Default
	$(_V)$(if $(EXTRA_MESSAGE),echo $(EXTRA_MESSAGE))
	$(_V)$(MAKE) _precommit ACTION=build
	$(_V)$(_V)$(MAKE) _precommit ACTION=test
	$(_V)$(TIME_JOBS) pop

.PHONY:_precommit
_precommit:
_precommit:
	$(_V)$(MAKE) _precommit2 FOLDER=d CONFIG_NAME=Debug
	$(_V)$(MAKE) _precommit2 FOLDER=r CONFIG_NAME=RelWithDebInfo
	$(_V)$(MAKE) _precommit2 FOLDER=f CONFIG_NAME=Final

.PHONY:_precommit2
_precommit2: export _FOLDER:=$(BUILD_FOLDER)/$(FOLDER_PREFIX)$(FOLDER)$(SANITIZER).$(OS)
_precommit2:
	$(_V)$(TIME_JOBS) push "Config" "$(CONFIG_NAME)"
	$(_V)$(MAKE) _precommit_$(ACTION)
	$(_V)$(TIME_JOBS) pop

.PHONY:_precommit_clean
_precommit_clean:
	$(_V)$(TIME_JOBS) push "Action" "Clean"
	$(_V)cd "$(_FOLDER)" && ninja -j $(NPROC) clean
	$(_V)$(TIME_JOBS) pop

.PHONY:_precommit_build
_precommit_build:
	$(_V)$(TIME_JOBS) push "Action" "Build"
	$(_V)cd "$(_FOLDER)" && ninja -j $(NPROC)
	$(_V)$(TIME_JOBS) pop

.PHONY:_precommit_test
_precommit_test:
	$(_V)$(TIME_JOBS) push "Action" "Test"
	$(_V)cd "$(_FOLDER)" && ctest --progress -j $(NPROC)
	$(_V)cd "$(_FOLDER)" && $(PYTHON3) "../../bin/check_ctest_log.py" "Testing/Temporary/LastTest.log"
	$(_V)$(TIME_JOBS) pop

##########################################################################
##########################################################################

# TODO: decide what to do about this.

# .PHONY:buildall_no_sanitizers
# buildall_with_sanitizers:
# 	$(MAKE) buildall
# 	$(MAKE) _buildall SANITIZER=u
# 	$(MAKE) _buildall SANITIZER=a
# 	$(MAKE) _buildall SANITIZER=t
# 	$(MAKE) _buildall SANITIZER=m

# .PHONY:_buildall
# _buildall:
# 	$(MAKE) _buildall2 FOLDER=d$(SANITIZER)
# 	$(MAKE) _buildall2 FOLDER=r$(SANITIZER)
# 	$(MAKE) _buildall2 FOLDER=f$(SANITIZER)

# .PHONY:_buildall2
# _buildall2: _FOLDER:=$(BUILD_FOLDER)/$(FOLDER_PREFIX)$(FOLDER).$(OS)
# _buildall2:
# 	test ! -d "$(_FOLDER)" || (cd $(_FOLDER) && ninja)

##########################################################################
##########################################################################

ifdef INSTALLER

build_dir=$(BUILD_FOLDER)/$(1).$(OS)

.PHONY:install
install:
	@test -n "$(DEST)" || sh -c 'echo DEST variable must be set && false'
	cd $(call build_dir,r) && ninja
	cd $(call build_dir,f) && ninja
	mkdir -p "$(DEST)/bin"
	cp -v $(call build_dir,r)/src/b2/b2 "$(DEST)/bin/b2-debug"
	cp -v $(call build_dir,f)/src/b2/b2 "$(DEST)/bin/b2"

# Any build will do as source for the assets.
	mkdir -p "$(DEST)/share/b2"
	cp -Rv $(call build_dir,r)/src/b2/assets/* "$(DEST)/share/b2/"
endif

##########################################################################
##########################################################################

.PHONY:tom_emacs
tom_emacs: _BUILD_FOLDER=$(shell pwd)/$(BUILD_FOLDER)/d.$(OS)
tom_emacs:
# let Emacs know where the build is actually taking place -
# compilation mode watches the build output to figure out where
# relative paths are relative to, but it doesn't reliably spot
# everything...
	@echo make: Entering directory \'$(_BUILD_FOLDER)\'
	cd "$(_BUILD_FOLDER)" && ninja
#	cd "$(_BUILD_FOLDER)" && ctest -LE 'slow|kevin_edwards' -j $(NPROC) --output-on-failure

##########################################################################
##########################################################################

# Don't bother doing clang+no ffmpeg. Any serious issues will be
# caught by the gcc version. Hopefully.
.PHONY:github_ci_ubuntu_clang_without_ffmpeg
github_ci_ubuntu_clang_with_ffmpeg: export CC=clang-18
github_ci_ubuntu_clang_with_ffmpeg: export CXX=clang++-18
github_ci_ubuntu_clang_with_ffmpeg:
	$(MAKE) _github_ci_ubuntu_start
	$(MAKE) _github_ci_ubuntu_install_ffmpeg
	$(MAKE) _github_ci_ubuntu_release SUFFIX1=ffmpeg-clang-

.PHONY:github_ci_ubuntu_without_ffmpeg
github_ci_ubuntu_without_ffmpeg:
	$(MAKE) _github_ci_ubuntu_start
	$(MAKE) _github_ci_ubuntu_release SUFFIX1=noffmpeg-

.PHONY:github_ci_ubuntu_with_ffmpeg
github_ci_ubuntu_with_ffmpeg:
	$(MAKE) _github_ci_ubuntu_start
	$(MAKE) _github_ci_ubuntu_install_ffmpeg
	$(MAKE) _github_ci_ubuntu_release SUFFIX1=ffmpeg-

.PHONY:_github_ci_ubuntu_start
_github_ci_ubuntu_start:
	sudo apt-get -y update
	sudo apt-get -y install libcurl4-openssl-dev libgl1-mesa-dev libglvnd-dev libgtk-4-dev libpulse-dev uuid-dev libsdl2-dev libuv1-dev ninja-build

.PHONY:_github_ci_ubuntu_install_ffmpeg
_github_ci_ubuntu_install_ffmpeg:
	sudo apt-get -y install ffmpeg libavcodec-dev libavutil-dev libswresample-dev libavformat-dev libswscale-dev

.PHONY:_github_ci_ubuntu_release
_github_ci_ubuntu_release:
	$(PYTHON3) "./etc/release/release.py" --verbose $(SUFFIX1)$(shell $(PYTHON3) "./etc/release/release2.py" print-suffix)	

##########################################################################
##########################################################################

# could potentially figure out the branches using git branch -a
# --format='%(refname:short)'... but the list is not all that long.

.PHONY:_ffmpeg_releases
_ffmpeg_releases: export FFMPEG_UPSTREAM=git@github.com:FFmpeg/FFmpeg
_ffmpeg_releases: export FFMPEG_DEST:=/tmp/ffmpeg
_ffmpeg_releases: export FFMPEG_MIRROR:=$(FFMPEG_DEST)/FFmpeg.mirror
_ffmpeg_releases:
	$(MAKE) _ffmpeg_mirror
	$(MAKE) _ffmpeg_release VERSION=4.0
	$(MAKE) _ffmpeg_release VERSION=4.1
	$(MAKE) _ffmpeg_release VERSION=4.2
	$(MAKE) _ffmpeg_release VERSION=4.3
	$(MAKE) _ffmpeg_release VERSION=4.4
	$(MAKE) _ffmpeg_release VERSION=5.0
	$(MAKE) _ffmpeg_release VERSION=5.1
	$(MAKE) _ffmpeg_release VERSION=6.0
	$(MAKE) _ffmpeg_release VERSION=6.1
	$(MAKE) _ffmpeg_release VERSION=7.0
	$(MAKE) _ffmpeg_release VERSION=7.1

.PHONY:_ffmpeg_mirror
_ffmpeg_mirror:
	rm -Rf "$(FFMPEG_DEST)"
	mkdir -p "$(FFMPEG_DEST)"
	cd "$(FFMPEG_DEST)" && git clone --bare "$(FFMPEG_UPSTREAM)" "$(FFMPEG_MIRROR)"

.PHONY:_ffmpeg_release
_ffmpeg_release: VERSION=$(must specify VERSION)
_ffmpeg_release: _DEST=$(FFMPEG_DEST)/FFmpeg.$(VERSION)
_ffmpeg_release:
	rm -Rf "$(_DEST)"
	git clone "$(FFMPEG_MIRROR)" "$(_DEST)"
	cd "$(_DEST)" && git checkout "release/$(VERSION)"

##########################################################################
##########################################################################

# for me, on my desktop PC...

GCC_CC:=gcc
GCC_CXX:=g++

CLANG_CC:=clang-19
CLANG_CXX:=clang++-19

.PHONY:_precommit_tom_init_gcc
_precommit_tom_init_gcc:
	$(_V)$(MAKE) init_parallel FOLDER_PREFIX=precommit-gcc. CC=$(GCC_CC) CXX=$(GCC_CXX)

.PHONY:_precommit_tom_init_clang
_precommit_tom_init_clang:
	$(_V)$(MAKE) init_parallel FOLDER_PREFIX=precommit-clang. CC=$(CLANG_CC) CXX=$(CLANG_CXX)

.PHONY:precommit_tom
precommit_tom:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format

	$(_V)$(TIME_JOBS) init
	$(_V)$(MAKE) precommit_tom_main REINIT=$(REINIT)
	$(_V)$(TIME_JOBS) print -s Config -s Compiler $(if $(JOB_TIMES_FILE),| tee "$(JOB_TIMES_FILE)")

.PHONY:precommit_tom_main
precommit_tom_main:
	$(_V)$(if $(REINIT),$(MAKE) -j $(NPROC) _precommit_tom_init_gcc _precommit_tom_init_clang,)
	$(_V)$(TIME_JOBS) push "Compiler" "$(shell $(GCC_CC) --version | head -n 1)"
	$(_V)$(if $(CLEAN),$(MAKE) _precommit ACTION=clean FOLDER_PREFIX=precommit-gcc.)
	$(_V)$(MAKE) _precommit ACTION=build FOLDER_PREFIX=precommit-gcc.
	$(_V)$(MAKE) _precommit ACTION=test FOLDER_PREFIX=precommit-gcc.
	$(_V)$(TIME_JOBS) pop
	$(_V)$(TIME_JOBS) push "Compiler" "$(shell $(CLANG_CC) --version | head -n 1)"
	$(_V)$(if $(CLEAN),$(MAKE) _precommit ACTION=clean FOLDER_PREFIX=precommit-clang.)
	$(_V)$(MAKE) _precommit ACTION=build FOLDER_PREFIX=precommit-clang.
	$(_V)$(MAKE) _precommit ACTION=test FOLDER_PREFIX=precommit-clang.
	$(_V)$(TIME_JOBS) pop

##########################################################################
##########################################################################

.PHONY:test_build_times
test_build_times: TARGET=$(error must specify TARGET)
test_build_times:
	$(_V)$(TIME_JOBS) init
	$(_V)$(MAKE) _test_build_times TARGET=$(TARGET) RUN=1
	$(_V)$(MAKE) _test_build_times TARGET=$(TARGET) RUN=2
	$(_V)$(MAKE) _test_build_times TARGET=$(TARGET) RUN=3
	$(_V)$(TIME_JOBS) print -s Run -s Config -s Compiler  $(if $(JOB_TIMES_FILE),| tee "$(JOB_TIMES_FILE)")

.PHONY:_test_build_times
_test_build_times: TARGET=$(error must specify TARGET)
_test_build_times:
	$(_V)$(TIME_JOBS) push "Run" "$(RUN)"
	$(_V)$(MAKE) $(TARGET)_main REINIT=1 NO_SANITIZERS=1 "EXTRA_MESSAGE=Run $(RUN)"
	$(_V)$(TIME_JOBS) pop

##########################################################################
##########################################################################

B2_JSON_FOLDER?=$(HOME)/$(if $(XDG_CONFIG_HOME),$(XDG_CONFIG_HOME),.config)/b2
