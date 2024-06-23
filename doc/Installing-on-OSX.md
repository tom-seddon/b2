# Install on macOS

Requires a 64-bit machine running OS X 10.9 (Mavericks) or later.

Files for the latest release are here:
https://github.com/tom-seddon/b2/releases/latest.

If using macOS 11 (Big Sur) or later, download the
`b2-osx-11.0-XXX.dmg` file.

If using OS X 10.9 (Mavericks) or later, download the
`b2-osx-10.9-XXX.dmg` file. The 10.9 version does not support video
writing, but is otherwise fully-featured.

You don't need to download any of the other files; the dmg contains
everything required.

Open the dmg, drag b2 to your Applications folder, and run it from
there.

## macOS files missing from releases page?

The releases are created automatically, and it can sometimes take a
day or two for the macOS version to appear.

If this happens, you can download an older version for now from the
full list at https://github.com/tom-seddon/b2/releases.

## Keystroke Receiving warning

The first time you run b2 on newer macOS, you may get a warning about
b2 wanting to receive keystrokes from any application. (This is due to
b2's tracking of the Caps Lock key... it isn't trying to steal your
data!)

Granting it access is optional, though necessary for the Caps Lock key
to work in the emulator. Follow the prompts if you want to do this.

If you'd prefer to deny it access, the Mac's Caps Lock key will simply
do nothing in b2. The default keyboard settings treat the Page Up key
as Caps Lock, so you won't lose out in this case.

## Upgrading from an older version of b2

If you granted b2 keystroke access, this setting will get forgotten
when upgrading to a newer version.

To fix this, go to `System Preferences`, `Security and Privacy`,
`Privacy` tab, `Input Monitoring`, and remove the entry for `b2` by
clicking on its row and using the `-` button.

Run it again, and you should get the keystroke access warning.

# Prerelease versions

Prerelease versions may also be available:
https://github.com/tom-seddon/b2/releases (look for the ones marked
`Pre-release`)

These will include new features and fixes added since the latest
release, but won't have had as much testing. (But they aren't
unsupported! Please do report any problems via
[the b2 GitHub issues page](https://github.com/tom-seddon/b2/issues)
or
[the b2 thread on Stardot](https://stardot.org.uk/forums/viewtopic.php?f=4&t=13081).

(Periodically, I promote one of the later prerelease versions to be
the new latest release, and remove any now-redundant prerelease
versions. That's why the latest release's date and the time stamp in
the file names for it aren't always in sync.)
