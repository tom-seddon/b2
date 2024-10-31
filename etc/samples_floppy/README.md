Source: https://github.com/mamedev/mame/tree/master/samples/floppy

Note that the list of WAVs to include in the assets folder is picked
up by a cmake glob command. If files are added or removed then you'll
have to redo `make init` or (quicker...) touch `src/b2/CMakeLists.txt`
and recompile.

# **Samples** #

Samples are taken by team members or contributors to make mechanical sounds of various machines available in emulation.

Licensed under [CC0 1.0 Universal (CC0 1.0)](https://creativecommons.org/publicdomain/zero/1.0/)
