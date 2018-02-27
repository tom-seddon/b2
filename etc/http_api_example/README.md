# HTTP API demo

Slightly shoddy demo of the HTTP API...

## Windows prerequisites

* 64tass (Windows EXE supplied in repo - Makefile will use it)
* curl on PATH

## OS X/Linux prerequisites

* 64tass (get it from [the 64tass SourceForge page](https://sourceforge.net/projects/tass64/))
* GNU Make
* curl on PATH

## Building it

To build, run emulator, then run make. (For Windows, there's a
`make.bat` supplied that will use the repo's copy of GNU Make.)

It will assemble the code, reboot the emulated BBC and set the code
running.

I use this with M-x compile from inside Emacs.
