# -*- mode:makefile-gmake; -*-
ifndef OS
$(error must specify OS on command line)
endif

CMAKE_TYPE?=Ninja

.PHONY:init
init:
	$(MAKE) _unix SANITIZER= SUFFIX=
ifndef RELEASE_MODE
	$(MAKE) -j$(NPROC) _sanitizers
endif

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

.PHONY:buildall
buildall:
	$(MAKE) _buildall SANITIZER=
	$(MAKE) _buildall SANITIZER=u
	$(MAKE) _buildall SANITIZER=a
	$(MAKE) _buildall SANITIZER=t
	$(MAKE) _buildall SANITIZER=m

.PHONY:_buildall
_buildall:
	$(MAKE) _buildall2 FOLDER=d$(SANITIZER)
	$(MAKE) _buildall2 FOLDER=r$(SANITIZER)
	$(MAKE) _buildall2 FOLDER=f$(SANITIZER)

.PHONY:_buildall2
_buildall2: _FOLDER:=$(BUILD_FOLDER)/$(FOLDER_PREFIX)$(FOLDER).$(OS)
_buildall2:
	test ! -d "$(_FOLDER)" || (cd $(_FOLDER) && ninja)

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

.PHONY:travis_ci_before_install_linux
travis_ci_before_install_linux:
	sudo apt-get update
	sudo apt-get -y install ninja-build cmake
	sudo apt-get -y install libcurl4-openssl-dev libgl1-mesa-dev libglvnd-dev libgtk2.0-dev libpulse-dev uuid-dev

#	cd ~ && git clone https://github.com/Kitware/CMake
#	cd ~/CMake && git checkout v3.16.6 && ./bootstrap && make && sudo make install
#	cd ~ && git clone https://github.com/ninja-build/ninja
#	cd ~/ninja && git checkout v1.8.2 && ./configure.py --bootstrap && sudo cp ninja /usr/local/bin/

##########################################################################
##########################################################################

.PHONY:github_ci_ubuntu_without_ffmpeg
github_ci_ubuntu_without_ffmpeg:
	$(MAKE) _github_ci_ubuntu_start
	$(MAKE) _github_ci_ubuntu_release SUFFIX1=noffmpeg-

.PHONY:github_ci_ubuntu_with_ffmpeg
github_ci_ubuntu_with_ffmpeg:
	$(MAKE) _github_ci_ubuntu_start
	sudo apt-get -y install ffmpeg libavcodec-dev libavutil-dev libswresample-dev libavformat-dev libswscale-dev
	$(MAKE) _github_ci_ubuntu_release SUFFIX1=ffmpeg-

.PHONY:_github_ci_ubuntu_start
_github_ci_ubuntu_start:
	sudo apt-get -y update
	sudo apt-get -y install libcurl4-openssl-dev libgl1-mesa-dev libglvnd-dev libgtk2.0-dev libpulse-dev uuid-dev libsdl2-dev libuv1-dev ninja-build

.PHONY:_github_ci_ubuntu_release
_github_ci_ubuntu_release:
	$(PYTHON3) "./etc/release/release.py" --verbose $(SUFFIX1)$(shell $(PYTHON3) "./etc/release/release2.py" print-suffix)	
