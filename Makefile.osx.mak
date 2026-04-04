# -*- mode:makefile-gmake; -*-

##########################################################################
##########################################################################

.PHONY:init_xcode
init_xcode:
	$(_V)$(PYTHON3) "bin/b2build.py" init --xcode

# Quicker turnaround when iterating on CMake stuff. CMake is supposed
# to do this for you automatically, something that works pretty well
# with Ninja and Visual Studio, but with Xcode it seems very
# unreliable.
.PHONY:reinit_xcode
reinit_xcode:
	$(_V)$(PYTHON3) "bin/b2build.py" init --xcode --reinit

##########################################################################
##########################################################################

.PHONY:github_ci_macos_homebrew
github_ci_macos_homebrew:
	brew update
	brew install ninja

.PHONY:github_ci_macos_homebrew_ffmpeg
github_ci_macos_homebrew_ffmpeg:
	brew install ffmpeg

.PHONY:_github_ci_macos_release
_github_ci_macos_release: export PYTHONUNBUFFERED=1
_github_ci_macos_release:
	$(PYTHON3) "./bin/b2build.py" --verbose release-binary-macos "$(shell $(PYTHON3) "./bin/b2build.py" print-build-suffix)" --timestamp "$(shell $(PYTHON3) "./bin/b2build.py" print-build-timestamp)" --gh-release $(TARGET_ARGS) $(if $(NO_FFMPEG),--no-ffmpeg,)

.PHONY:github_ci_macos_x64
github_ci_macos_x64:
	$(MAKE) _github_ci_macos_release TARGET_ARGS=--osx-deployment-target=12.0

.PHONY:github_ci_older_macos_x64
github_ci_older_macos_x64:
	$(MAKE) _github_ci_macos_release TARGET_ARGS=--osx-deployment-target=10.9

.PHONY:github_ci_macos_arm
github_ci_macos_arm:
	$(MAKE) _github_ci_macos_release TARGET_ARGS=--osx-deployment-target=13.0

##########################################################################
##########################################################################

B2_JSON_FOLDER:=$(HOME)/Library/Application Support/com.tom-seddon.b2
