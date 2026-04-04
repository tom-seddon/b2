# -*- mode:makefile-gmake; -*-
SHELL:=$(windir)\system32\cmd.exe

##########################################################################
##########################################################################

.PHONY:init_vs2022
init_vs2022:
	$(_V)$(PYTHON3) "bin/b2build.py" init --vs2022

##########################################################################
##########################################################################

.PHONY: precommit_vs2022
precommit_vs2022:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format QUIET=1
	$(_V)$(PYTHON3) "bin/b2build.py" $(__VERBOSE) batch --prefix "precommit." $(if $(REINIT),,--no-init) $(if $(CLEAN),,--no-clean)

##########################################################################
##########################################################################

.PHONY:github_ci_windows
github_ci_windows:
	$(PYTHON3) "./bin/b2build.py" --verbose release-binary-windows "$(shell $(PYTHON3) "./bin/b2build.py" print-build-suffix)" --timestamp "$(shell $(PYTHON3) "./bin/b2build.py" print-build-timestamp)" --gh-release

##########################################################################
##########################################################################

ifdef LOCALAPPDATA
B2_JSON_FOLDER:=$(subst \,/,$(LOCALAPPDATA)/b2)
endif
