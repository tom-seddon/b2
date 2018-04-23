[Windows Build Status: ![status](https://ci.appveyor.com/api/projects/status/3sdnt3mh1r61h74y?svg=true)](https://ci.appveyor.com/project/tom-seddon/b2)

# b2

A cross-platform BBC Micro emulator. Use your Windows/Linux PC or
macOS computer to play your old BBC games or develop new BBC software.

If you have feedback, please add it to the
[b2 GitHub issues page](https://github.com/tom-seddon/b2/issues).

# Install

For Windows and OS X, you can get a binary release from the
[b2 GitHub releases page](https://github.com/tom-seddon/b2/releases).
Releases are tagged with their date, time, and GitHub commit. You
probably want the latest one, which will be at the top.

On Windows, unzip to a folder of your choice; on OS X, open the dmg
and drag b2 to your Applications folder and run it from there.

If you use Linux, or if you just want to build it yourself anyway,
follow [the building instructions](#building).

## Rolling Windows builds

Feeling daring? You can get a Windows build of whatever the latest
buildable code is/was, hot off the presses, for good or for ill, from
the
[AppVeyor b2 build history page](https://ci.appveyor.com/project/tom-seddon/b2/history).
Find the highest-numbered green build, click its version number in the
right-hand column, select `ARTIFACTS` from the version page, download
the zip file. Install it as above.

# Guided tour

Double-click the icon: `b2` on OS X, `b2.exe` on 64-bit Windows, or
`b2_32bit.exe` on 32-bit Windows.

Everything you need to get started is provided, and the familiar `BBC
Computer 32K` message should appear straight away.

## Load a disc

Go to `Drive 0` or `Drive 1` on the `File` menu to load a disc. Use
the `Disc image` option to select an `SSD` file, then hit Shift+Break
(PC Shift + PC F11) to start it.

Changing the BBC disc image doesn't affect the file on the PC disk -
after loading a disc image, there are additional `Save disc image` and
`Save disc image as...` options you can use to do this manually.

## Change config

The default setup is a BBC Model B with Acorn 1770 DFS and 16K
sideways RAM. The `File` > `Change config` submenu lets you select
something different.

## Change keyboard mapping ##

The default keyboard layout maps PC keys to their BBC equivalent by
position - typically what you want for games. Some games might be
better with the `Default (caps/ctrl)` layout, which sets PC Left Ctrl
to BBC Caps Lock, PC Left Alt to BBC Ctrl, and PC Caps Lock to BBC
Ctrl.

There are also two character map options, designed for typing, which
try to map PC keys to BBC keys based on the character, so that PC
Shift+0 gives you `)` and so on. `Default UK` is for UK-style
keyboards, and `Default US` is for US-style keyboards. For US-style
keyboards, press ` to get the pound sign.
  
## Undo/redo

Use `File` > `Save state` to save your place, and `File` > `Load last
state` to reload it. All state is saved, including disc contents.

Use `Tools` > `Timeline...` to visualise the saved states. The
timeline is shown as a tree of rounded boxes (saved states) and square
boxes (open windows), with arrows showing the relationships.

Click `Load` to load an old state, `Clone` to create a new window
starting from that state, `Replay` to watch a replay from that state
to the current state, or (*Windows/Linux only*) `Video` to save a
video replay. (Video is MP4: 800Kbps H264 video + 128Kbps AAC audio.)

If the timeline becomes too full of junk, click `Delete` to delete a
saved state.

## Options

Use `Tools` > `Options...` to bring up the options dialog, letting you
fiddle with screen size and speed limiting and stuff.

### Display options

Tick `Auto scale` to have the Beeb display automatically scaled based
on the window size, or untick it and use the `Manual scale` filter to
choose your preferred scale.

`Correct aspect ratio` makes the display slightly narrower, better
matching the output from a real BBC.

`Filter display` lets the GPU smooth the display a bit when it's being
stretched.

For best results, leave these options on.

For (inauthentic!) 1:1 pixel output, switch them off and set a manual
scale of 1.0, or 2.0, and so on.

### Turbo disc

Click `Turbo disc` to activate turbo disc mode. Turbo disc mode
improves disc read/write throughput by 2-3x, and further improves
speed by turning off emulation of seek times and internal disc delays.

**Turbo disc mode is experimental**

## Customize keyboard layout

Click `Tools` > `Keyboard layout...` to bring up the keyboard layout
dialog, showing a map of the BBC keyboard. Hover over a key to see
which PC keys correspond to it. (For character maps, some keys have
two parts, mapped separately.)

Each keymap has its own section, though initially only the active
keymap is shown. Click the little disclosure arrow thing to reveal the
others.

The default keymaps are read-only. Click the `Copy` button to make a
modifiable copy, which you can give a name. It will appear in the
`Keymap` submenu along with the others.

(To create a new positional map, copy one of the positional maps; to
create a new character map, copy one of the character maps.)

(The BBC keyboard map always shows the Master 128 keypad, but this
only has an effect in Master 128 mode.)

At the top of the list you can also configure the (limited number of)
keyboard shortcuts available for emulator functions.

## Customize configurations

`Tools` > `Configurations` lets you customize the configurations list.

To create a new config, use the `Copy` button to copy an existing one
that has the disc interface you want. You can give it a name.

Click the `...` button next to each ROM slot to load the ROM image for
that slot.

Check the box in the RAM column to make that sideways slot writeable.

To add a new ROM slot, use one of the `Add ROM` buttons and select a
file.

To add a new sideways RAM slot, use one of the `Add RAM` buttons.
Sideways RAM slots are by default empty but you can use the `...`
button to load a ROM image on startup.

## Paste from clipboard

Paste text from the clipboard to the BASIC prompt using `OSRDCH Paste`
and `OSRDCH Paste (+Return)`. The `(+Return)` version effectively
presses Return at the end, which is sometimes necessary when copying
and pasting BASIC listings.

This is intended for pasting in BASIC listings at the BASIC prompt. No
guarantees it will work properly anywhere else, but you might get
lucky...

To make it easy to paste text in from modern applications, newlines
(`CR LF`, `LF CR`, `LF`) are translated into `CR` (ASCII 13), and £ is
translated into BBC-style £ (ASCII 95). ASCII characters between 32
and 126 are passed on as-is.

Other characters are not currently supported.

## Copy to clipboard

Copy text output using `OSWRCH Copy Text`. This works a bit like
`*SPOOL`, in that once activated it captures anything printed via
`OSWRCH` until deactivated.

It's explicitly described as `Copy Text`, because it strips out VDU
control codes and normalizes line endings. You stand a good chance of
being able to paste the result into a word processor or text editor or
what have you.

(A future version will probably sport a `Copy Binary` version, which
grabs everything. Though I'm not entirely sure how useful this will
be.)

-----

# Debug version - BBC development, debugger, HTTP API, etc. #

The debug version has some additional functionality that might prove
useful for developing BBC software.

[Extra instructions for the debug version](doc/Debug-version.md).

-----

# Building

This repo has submodules, so you have to clone it to build it. The
source code archives in the releases page won't work. (This is a
GitHub bug and there doesn't appear to be any way around it.)

The submodules are referred to by https. Before cloning, you can use
`git config --global url.ssh://git@github.com/.insteadOf
https://github.com/` to have them cloned over SSH instead, if you have
a GitHub login.

To clone the repo:

    git clone --recursive https://github.com/tom-seddon/b2.git
	
If you're reading this after already cloning it:

    git submodule init
	git submodule update

Regarding branches, `master` should always build (and shouldn't
contain anything too outrageously half-baked) and `build` is the
branch used by the CI servers to prepare releases. `master` is
periodically merged into `build` when things are settled down.

`wip/*` is stuff that's being worked on.

Once you're all set up:

[Instructions for building on Windows](doc/Building-on-Windows.md).

[Instructions for building on OS X](doc/Building-on-OSX.md).

[Instructions for building on OS X or Linux](doc/Building-on-Unix.md).

[Notes about the 6502 test suites](doc/6502-test-suites.md).

-----

# Licence

## `etc`, `submodules`

Please consult folders and files for more info.

The contents of `etc` is all stuff that's freely available and
distributable, included in the repo so it's self-contained, since not
every dependency can be added as a git submodule.

## `experimental`, `src`

Copyright (C) 2016-7 by Tom Seddon

Licence: GPL v3.

-----

