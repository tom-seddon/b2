# -*- mode:makefile-gmake; -*-

##########################################################################
##########################################################################

.PHONY:init_vs2015
init_vs2015:
	$(MAKE) _older_vs VSYEAR=2015 VSVER=14

##########################################################################
##########################################################################

.PHONY:init_vs2017
init_vs2017:
	$(MAKE) _older_vs VSYEAR=2017 VSVER=15

##########################################################################
##########################################################################

.PHONY:init_vs2019
init_vs2019:
	$(MAKE) _newer_vs VSYEAR=2019 VSVER=16

##########################################################################
##########################################################################

.PHONY:_older_vs
_older_vs: FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)win64.vs$(VSYEAR)
_older_vs:
	cmd /c bin\recreate_folder.bat $(FOLDER)
	cd "$(FOLDER)" && cmake $(CMAKE_DEFINES) -G "$(strip Visual Studio $(VSVER) Win64)" ../..
	$(SHELLCMD) copy-file etc\b2.ChildProcessDbgSettings "$(FOLDER)"

##########################################################################
##########################################################################

.PHONY:_newer_vs
_newer_vs: FOLDER=$(BUILD_FOLDER)/$(FOLDER_PREFIX)win64.vs$(VSYEAR)
_newer_vs:
	cmd /c bin\recreate_folder.bat $(FOLDER)
	cd "$(FOLDER)" && cmake $(CMAKE_DEFINES) -G "$(strip Visual Studio $(VSVER))" -A x64 ../..
	$(SHELLCMD) copy-file etc\b2.ChildProcessDbgSettings "$(FOLDER)"

##########################################################################
##########################################################################
