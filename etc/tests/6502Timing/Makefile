ifeq ($(OS),Windows_NT)
PYTHON3:=py -3
else
PYTHON3:=/usr/bin/python3
endif

export _V:=$(if $(VERBOSE),,@)

##########################################################################
##########################################################################

export BEEBASM:=beebasm

##########################################################################
##########################################################################

SHELLCMD_RELATIVE_PATH:=submodules/shellcmd.py/shellcmd.py

WORKING_COPY_PATH:=$(shell $(PYTHON3) "../../../$(SHELLCMD_RELATIVE_PATH)" realpath "../../../")

SHELLCMD:=$(PYTHON3) "$(WORKING_COPY_PATH)/$(SHELLCMD_RELATIVE_PATH)"

CURRENT_PATH:=$(shell $(SHELLCMD) realpath .)

#SSD_EXTRACT:=$(PYTHON3) "$(WORKING_COPY_PATH)/$(SHELLCMD_

##########################################################################
##########################################################################

6502TIMING_PATH:=$(WORKING_COPY_PATH)/submodules/6502Timing

##########################################################################
##########################################################################

.PHONY:default
default:
	$(_V)cd "$(WORKING_COPY_PATH)/submodules/6502Timing" && $(MAKE) -f "$(CURRENT_PATH)/Makefile2.mak" _build
	$(_V)$(MAKE) extract

##########################################################################
##########################################################################

.PHONY:extract
extract:
	$(_V)$(MAKE) _extract SSD=6502timing DRIVE=P
	$(_V)$(MAKE) _extract SSD=6502timing1M DRIVE=Q
	$(_V)$(MAKE) _extract SSD=65C02timing DRIVE=R
	$(_V)$(MAKE) _extract SSD=65C02timing1M DRIVE=S
	$(_V)$(MAKE) _extract SSD=65C12timing DRIVE=T
	$(_V)$(MAKE) _extract SSD=65C12timing1M DRIVE=U

.PHONY:_extract
_extract: SSD=$(error Must specify SSD)
_extract: DRIVE=$(error Must specify DRIVE)
_extract:
	$(_V)$(PYTHON3) "$(WORKING_COPY_PATH)/submodules/beeb/bin/ssd_extract.py" -0 -o "$(WORKING_COPY_PATH)/etc/b2_tests/$(DRIVE)/" "$(WORKING_COPY_PATH)/submodules/6502Timing/$(SSD).ssd"
