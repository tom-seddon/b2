[Windows Build Status: ![status](https://ci.appveyor.com/api/projects/status/3sdnt3mh1r61h74y?svg=true)](https://ci.appveyor.com/project/tom-seddon/b2)

[OS X Build Status: ![status](https://travis-ci.org/tom-seddon/b2.svg?branch=master)](https://travis-ci.org/tom-seddon/b2)

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

## Windows

Unzip to a folder of your choice and run `b2.exe`.

### Windows 7 ###

If you have trouble with slow startup and poor performance on Windows
7, try running `b2.exe` from the command line with the `--timer`
option: `b2 --timer`.

This setting is sticky, and will be saved on exit for future runs. So
after you've done this once, you can just run it from Windows Explorer
in future.

## OS X

Open the dmg, drag b2 to your Applications folder and run it from
there.

## Linux

Please follow [the building instructions](#building).

# Guided tour

Double-click the icon: `b2` on OS X, `b2.exe` on 64-bit Windows, or
`b2_32bit.exe` on 32-bit Windows.

Everything you need to get started is provided, and the familiar `BBC
Computer 32K` message should appear straight away.

## Load a disc

Go to `Drive 0` or `Drive 1` on the `File` menu to load a disc. Use
the `Disc image...` or `Direct disc image...` option to select an .ssd
file, then hit Shift+Break (PC Shift + PC F11) to start it.

`Disc image...` loads the file into memory. The file isn't updated
when changes are made in the emulator (use `Save` to do that), and
changes made to the file won't be seen in the emulator (reload the
disc image for that).

`Direct disc image...` accesses the file directly for each read or
write. Any changes made in the emulator are immediately made to the
file, and vice versa.

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
  
## Save states

Use `File` > `Save state` to save your place, and `File` > `Load last
state` to reload it. (All state is saved, including disc contents.)
Use `Tools` > `Saved states` to see the list of states saved; click
`Load` to reload one, or `Delete` to delete it.

Because the disc contents are outside the emulator's control, you
can't save a state when a direct disc image is loaded.

## Timeline

Use the timeline functionality to record a sequence of events for
later playback. *This functionality is a work in progress* - so it's
not super useful yet. But, on Windows and Linux, having recorded a
timeline, you have the option of creating a video from it.

Use `Tools` > `Timeline...` to show the Timeline window. Click
`Record` to start recording; events are recorded to the timeline,
along with occasional saved states. Reload a saved state from the
timeline to rewind the timeline back to that point and continue
recording.

Having recorded a timeline, click the `Video` button to produce a
video starting from that point. There are two output formats
available: 50Hz 1:1 BBC pixels, and 50Hz 2:1 BBC pixels. Note that the
2:1 output does nothing advanced; it's just there so that when
uploaded to YouTube it comes out as a 1080p50 video.

Click `Replay` to play the timeline back. 

As with save states, some functionality may cause recording to be
disabled.

## Options

Use `Tools` > `Options...` to bring up the options dialog, letting you
fiddle with screen size, emulated speed, and so on.

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

### Sound options

`BBC volume` controls BBC sound chip volume. `Disc volume` controls
volume of the disc drive noises.

When `Power-on tone` is unticked, the Brrrr... power-on tone will be
silenced. This is not very authentic! - but it might make things a bit
less annoying when you find yourself hearing it a lot. (You will still
get the beep.)

## Customize keyboard keys

`Tools` > `Command Keys...` lets you select shortcut keys for many of
the menu options and window buttons.

By default, BBC keys take priority. If a key is both a shortcut key
and a BBC key, its shortcut will be ignored. Tick `Edit` > `Prioritize
command keys` to change this, so the emulator will process shortcut
keys before processing BBC keys.

(The results aren't always perfect. For example, suppose you assign
Shift+F5 to a command, and then use that combination: when you press
Shift, the emulated BBC will see the Shift press, even though the F5
will then be ignored when the emulator recognises the combination.)

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

Each keymap also has a `Prioritize command keys` checkbox, which is
the setting for the `Edit` > `Prioritize command keys` option (see
above) when the keymaps is first selected. (You can use the menu item
to change it afterwards.)

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

### External memory

Check the `External memory` box to add a 16MByte paged RAM 1MHz bus
device. Paging registers are at &FC00 (LSB) and &FC01 (MSB), and the
corresponding page of the memory appears in page &FD.

(The external RAM can't be enabled in conjunction with the Opus
Challenger disc interface, as both devices use page &FD.)

### BeebLink

Tick the `BeebLink` box to add an emulated
[BeebLink](https://github.com/tom-seddon/beeblink) widget to the
emulated user port. For more details, see the
[BeebLink notes](./docs/BeebLink.md).

## Non-volatile RAM

The Master 128 has non-volatile RAM. If you're using an emulated
Master 128, use `File` > `Save default NVRAM` to save the current
non-volatile RAM state to the config file, so it'll be restored on the
next run.

Use `Tools` > `Reset default NVRAM` to reset to default settings.
(This may be preferable to using the MOS's reset functionality, should
it be needed, because these default settings aren't completely
useless...)

The saved contents will also be restored on the next `File` > `Hard
reset`, or when using `File` > `Change config' to change to a Master
128 config.

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

Regarding branches, `master` should always build, and shouldn't
contain anything outrageously half-baked - it's the branch used by the
CI servers to prepare releases.

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

