[Windows Build Status: ![status](https://ci.appveyor.com/api/projects/status/3sdnt3mh1r61h74y?svg=true)](https://ci.appveyor.com/project/tom-seddon/b2)

[macOS/Linux Build Status: ![status](https://travis-ci.org/tom-seddon/b2.svg?branch=master)](https://travis-ci.org/tom-seddon/b2)

# b2

A cross-platform BBC Micro emulator. Use your Windows/Linux PC or
macOS computer to play your old BBC games or develop new BBC software.

For Windows and macOS, see the
[Windows installation instructions](./doc/Installing-on-Windows.md) or
[macOS installation instructions](./doc/Installing-on-OSX.md). You can
also follow the [building instructions](./doc/Building.md) to build
from source.

For Linux, follow the [building instructions](./doc/Building.md) to
build from source. There is also a [b2 snap](https://snapcraft.io/b2),
looked after by [Alan Pope](https://github.com/popey/b2-snap/) (this
is something I approve of, but due to lack of Linux knowledge I can't
provide any support for it myself).

There's a summary of the functionality available once up and running
in [the overview](./doc/Overview.md).

You can configure how the emulator starts up using the
[command line options](./doc/Command-Line.md).

For creating or hacking BBC software, the
[debug version](./doc/Debug-version.md) includes a range of debugging
functionality (including an integrated debugger), and a simple HTTP
API for remote control.

# Bugs/feedback/etc.

Please submit feedback to the
[b2 GitHub issues page](https://github.com/tom-seddon/b2/issues), or
post in
[the b2 thread on Stardot](https://stardot.org.uk/forums/viewtopic.php?f=4&t=13081).

# Licence

## `etc`, `submodules`

Please consult folders and files for more info.

The contents of `etc` is all stuff that's freely available and
distributable, included in the repo so it's self-contained, since not
every dependency can be added as a git submodule.

## `experimental`, `src`

Copyright (C) 2016-9 by Tom Seddon

Licence: GPL v3.

-----
