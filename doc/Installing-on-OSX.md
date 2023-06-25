# Install on macOS

Requires a 64-bit machine running OS X 10.9 (Mavericks) or later.

Files for the latest release are here:
https://github.com/tom-seddon/b2/releases/latest.

If using macOS 11 (Big Sur) or later, download the
`b2-osx-11.0-XXX.dmg` file.

If using OS X 10.9 (Mavericks) or later, download the
`b2-osx-10.9-XXX.dmg` file. (The 10.9 version does not support video
writing, but is otherwise fully-featured.)

You don't need to download any of the other files; the dmg contains
everything required.

Open the dmg, drag b2 to your Applications folder, and run it from
there.

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
