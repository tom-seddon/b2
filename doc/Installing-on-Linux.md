<!-- << not_source_release -->
# Installing on Linux from source

You can build from a source code release. Instructions below.

You can also build from the source code in the repo. See the [building
instructions](./Building.md).

<!-- >> not_source_release -->
# Install from source release

## Prerequisites

- Python 3.x
- gcc and g++, or clang
- SDL2 2.0.12 or later (SDL 2.0.16 or later will give slightly
  better-quality results from `File` > `Save screenshot` when the
  `Correct aspect ratio` option is ticked.)
- libcurl
- libuv
- cmake
- Gtk 4.10 or later

### APT-based distributions

The dependencies can be installed via apt. Ubuntu 24 and Linux Mint 22
should install suitable versions of the packages mentioned.

Required dependencies can be installed with:

    sudo apt-get -y install libcurl4-openssl-dev libgl1-mesa-dev libglvnd-dev libgtk-4-dev libpulse-dev uuid-dev libsdl2-dev libuv1-dev cmake
	
Optional dependencies for compressed video writing can be installed
with:
	
	sudo apt-get -y install libswresample-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libx264-dev

### Other distributions

You're on your own here I'm afraid, but hopefully the APT package list
will be enough to help figure out how to install the dependencies.

The code is intended to be compatible with `sdl2-compat` (an SDL2
compatibily layer for SDL3), as found on Arch Linux at least.

Any feedback welcome.

## Source release files

<!-- << not_source_release -->
Files for the latest release are here:

https://github.com/tom-seddon/b2/releases/latest

Download the `b2-linux-source-XXX.tar.bz2` file. (You don't need to
download any of the other files; this file contains everything
required.)

<!-- >> not_source_release -->
Extract to a folder of your choice. It will create a new folder called
`b2-XXX` (same suffix as the tar.bz2), with the actual files inside.
Change to that folder and run the steps from there.

## Configure

The configure step will find required libraries and whatnot and set
things up for the build.

	./configure

By default, this will set things up with the following options:

- build b2
- not build b2 with debugger
- run the automated tests after building
- when installing, install to `/usr/local`

For more options, see `./configure --help`. Options possibly most of
interest will be `--prefix` (specify an installation path other than
`/usr/local`) and `--build-b2-with-debugger` (also build b2 with
debugger).

## Build

The build step will build the code, and possibly run the tests, as per
the `./configure` settings.

    make
	
It is quite normal for this to take longer than you'd expect.

The build and test will make use of as many CPU cores/threads as
`nproc` reports. If this awakens the OOM killer, supply `NPROC=N` on
the command line to have it do only `N` jobs at once. (For example'
`make NPROC=1`. It will It will be quite normal for this to take even
longer than you'd expect.)

## Install

	make install
	
This will install the built programs to the configured prefix,
assuming a [folder structure like
`/usr`](https://en.wikipedia.org/wiki/Filesystem_Hierarchy_Standard).

Use `sudo` if required.

The following files and folders will be created under the prefix path:

- `bin/b2` - if standard b2 was built
- `bin/b2-debug` - if b2 with debugger was built
- `share/b2/` - always created

## Uninstall

To uninstall, manually delete the files and folders above.

## Other notes

- the build process is deliberately designed to follow the
  standard(ish) sequence of `./configure && make && sudo make
  install` - but it is not autoconf-based, so autoconf-related
  assumptions may not apply

- there are no build options other than those provided by
  `./configure` or described above, and no other supported way of
  building
  
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
	  
  (Depending on Linux distribution, it's possible there could be
  additional dependencies to install)

# Install from source prerelease

Prerelease versions may also be available:
https://github.com/tom-seddon/b2/releases (look for the ones marked
`Pre-release`, avoiding any with release notes telling you not to
download it!)

These may include new features and fixes added since the latest
release, that will make their way into some future release in due
course, once they've had a bit more testing.

Installation instructions are as above.
