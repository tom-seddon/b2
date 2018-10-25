# Building - OS X/Linux

The build process is somewhat similar on both platforms.

I do some of the development on OS X, so the OS X version should work
well.

The Linux version is most politely described as "experimental".

Prerequisites for both:

- gcc, g++, make (on OS X you get these with Xcode)
- Python 2.x (on OS X this comes with the system)
- [cmake](https://cmake.org/) version 3.9+ on the PATH (I built it from [the CMake github repo](https://github.com/Kitware/CMake))
- [ninja](https://ninja-build.org/) (build from the [Ninja github repo](https://github.com/ninja-build/ninja), install from MacPorts, etc.)

Additional Linux apt package dependencies:

- `uuid-dev`
- `libgtk2.0-dev`
- `libgl1-mesa-dev`
- `libpulse-dev`
- `libgles1-mesa-dev`

Optional Linux dependencies needed for video writing: (the project
will build without these, but video writing won't work)

- `libx264-dev` apt package
- `FFmpeg` ver 57 or better (the Ubuntu package is ver 56. I built it
  from [the FFmpeg github repo](https://github.com/FFmpeg/FFmpeg) with
  configure options `--enable-libx264 --enable-gpl --enable-shared`)

Initial setup steps:

1. Open terminal and change to working copy folder

2. Run `make init` - you should get a bunch of output - if anything
   goes wrong it should stop with a `CMake Error` that will hopefully
   explain things

General day-to-day build steps:

1. Change to build folder.

   For OS X, debug (`build/d.osx`) or release (`build/r.osx`).
   
   For Linux, debug (`build/d.linux`) or release (`build/r.linux`).
   
   (Debug build has no optimizations, asserts compiled in, plus maybe
   some other stuff that's snuck in.)
   
2. Run `ninja` to build

3. Run `ninja test` to run the automated tests (this might take a few
   minutes - they should all pass)

4. On Linux, run `./src/b2/b2` to run

   On OS X, run `./src/b2/b2.app/Contents/MacOS/b2` to run

You'll probably be able to automate this somehow from whatever text
editor you use. For example, on OS X I use Emacs with a .dir-locals
file as follows. Copying this particular `ctest` command line is
recommended; the slow tests are skipped (total time is usually <1
second), and if a test fails then its output is shown directly.

    ((nil . ((compile-command . "cd ~/beeb/b2/build/osx && ninja && ctest -LE slow --output-on-failure"))))

To force a rebuild, run `ninja clean` in the build folder. Maybe you
can automate that too. I haven't bothered.

# OS X bundle

The app is built as an OS X bundle, which you can find in the `src/b2`
folder in the build folder.

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
