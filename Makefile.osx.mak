# -*- mode:makefile-gmake; -*-

##########################################################################
##########################################################################

.PHONY:init_xcode
init_xcode: _FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)Xcode
init_xcode:
	rm -Rf "$(_FOLDER)"
	mkdir -p "$(_FOLDER)"
	cd "$(_FOLDER)" && cmake -G Xcode $(CMAKE_DEFINES) ../..

# Quicker turnaround when iterating on CMake stuff. CMake is supposed
# to do this for you automatically, something that works pretty well
# with Ninja and Visual Studio, but with Xcode it seems very
# unreliable.
.PHONY:reinit_xcode
reinit_xcode: _FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)Xcode
reinit_xcode:
	cd "$(_FOLDER)" && cmake -G Xcode $(CMAKE_DEFINES) ../..

##########################################################################
##########################################################################

# for me, on my Mac... but it might work for you...
.PHONY:run_tests
run_tests: BUILD?=Debug
run_tests: _OUTPUT=b2_tests_output/
run_tests: MELD?=/Applications/Meld.app/Contents/MacOS/Meld
run_tests:
	cd build/Xcode && rm -Rf $(_OUTPUT) && ctest -C $(BUILD) -j$(NPROC) -LE 'slow|kevin_edwards' --output-on-failure || $(MELD) $(_OUTPUT)/got/ $(_OUTPUT)/wanted/

.PHONY:run_all_tests
run_all_tests: BUILD?=RelWithDebInfo
run_all_tests:
	cd build/Xcode && ctest -C $(BUILD) -j$(NPROC) --output-on-failure

##########################################################################
##########################################################################

.PHONY:travis_ci_before_install_osx
travis_ci_before_install_osx:
	brew install ninja
	brew install ffmpeg
