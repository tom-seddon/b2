# Installing on Linux from source

You can build from a source code release, which has a streamlined
process designed to be buildable and installable without too much
fuss.

You can also build from the source code in the repo.

## Install from source release

### Prerequisites

- Python 3.x
- gcc and g++, or clang
- SDL2 2.0.12 or later (SDL 2.0.16 or later will give slightly
  better-quality results from `File` > `Save screenshot` when the
  `Correct aspect ratio` option is ticked.)
- libcurl
- libuv
- cmake
- Ninja
- Gtk 4.10 or later

#### APT-based distributions

The dependencies can be installed via apt. Ubuntu 24 and Linux Mint 22
should install suitable versions of the packages mentioned.

Required dependencies can be installed with:

    sudo apt-get -y install libcurl4-openssl-dev libgl1-mesa-dev libglvnd-dev libgtk-4-dev libpulse-dev uuid-dev libsdl2-dev libuv1-dev cmake ninja-build
	
Optional dependencies for compressed video writing can be installed
with:
	
	sudo apt-get -y install libswresample-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libx264-dev

#### Other distributions

You're on your own here I'm afraid, but hopefully the APT package list
will be enough to help figure out how to install the dependencies.

The code is intended to be compatible with `sdl2-compat` (an SDL2
compatibily layer for SDL3), as found on Arch Linux at least.

Any feedback welcome.

### Download files

Files for the latest release are here:

https://github.com/tom-seddon/b2/releases/latest

The full list of releases includes prereleases, downloadable at your
own risk:

https://github.com/tom-seddon/b2/releases/

Whatever you go for, download the `b2-linux-source-XXX.tar.bz2` file.
(You don't need to download any of the other files; this file contains
everything required.)

Extract to a folder of your choice. It will create a new folder called
`b2-XXX` (same suffix as the tar.bz2), with the b2 code inside. Change
to that folder.

### Configure

The configure step will find required libraries and whatnot and set
things up for the build.

	make configure B2_STANDARD=1 B2_WITH_DEBUGGER=1
	
Set the `B2_STANDARD` flag to 1 or 0, depending on whether you want to
build normal b2; and same for `B2_WITH_DEBUGGER` depending whether you
want to build [b2 with debugger](./docs/Debug-version.md).

### Build

The build step will build the code, and then run the automated tests.

    make build
	
It is quite normal for this to take longer than you'd expect.

The build will make use of as many cores as it can find. If this
awakens the OOM killer, try supplying `NPROC=1` on the command line to
have it compile only one file at once.

### Install

    make install PREFIX=<<path>>
	
This will install the built programs to `<<path>>`, assuming a [folder
structure like
`/usr`](https://en.wikipedia.org/wiki/Filesystem_Hierarchy_Standard).

Use `sudo` if required.

The following files and folders will be created under the prefix path:

- `bin/b2` - if standard b2 was built
- `bin/b2-debug` - if b2 with debugger was built
- `share/b2/` - always copied

### Uninstall

To uninstall, manually delete the files and folders above.

### Other notes

- there are no build options other than described in this document,
  and no other supported way of building

- the source distribution is entirely self-contained, and not
  upgradeable. Each distribution quite deliberately creates an
  entirely separate folder structure
  
- once the thing is installed, there is no need for the source
  distribution any more, and you can delete it

- CMake will hopefully pick a sensible compiler, but you can set one
  explicitly using the `CC` and `CXX` environment variables. For
  example:
  
      export CC=$(which clang-20)
	  export CXX=$(which clang++-20)
	  
## Install from repo

See the [building instructions](./Building.md).
