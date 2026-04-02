# -*- mode:makefile-gmake; -*-
ifndef OS
$(error must specify OS on command line)
endif

.PHONY:init
init:
	$(_V)$(PYTHON3) "bin/b2build.py" -j $(NPROC) $(if $(VERBOSE),--verbose,) init --unix $(if $(SANITIZERS),--enable-sanitizers,) $(if $(FOLDER_PREFIX),--prefix "$(FOLDER_PREFIX)",) $(if $(CC),--cc "$(CC)") $(if $(CXX),--cxx "$(CXX)")

##########################################################################
##########################################################################

.PHONY: precommit
precommit:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format VERBOSE=$(VERBOSE)
	$(_V)$(PYTHON3) "bin/b2build.py" -j $(NPROC) $(if $(VERBOSE),--verbose,) batch --prefix precommit

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
	$(PYTHON3) "./etc/release/release.py" --verbose --ctest-output-on-failure $(SUFFIX1)$(shell $(PYTHON3) "./etc/release/release2.py" print-suffix)

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

# for me, on my desktop PC or my Mac.

ifeq ($(UNAME),Darwin)
DEFAULT_COMPILERS:=clang
else
DEFAULT_COMPILERS:=gcc-13 clang-20
endif

.PHONY:precommit_tom
precommit_tom: _COMPILERS:=$(if $(COMPILERS),$(COMPILERS),$(DEFAULT_COMPILERS))
precommit_tom:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format VERBOSE=$(VERBOSE)
	$(_V)$(PYTHON3) "bin/b2build.py" -j $(NPROC) $(if $(VERBOSE),--verbose,) batch --prefix precommit $(foreach COMPILER,$(_COMPILERS),--cc-cxx $(COMPILER) $(subst clang,clang++,$(subst gcc,g++,$(COMPILER))))

##########################################################################
##########################################################################

B2_JSON_FOLDER?=$(HOME)/$(if $(XDG_CONFIG_HOME),$(XDG_CONFIG_HOME),.config)/b2
