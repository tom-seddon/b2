# -*- mode:makefile-gmake; -*-
SHELL:=$(windir)\system32\cmd.exe

##########################################################################
##########################################################################

.PHONY:init_vs2022
init_vs2022:
	$(_V)$(MAKE) _init_vs VSYEAR=2022

.PHONY:init_vs2026
init_vs2026:
	$(_V)$(MAKE) _init_vs VSYEAR=2026

.PHONY:_init_vs
_init_vs: VSYEAR=$(error must specify VSYEAR)
_init_vs:
	$(_V)$(PYTHON3) "bin/b2build.py" init --vs$(VSYEAR) $(if $(ARCH),--vs$(VSYEAR)-architecture $(ARCH),)

##########################################################################
##########################################################################

.PHONY:precommit_vs:
precommit_vs:
	$(_V)$(MAKE) _precommit_vs "OPTIONS=--vs2022 --vs2026"

.PHONY:precommit_vs2022
precommit_vs2022:
	$(_V)$(MAKE) _precommit_vs "OPTIONS=--vs2026"

.PHONY:precommit_vs2026
precommit_vs2026:
	$(_V)$(MAKE) _precommit_vs "OPTIONS=--vs2026"

.PHONY: _precommit_vs
_precommit_vs:
	$(_V)echo clang-format...
	$(_V)$(MAKE) clang-format QUIET=1
	$(_V)$(PYTHON3) "bin/b2build.py" $(__VERBOSE) batch --prefix "precommit." $(if $(REINIT),,--no-init) $(if $(CLEAN),,--no-clean) $(OPTIONS)

##########################################################################
##########################################################################

.PHONY:github_ci_windows
github_ci_windows: VSYEAR=$(error must specify VSYEAR)
github_ci_windows:
	$(PYTHON3) "./bin/b2build.py" --verbose release-binary-windows "$(shell $(PYTHON3) "./bin/b2build.py" print-build-suffix)" --timestamp "$(shell $(PYTHON3) "./bin/b2build.py" print-build-timestamp)" $(if $(UPLOAD),--gh-release,) --vs$(VSYEAR)

##########################################################################
##########################################################################

ifdef LOCALAPPDATA
B2_JSON_FOLDER:=$(subst \,/,$(LOCALAPPDATA)/b2)
endif
