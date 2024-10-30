# -*- mode:makefile-gmake; -*-
SHELL:=$(windir)\system32\cmd.exe

##########################################################################
##########################################################################

.PHONY:init_vs2019
init_vs2019:
	$(MAKE) _newer_vs VSYEAR=2019 VSVER=16

##########################################################################
##########################################################################

.PHONY:_newer_vs
_newer_vs: VS_PATH:=$(shell "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -version $(VSVER) -property installationPath)
_newer_vs: CMAKE:=$(VS_PATH)\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
_newer_vs: FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)vs$(VSYEAR)
_newer_vs:
	$(if $(VS_PATH),,$(error Visual Studio $(VSYEAR) installation not found))
	cmd /c bin\recreate_folder.bat $(FOLDER)
	cd "$(FOLDER)" && "$(CMAKE)" $(CMAKE_DEFINES) -G "$(strip Visual Studio $(VSVER))" -A x64 ../..
	$(SHELLCMD) copy-file etc\b2.ChildProcessDbgSettings "$(FOLDER)"

##########################################################################
##########################################################################

.PHONY:run_tests_vs2019
run_tests_vs2019: CONFIG=$(error Must specify CONFIG)
run_tests_vs2019:
	$(MAKE) _run_tests VSYEAR=2019 VSVER=16 CONFIG=$(CONFIG)

.PHONY:_run_tests
_run_tests: VS_PATH:=$(shell "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -version $(VSVER) -property installationPath)
_run_tests: CTEST:=$(VS_PATH)\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe
_run_tests:
	cd "build\vs$(VSYEAR)" && "$(CTEST)" -C $(CONFIG) -j $(NUMBER_OF_PROCESSORS)

##########################################################################
##########################################################################

.PHONY:github_ci_windows
github_ci_windows:
	$(PYTHON3) "./etc/release/release.py" --verbose --timestamp=$(shell $(PYTHON3) "./etc/release/release2.py" print-timestamp) --gh-release $(shell $(PYTHON3) "./etc/release/release2.py" print-suffix)
