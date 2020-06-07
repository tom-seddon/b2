# Overview

A brief summary of the main features of the emulator.

## Load a disc

If the disc auto-boots, use `Run` on the `File` menu to load a disc
and have the emulator auto-boot it. Use
the `Disc image...` or `Direct disc image...` option to select an .ssd
file, and it will start automatically.

To load a disc without auto-booting, go to `Drive 0` or `Drive 1` on
the `File` menu instead.

`Disc image...` loads the file into memory. The file isn't updated
when changes are made in the emulator (use `Save` to do that), and
changes made to the file won't be seen in the emulator (reload the
disc image for that).

`Direct disc image...` accesses the file directly for each read or
write. Any changes made in the emulator are immediately made to the
file, and any changes made to the file are immediately visible in the
emulator.

## Change config

The default setup is a BBC Model B with Acorn 1770 DFS and 16K
sideways RAM. The `Hardware` menu lets you select something different.

## Change keyboard mapping ##

The default keyboard layout tries to map PC keys to their BBC
equivalent by position - typically what you want for games.

Use the `Keyboard` menu to select a different mapping. Some games
might be better with the `Default (caps/ctrl)` layout, which sets PC
Left Ctrl to BBC Caps Lock, PC Left Alt to BBC Ctrl, and PC Caps Lock
to BBC Ctrl.

For typing, you may prefer the two character map options, which try to
map PC keys to BBC keys based on the character, so that PC Shift+0
gives you `)` and so on. `Default UK` is for UK-style keyboards, and
`Default US` is for US-style keyboards.
  
To get BBC Copy, use PC End (fn+Cursor Right on a Macbook Pro).

### US-style keyboards

If using `Default` or `Default (caps/ctrl)`, the PC backslash key
produces BBC @. Press PC Home (fn+Cursor Left on a Macbook Pro) to get
BBC backslash.

If using `Default US` character map, press PC ` to get BBC pound sign.

## Save states

Use `File` > `Save state` to save your place, and `File` > `Load last
state` to reload it. (All state is saved, including disc contents.)
Use `Tools` > `Saved states` to see the list of states saved; click
`Load` to reload one, or `Delete` to delete it.

(Note that if you load a direct disc image, save states become
disabled, as the disc contents can be changed from outside the
emulator.)

## Timeline

Use the timeline functionality to record a sequence of events for
later playback. *This functionality is a work in progress* - so it's
not super useful yet. But having recorded a timeline, you have the
option of creating a video from it.

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

`Keyboard` > `Command Keys...` lets you select shortcut keys for many of
the menu options and window buttons.

By default, BBC keys take priority. If a key is both a shortcut key
and a BBC key, its shortcut will be ignored. Tick `Keyboard` >
`Prioritize command keys` to change this, so the emulator will process
shortcut keys before processing BBC keys.

(The results aren't always perfect. For example, suppose you assign
Shift+F5 to a command, and then use that combination: when you press
Shift, the emulated BBC will see the Shift press, even though the F5
will then be ignored when the emulator recognises the combination.)

## Customize keyboard layout

Click `Keyboard` > `Keyboard layouts...` to bring up the keyboard
layout dialog. Select the keymap of interest in the left hand list,
and use the BBC keyboard map to edit which PC keys map to which BBC
keys.

(Note that the BBC keyboard map always shows the Master 128 keypad,
though the keypad is only active in Master 128 mode.)

Hover over a BBC key to see a little `Edit PC Keys` popup, showing
which PC keys correspond to it. While the popup is visible, press a
key to make that PC key map to the Beeb key.

To remove a mapping, click the BBC key to make the `Edit PC Keys`
popup stick. Then click the X button next to the PC key's name.

Each keymap also has a `Prioritize command keys` checkbox, which is
the setting for the `Keyboard` > `Prioritize command keys` option (see
above) when the keymaps is first selected. The tick box is the default
setting; you can always use the menu item to change it afterwards.

To create a new keymap, click the `New...` button to create one that's
a copy of one of the default keymaps, or click the `Copy...` to create
one that's a copy of one of the ones in the list.

## Customize hardware

The hardware menu by default lists several types of BBC B (with
different disk interfaces), B+, B+128, Master 128 with MOS 3.20 and
Master 128 with MOS 3.50. `Hardware` > `Configs` lets you edit this
list and choose what each config includes - ROMs, sideways RAM status,
and extra hardware.

Select the config to modify in the list on the left hand side. The
right hand side will display the ROM slot contents, and tick boxes for
extra hardware.

Click the `...` button next to a sideways ROM slot/OS ROM to select
the ROM image. You can load a file off disk, or choose one of the
various standard ROMs that are supplied with the emulator.

Tick the box in the RAM column to make that sideways slot writeable.

Use the up/down arrows to rearrange the ROM contents, changing the
priorities.

Items of optional hardware are as follows:

- Check the `External memory` box to add a 16MByte paged RAM 1MHz bus
  device. Paging registers are at &FC00 (LSB) and &FC01 (MSB), and the
  corresponding page of the memory appears in page &FD.
  
  (The external RAM can't be enabled in conjunction with the Opus
  Challenger disc interface, as both devices use page &FD.)
  
- Tick the `BeebLink` box to add an emulated
  [BeebLink](https://github.com/tom-seddon/beeblink) widget to the
  emulated user port. For more details, see the
  [BeebLink notes](./BeebLink.md).
  
- Tick the `Video NuLA` box to add a
  [Video NuLA](https://www.stardot.org.uk/forums/viewtopic.php?f=3&t=12150).
  (This is ticked by default, as it's very unlikely to cause a
  problem.)

Changes to a configuration don't affect the running Beeb until you do
a `File` > `Hard Reset` (if you're editing the current config) or a
`File` > `Configuration`.

To create a new configuration, click the `New...` button to create one
based off one of the default configs, or the `Copy...` button to
create one that's a copy of one of the ones in the list.

The `Delete` button will delete the currently selected config.

## Non-volatile RAM

The Master 128 has non-volatile RAM. If you're using an emulated
Master 128, use `File` > `Save default NVRAM` to save the current
non-volatile RAM state to the config file, so it'll be restored on the
next run.

Use `Tools` > `Reset default NVRAM` to reset to default settings.
(This may be preferable to using MOS 3.20's reset functionality,
should it be needed, because the MOS resets the CMOS to fairly useless
values.)

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
