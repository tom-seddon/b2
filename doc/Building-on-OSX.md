# Building on macOS

You can treat macOS as a Unix, and
[build from the command line](./Building-on-Unix.md). (The binary
build is prepared this way.)

You can also build using Xcode, giving quick and easy access to a
debugger.

# Build using Xcode

For prerequisites, see the
[building from the command line notes](./Building-on-Unix.md).

Initial build steps:

1. Open terminal in working copy folder

2. Run `make init_xcode` - you should get a bunch of output and no
   obvious errors
   
3. Load `build/Xcode/b2.xcodeproj` from Xcode

4. Ensure `ALL_BUILD` is selected in the schemes dropdown (this should
   be the case when you first load the newly-generated xcodeproj)

5. Select `Edit Scheme...` from the schemes dropdown, select the `Run`
   option, `Info` section, and select `b2.app` as the `Executable`

General day-to-day build steps:

1. Use `Product` > `Build` to build

2. Use `Product` > `Run` to run

By default, this builds the Debug build. Use `Edit Scheme...` from the
scheme dropdown to select a different configuration for the `Run`
option if you want something different.

After changing any of the cmake files, to regenerate the xcodeproj run
`make reinit_xcode` from the working copy folder. (`Product` > `Build`
is supposed to do this for you automatically, but it seems to be
rather unreliable.)
