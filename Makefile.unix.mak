# -*- mode:makefile-gmake; -*-
ifndef OS
$(error must specify OS on command line)
endif

.PHONY:init
init:
	$(_V)$(PYTHON3) "bin/b2build.py" -j $(NPROC) $(__VERBOSE) init --unix $(if $(SANITIZERS),--enable-sanitizers,) $(if $(FOLDER_PREFIX),--prefix "$(FOLDER_PREFIX)",) $(if $(CC),--cc "$(CC)") $(if $(CXX),--cxx "$(CXX)")

##########################################################################
##########################################################################

.PHONY: precommit
precommit:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format VERBOSE=$(VERBOSE)
	$(_V)$(PYTHON3) "bin/b2build.py" -j $(NPROC) $(__VERBOSE) batch --prefix precommit $(if $(REINIT),,--no-init) $(if $(CLEAN),,--no-clean)

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
	$(MAKE) _github_ci_ubuntu_release

.PHONY:github_ci_ubuntu_without_ffmpeg
github_ci_ubuntu_without_ffmpeg:
	$(MAKE) _github_ci_ubuntu_start
	$(MAKE) _github_ci_ubuntu_release

.PHONY:github_ci_ubuntu_with_ffmpeg
github_ci_ubuntu_with_ffmpeg:
	$(MAKE) _github_ci_ubuntu_start
	$(MAKE) _github_ci_ubuntu_install_ffmpeg
	$(MAKE) _github_ci_ubuntu_release UPLOAD=1

.PHONY:_github_ci_ubuntu_start
_github_ci_ubuntu_start:
	sudo apt-get -y update
	sudo apt-get -y install libcurl4-openssl-dev libgl1-mesa-dev libglvnd-dev libgtk-4-dev libpulse-dev uuid-dev libsdl2-dev libuv1-dev ninja-build

.PHONY:_github_ci_ubuntu_install_ffmpeg
_github_ci_ubuntu_install_ffmpeg:
	sudo apt-get -y install ffmpeg libavcodec-dev libavutil-dev libswresample-dev libavformat-dev libswscale-dev

.PHONY:_github_ci_ubuntu_release
_github_ci_ubuntu_release:
	$(PYTHON3) "./bin/b2build.py" --verbose release-source-linux "$(shell $(PYTHON3) "./bin/b2build.py" print-build-suffix)" --timestamp "$(shell $(PYTHON3) "./bin/b2build.py" print-build-timestamp)" $(if $(UPLOAD),--gh-release,) --build --test --install

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
	$(_V)$(PYTHON3) "bin/b2build.py" -j $(NPROC) $(__VERBOSE) batch --prefix precommit $(foreach COMPILER,$(_COMPILERS),--cc-cxx $(COMPILER) $(subst clang,clang++,$(subst gcc,g++,$(COMPILER)))) $(if $(REINIT),,--no-init) $(if $(CLEAN),,--no-clean)

##########################################################################
##########################################################################

B2_JSON_FOLDER?=$(HOME)/$(if $(XDG_CONFIG_HOME),$(XDG_CONFIG_HOME),.config)/b2
