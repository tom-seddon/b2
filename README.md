# b2

A cross-platform emulator of the BBC Micro, BBC Master 128, BBC Master
Compact and Olivetti PC 128 S. Use your Windows/Linux PC or macOS
computer to play your old games or develop new software.

<!-- @@ source_release_README -->

## Getting started

<!-- << not_source_release -->
### Windows

See the
[Windows installation instructions](./doc/Installing-on-Windows.md).

You can set b2 to run when you double click a disk image in the
Windows file explorer. See the
[file assocation instructions](./doc/File-Association.md).

### macOS

See the [macOS installation instructions](./doc/Installing-on-OSX.md).
**Please also revisit the installation instructions when upgrading
from a previous version!**

You can set b2 to run when you double click a disk image in the
Finder. See the
[file assocation instructions](./doc/File-Association.md).

### Linux
<!-- >> not_source_release -->
See the [Linux installation
instructions](./doc/Installing-on-Linux.md).
<!-- << not_source_release -->

There are some additional options, though I can't provide any support
for them myself:

* [b2 snap](https://snapcraft.io/b2), looked after by
[Alan Pope](https://github.com/popey/b2-snap/)
* [SlackBuilds](https://slackbuilds.org/repository/15.0/system/b2/),
  maintaned by Antonio Leal

### libretro/RetroArch

There is a version of the core b2 emulation code available as a
libretro core: https://docs.libretro.com/library/b2/# - looked after
by [Zoltan Balogh](https://github.com/zoltanvb/b2-libretro). I can't
provide any support for this but I do approve!

Included in RetroArch, available on the Apple App Store: :
https://apps.apple.com/us/app/retroarch/id6499539433
<!-- >> not_source_release -->

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

(Regarding the GitHub issues list: nothing in the list will get
forgotten, and I do intend to fix every open item! But this is a spare
time hobby project for me, so it can take a while before the right
combination of appropriate block of time, motivation, equipment setup,
etc., presents itself.)

<!-- << not_source_release -->
# Source code

b2 is free and open source. Follow the [building
instructions](./doc/Building.md) to get the code from the GitHub repo
and build it yourself.
<!-- >> not_source_release -->
# Licence

## `etc`, `submodules`

Please consult folders and files for more info.

The contents of `etc` is all stuff that's freely available and
distributable, included in the repo so it's self-contained, since not
every dependency can be added as a git submodule.

## `experimental`, `src`

Copyright (C) 2016-2026 by Tom Seddon

Licence: GPL v3.
