# -*- mode:makefile-gmake; -*-
SHELL:=$(windir)\system32\cmd.exe

##########################################################################
##########################################################################

.PHONY:init_vs2022
init_vs2022:
	$(_V)$(MAKE) _newer_vs VSYEAR=2022 VSVER=17 VSVERNAME="Visual Studio 17 2022" FOLDER_SUFFIX=$(FOLDER_SUFFIX)

# .PHONY:init_vs2019
# init_vs2019:
# 	$(_V)$(MAKE) _newer_vs VSYEAR=2019 VSVER=16 VSVERNAME="Visual Studio 16"

##########################################################################
##########################################################################

.PHONY:_newer_vs
_newer_vs: VS_PATH:=$(shell "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -version $(VSVER) -property installationPath)
_newer_vs: CMAKE:=$(VS_PATH)\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
_newer_vs: FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)vs$(VSYEAR)$(FOLDER_SUFFIX)
_newer_vs:
	$(_V)$(if $(VS_PATH),,$(error Visual Studio $(VSYEAR) installation not found))
	$(_V)cmd /c bin\recreate_folder.bat $(FOLDER)
	$(_V)cd "$(FOLDER)" && "..\..\bin\msbuild_bug_wrapper.bat" "$(CMAKE)" $(CMAKE_DEFINES) -G "$(VSVERNAME)" -A x64 ../..
	$(_V)$(SHELLCMD) copy-file etc\b2.ChildProcessDbgSettings "$(FOLDER)"

##########################################################################
##########################################################################

.PHONY: run_tests_vs2022
run_tests_vs2022: CONFIG=$(error Must specify CONFIG)
run_tests_vs2022:
	$(_V)$(MAKE) _run_tests VSYEAR=2022 VSVER=17 CONFIG=$(CONFIG)

# .PHONY: run_tests_vs2019
# run_tests_vs2019: CONFIG=$(error Must specify CONFIG)
# run_tests_vs2019:
# 	$(_V)$(MAKE) _run_tests VSYEAR=2019 VSVER=16 CONFIG=$(CONFIG)

.PHONY:_run_tests
_run_tests: VS_PATH:=$(shell "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -version $(VSVER) -property installationPath)
_run_tests: CTEST:=$(VS_PATH)\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe
_run_tests:
	$(_V)cd "build\vs$(VSYEAR)$(FOLDER_SUFFIX)" && "$(CTEST)" -C $(CONFIG) -j $(NPROC) --timeout 180 --progress
	$(_V)cd "build\vs$(VSYEAR)$(FOLDER_SUFFIX)" && $(PYTHON3) "../../bin/check_ctest_log.py" "Testing\Temporary\LastTest.log"

##########################################################################
##########################################################################

TIME_JOBS:=$(PYTHON3) "bin/time_jobs.py" -f "$(BUILD_FOLDER)/time_jobs.txt"

##########################################################################
##########################################################################

PRECOMMIT_FOLDER_SUFFIX:=.precommit

.PHONY: precommit_vs2022
precommit_vs2022:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format QUIET=1
	$(_V)$(MAKE) _precommit VSYEAR=2022 VSVER=17

.PHONY: _precommit
_precommit:
	$(_V)$(TIME_JOBS) init
# might do something with the compiler column at some point?
	$(_V)$(TIME_JOBS) push "Compiler" "VS$(VSYEAR)"
	$(_V)$(if $(REINIT),$(MAKE) init_vs$(VSYEAR) FOLDER_SUFFIX=$(PRECOMMIT_FOLDER_SUFFIX))
	$(_V)$(MAKE) _precommit2 VSYEAR=$(VSYEAR) VSVER=$(VSVER) CONFIG=Debug CLEAN=$(CLEAN)
	$(_V)$(MAKE) _precommit2 VSYEAR=$(VSYEAR) VSVER=$(VSVER) CONFIG=RelWithDebInfo CLEAN=$(CLEAN)
	$(_V)$(MAKE) _precommit2 VSYEAR=$(VSYEAR) VSVER=$(VSVER) CONFIG=Final CLEAN=$(CLEAN)
	$(_V)$(TIME_JOBS) pop
	$(_V)$(TIME_JOBS) print

.PHONY: _precommit2
_precommit2: _VS_PATH:=$(shell "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -version $(VSVER) -property installationPath)
_precommit2: _DEVENV_PATH:=$(_VS_PATH)/Common7/IDE/devenv.com
_precommit2:
	$(_V)$(TIME_JOBS) push "Config" "$(CONFIG)"
	$(_V)$(if $(CLEAN),@$(TIME_JOBS) push "Action" "Clean")
	$(_V)$(if $(CLEAN),cd "build\vs$(VSYEAR)$(PRECOMMIT_FOLDER_SUFFIX)" && "..\..\bin\msbuild_bug_wrapper.bat" "$(_DEVENV_PATH)" b2.sln /Clean $(CONFIG))
	$(_V)$(if $(CLEAN),@$(TIME_JOBS) pop)
	$(_V)$(TIME_JOBS) push "Action" "Build"
	$(_V)cd "build\vs$(VSYEAR)$(PRECOMMIT_FOLDER_SUFFIX)" && "..\..\bin\msbuild_bug_wrapper.bat" "$(_DEVENV_PATH)" b2.sln /Build $(CONFIG)
	$(_V)$(TIME_JOBS) pop
	$(_V)$(TIME_JOBS) push "Action" "Test"
	$(_V)$(MAKE) _run_tests VSYEAR=$(VSYEAR) VSVER=$(VSVER) CONFIG=$(CONFIG) FOLDER_SUFFIX=$(PRECOMMIT_FOLDER_SUFFIX)
	$(_V)$(TIME_JOBS) pop
	$(_V)$(TIME_JOBS) pop -k "Config"

##########################################################################
##########################################################################

.PHONY:github_ci_windows
github_ci_windows:
	$(PYTHON3) "./etc/release/release.py" --verbose --timestamp=$(shell $(PYTHON3) "./etc/release/release2.py" print-timestamp) --gh-release $(shell $(PYTHON3) "./etc/release/release2.py" print-suffix)

##########################################################################
##########################################################################

ifdef LOCALAPPDATA
B2_JSON_FOLDER:=$(subst \,/,$(LOCALAPPDATA)/b2)
endif
