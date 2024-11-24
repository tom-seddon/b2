# Building - Unix

The Unix build process covers building from the command line (or
Emacs, vim, etc.) on Unix-type systems, currently macOS and Linux.

I do some of the development on macOS, so that version should work
well.

The Linux version doesn't get much testing by me, though I do try it
on a Ubuntu VM occasionally. But I've had few reports of problems.
(Please [create an issue](https://github.com/tom-seddon/b2/issues) if
necessary.)

# Common prerequisites

- [cmake](https://cmake.org/) version 3.20+ on the PATH (I built it from [the CMake github repo](https://github.com/Kitware/CMake))
- [ninja](https://ninja-build.org/) (build from the [Ninja github repo](https://github.com/ninja-build/ninja), install from MacPorts, etc.)
- Python 3.x

# macOS prerequisites

- Xcode

## Additional prerequisites for video writing

These are optional. b2 will build without them, but video writing
won't be available.

- FFmpeg libs (should work with FFmpeg 4 or above)

Yuu can get these from MacPorts or Homebrew.

# Linux prerequisites

- gcc and g++, or clang

Additional apt package dependencies can be installed with:

    sudo apt-get -y install libcurl4-openssl-dev libgl1-mesa-dev libglvnd-dev libgtk2.0-dev libpulse-dev uuid-dev libsdl2-dev libuv1-dev
	
b2 should build with any version of SDL2, the newer the better!

SDL 2.0.16 or later will give slightly better-quality results from
`File` > `Save screenshot` when the `Correct aspect ratio` option is
ticked.
	
## Additional prerequisites for video writing
	
These are optional. b2 will build without them, but video writing
won't be available.

- `libx264-dev` apt package
- `FFmpeg` libs: libavcodec 58+, libavformat 58+, libavutil 56+,
  libswresample 3+, libswscale 5+ (this corresponds to FFmpeg 4 or
  later)
  
FFmpeg is easy enough to build from the [the FFmpeg github
repo](https://github.com/FFmpeg/FFmpeg) if the package manager version
doesn't suit.

# Building

Initial setup, for use after cloning or updating the repo:

1. Open terminal and change to working copy folder

2. Run `make init` - you should get a bunch of output - if anything
   goes wrong it should stop with a `CMake Error` that will hopefully
   explain things

Day-to-day build steps:

1. Change to build folder.

   For macOS, debug (`build/d.osx`) or release (`build/r.osx`).
   
   For Linux, debug (`build/d.linux`) or release (`build/r.linux`).
   
   (Debug build has no optimizations, asserts compiled in, plus maybe
   some other stuff that's snuck in.)
   
2. Run `ninja` to build. If building with gcc, note that it is normal
   for this to take longer than you might like. Build times with clang
   are a bit better. It may use a lot of RAM in either case
   
   (`ninja -j 1` will run max 1 job at once, possibly worth trying on
   Linux if the build awakens the OOM killer)

3. Run `ninja test` to run the automated tests (this might take a few
   minutes - they should all pass)

4. On Linux, run `./src/b2/b2` to run

   On macOS, run `./src/b2/b2.app/Contents/MacOS/b2` to run

(The day-to-day build steps may also work after updating the repo;
cmake is supposed to sort itself out. But it does cache some
information and the initial build steps ensure everything is rebuilt.)

You'll probably be able to automate all of this somehow from whatever
text editor you use...

# Installing on Linux

From the working copy folder, run `make install DEST=<<FOLDER>>`,
replacing `<<FOLDER>>` with the destination folder. For example:

    make install DEST=/usr/local
	
This will compile (if required) and install `$DEST/bin/b2-debug` (a
build of b2 with the debugger stuff included) and `$DEST/bin/b2` (a
build of b2 with no debugger functionality). It will also copy the
supporting files to `$DEST/share/b2`.

(Note that `b2-debug` is an optimized build of b2 with the debugger
included, not a debug build as produced by cmake.
[This naming scheme is not very clever](https://github.com/tom-seddon/b2/issues/40),
so it'll change at some point...)

You can also install by hand. The supporting files are searched for
relative to the EXE in `assets` and (if the EXE is in a folder called
`bin`) `../share/b2`, then in `share/b2` in the standard
`XDG_DATA_HOME` and `XDG_DATA_DIRS` folders.

# Installing on macOS

The app is built as a bundle, which you can find in the `src/b2`
folder inside the build folder of interest. You can run this in situ
from Finder or the command line, or copy it to your `Applications`
folder and run it from there.

# Sanitizers

As well as plain old release and debug, you can build b2 with
whichever gcc/clang sanitizers your compiler supports. Sanitizer build
folders have a suffix indicating which sanitizer is active:

* `a` - address sanitizer
* `t` - thread sanitizer
* `u` - undefined behaviour sanitizer
* `m` - memory sanitizer

Not all compilers support all sanitizers, nor is there any guarantee
this actually works. And if it does work, you might still get
warnings.

# Running the automated tests

`ninja test` will run the full set of tests.
