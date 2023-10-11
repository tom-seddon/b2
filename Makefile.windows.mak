# -*- mode:makefile-gmake; -*-

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
