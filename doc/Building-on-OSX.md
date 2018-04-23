# Building on OS X

You can treat OS X as a Unix, and
[build from the command line](./Building-on-Unix.md). (The binary
build is prepared this way.)

You can also build using Xcode, giving quick and easy access to a
debugger.

# Build using Xcode

Prerequisites:

* Xcode
* [cmake](https://cmake.org/) version 3.5+

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

If the cmake settings change, `Product` > `Build` is supposed to
rebuild the xcodeproj, and Xcode should then reload it, and the
workflow should all be relatively convenient. But in practice it seems
to be a bit flaky; I find the build is spuriously cancelled,
necessitating additional attempts. Sometimes it requires several goes
before the build will finish.

(I don't really think this works especially well, particularly
compared to Visual Studio. Xcode is awful, and cmake doesn't seem to
support it as well as it could. But it's not quite a total disaster,
and the Xcode debugger is certainly better than gdb... for whatever
that's worth, at any rate.)
