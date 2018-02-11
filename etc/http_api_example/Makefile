DUMP:=1

TASS:=../64tass-1.52.1237/64tass.exe
CURL:=curl --retry 0 --connect-timeout 0.25 --silent --show-error --fail -o -
ifdef DUMP
CURL:=$(CURL) --header "b2Dump:1"
endif
MKDIR:=mkdir.exe -p
BUILD:=.build
URL:=http://127.0.0.1:48075
WIN:=b2

.PHONY:all
all:
	$(MKDIR) $(BUILD)
	$(TASS) --nostart -L $(BUILD)/main.lst -o $(BUILD)/main.bin main.asm
	$(CURL) $(URL)/w/$(WIN)/reset
	$(CURL) --upload-file $(BUILD)/main.bin $(URL)/w/$(WIN)/poke/ffff1000
	$(CURL) --upload-file commands.txt --header "Content-Type: text/plain; charset=utf-8" $(URL)/w/$(WIN)/paste
