# Install on macOS

Requires a 64-bit Intel machine running OS X 10.9 (Mavericks) or
later, or an Apple Silicon Mac.

Files for the latest release are here:
https://github.com/tom-seddon/b2/releases/latest

If using macOS 13 (Ventura) or later on Apple Silicon, download the
`b2-macos-13.0-applesilicon-XXX.dmg` file if available, or follow the
instructions below if not.

If using macOS 11 (Big Sur) or later, download the
`b2-osx-11.0-XXX.dmg` or `b2-macos-11.0-intel-XXX.dmg` file.

If using OS X 10.9 (Mavericks) or later, download the
`b2-osx-10.9-XXX.dmg` or `b2-macos-10.9-intel-XXX.pmg` file. The 10.9
version does not support video writing, but is otherwise
fully-featured.

You don't need to download any of the other files; the dmg contains
everything required.

Open the dmg, drag b2 to your Applications folder, and run it from
there. (Running it from the dmg directly is unsupported, and it may
not work. I've had reports of odd behaviour when attempting it!)

On first run, you'll almost certainly get a warning about the
developer being unidentified. To get around this, follow the
instructions here:
https://support.apple.com/en-gb/guide/mac-help/mh40616/mac

## macOS files missing from releases page?

The releases are created automatically, and it can sometimes take a
day or two for the macOS version to appear.

If this happens, you can download an older version for now from the
full list at https://github.com/tom-seddon/b2/releases. Please revisit
after a day or two!

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

## What about Apple Silicon?

Apple Silicon releases are in the pipeline, and there's an open issue
for this here: https://github.com/tom-seddon/b2/issues/325

In the mean time, if you would be comfortable building b2 from source
code, you can get a native build on your Apple Silicon Mac by
following [the building instructions](./Building.md). I don't (yet!)
have a suitable Mac for testing Apple Silicon compatibility, but it is
supposed to work. Please report any issues via the
[b2 GitHub issues page](https://github.com/tom-seddon/b2/issues) or
[the b2 thread on Stardot](https://stardot.org.uk/forums/viewtopic.php?f=4&t=13081).
