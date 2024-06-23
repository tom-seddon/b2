# b2

A cross-platform emulator of the BBC Micro, BBC Master 128, BBC Master
Compact and Olivetti PC 128 S. Use your Windows/Linux PC or macOS
computer to play your old games or develop new software.

## Windows

See the
[Windows installation instructions](./doc/Installing-on-Windows.md).

You can set b2 to run when you double click a disk image in the
Windows file explorer. See the
[file assocation instructions](./doc/File-Association.md).

You can also follow the [building instructions](./doc/Building.md) to
build from source.

## macOS

See the [macOS installation instructions](./doc/Installing-on-OSX.md).
**Please also revisit the installation instructions when upgrading
from a previous version!**

You can set b2 to run when you double click a disk image in the
Finder. See the
[file assocation instructions](./doc/File-Association.md).

You can also follow the [building instructions](./doc/Building.md) to
build from source.

## Linux

Follow the [building instructions](./doc/Building.md) to
build from source.

There is also a [b2 snap](https://snapcraft.io/b2), looked after by
[Alan Pope](https://github.com/popey/b2-snap/) - this is something I
approve of, but due to lack of Linux knowledge I can't provide any
support for it myself)

# Documentation

See [the overview](./doc/Overview.md).

You can configure how the emulator starts up using the
[command line options](./doc/Command-Line.md).

For creating or hacking BBC software, the
[debug version](./doc/Debug-version.md) includes a range of debugging
functionality (including an integrated debugger), and a simple HTTP
API for remote control.

# Bugs/feedback/etc.

Please submit feedback to
[the b2 GitHub issues page](https://github.com/tom-seddon/b2/issues),
or post in
[the b2 thread on Stardot](https://stardot.org.uk/forums/viewtopic.php?f=4&t=13081).

# Licence

## `etc`, `submodules`

Please consult folders and files for more info.

The contents of `etc` is all stuff that's freely available and
distributable, included in the repo so it's self-contained, since not
every dependency can be added as a git submodule.

## `experimental`, `src`

Copyright (C) 2016-2024 by Tom Seddon

Licence: GPL v3.

-----

[Build status: ![status](https://ci.appveyor.com/api/projects/status/3sdnt3mh1r61h74y/branch/master?svg=true)](https://ci.appveyor.com/project/tom-seddon/b2/branch/master)

[Pre-release build status: ![status](https://ci.appveyor.com/api/projects/status/3sdnt3mh1r61h74y/branch/wip/master?svg=true)](https://ci.appveyor.com/project/tom-seddon/b2/branch/wip/master)
