# Building on macOS

You can treat macOS as a Unix, and [build from the command
line](./Building-on-Unix.md). (The binary build is prepared this way,
so it should work fine. But please do still give these notes a quick
skim, as some macOS-specific stuff will still apply.)

You can also build using Xcode, giving quick and easy access to a
debugger.

# Build using Xcode

For prerequisites, see the
[building from the command line notes](./Building-on-Unix.md).

Initial setup, for use after cloning:

1. Open terminal in working copy folder

2. Run `make init_xcode` - _this may take a very long time_. (20
   minutes on my laptop.) You should get a bunch of output and
   hopefully no obvious errors
   
3. Load `build/Xcode/b2.xcodeproj` from Xcode. I opt to automatically
   create all schemes

4. Select `b2` in the schemes dropdown

For use after updating the repo:

1. Open terminal in working copy folder

2. Run `make reinit_xcode`

General day-to-day build steps:

1. Load `build/Xcode/b2.xcodeproj` in Xcode, if not there already

2. Use `Product` > `Build` to build. It can take a while to build.
   Sorry

3. Use `Product` > `Run` to run

By default, this builds the Debug build. Use `Edit Scheme...` from the
scheme dropdown to select a different configuration for the `Run`
option if you want something different.

## Changing CMake settings?

After changing any of the CMake files, to regenerate the xcodeproj run
`make reinit_xcode` from the working copy folder. (`Product` > `Build`
is supposed to do this for you automatically, but it seems to be
rather unreliable.)

# Changing Info.plist? (also applies if building Unix-style)

There's an `Info.plist` in the Xcode project - it's auto-generated.
Don't edit it. The correct file to edit is `template.Info.plist`; the
`${...}` values are replaced with corresponding values from the CMake
setup.

If building with Xcode: I'm not sure when `template.Info.plist` is
supposed to be re-read, but it seems most reliable to do `make
reinit_xcode` to prod CMake into regenerating it.

# Bundle identifiers (also applies if building Unix-style)

All built app bundles end up with the same bundle identifier:
`com.tom-seddon.b2`.

The release DMGs are prepared using a separate process, which gives
the `b2 Debug` bundles a different bundle identifier. This is specific
to creating the release DMGs, and doesn't happen when building
normally (an annoying limitation in CMake).

# macOS badgering you about keystroke monitoring? (also applies if building Unix-style)

Adding b2 to the list of apps allowed to monitor keystrokes (see the
[install on macOS notes](./Installing-on-OSX.md)) doesn't work very
well if you're building it locally.

I haven't investigated this particularly thoroughly. The default
keyboard layouts treat the Page Up key as Caps Lock, so I just put up
with that.
