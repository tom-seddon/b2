# Overview

A brief summary of the main features of the emulator.

## Load a disc

If the disc auto-boots - and most that you'll find on the internet
will - use `Run` on the `File` menu to load a disc and have the
emulator auto-boot it. Or drag and drop the disk image from the File
Explorer.

`Disc image...` accesses the file directly. While the disc motor
indicator is on, the emulator has the file open, and will read and
write it. When the disc motor indicator switches off, the file will
reflect any changes made. You can also modify the image (using tools
such as [beebasm](https://github.com/stardot/beebasm/)) and the
changes will be picked up on the next access.

`In-memory disc image...` or drag and drop loads the file into memory.
The file isn't updated when changes are made in the emulator (use
`Save` to do that), and changes made to the file won't be seen in the
emulator (reload the disc image for that).

To load a disc without auto-booting, go to `Drive 0` or `Drive 1` on
the `File` menu instead, and use `Disc image...` or `In-memory disc
image...`. Or, to create a new blank disc, use `New disc image` or
`New in-memory disc image`.

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

## Joysticks

If you have compatible game controllers connected, use the Joysticks
menu to pick which ones are used for the BBC.

(Joysticks are referred to by name, and are numbered if you have
several of the same type connected. At the bottom of the joysticks
menu is an entry showing you the name of the last used joystick: the
one on which a button was last pressed. This may help you figure out
which is which.)

### Analogue

Joysticks 0 and 1 refer to the analogue joysticks.

Assuming an Xbox/Playstation-style game controller, left thumbstick
and d-pad correspond to the BBC joystick X and Y axes, and the main 4
buttons and the 2 shoulder buttons correspond to the BBC joystick
button. (Other buttons or triggers are unused.)

You can use the same gamepad for both BBC joysticks. In this case, by
default, the left thumbstick and left shoulder button control BBC
joystick 0, and the right thumbstick and right shoulder button control
BBC joystick 1. (The other buttons and triggers are unused.) Tick
`Swap shared joysticks` to have the assignment the other way round.

### Digital (Master 128/Master Compact only)

Digital joysticks are only available on Master 128 and Master Compact.

Assuming an Xbox-/Playstation-style game controller, left thumbstick
and d-pad correspond to the digital X and Y axes, the A and X buttons
correspond to fire button 1, and the B and Y buttons corresponding to
fire button 2 (when supported).

For Master 128, the digital joystick is connected via a Retro Hardware
ADJI cartridge (mostly compatible with the Slogger Switched Joystick
Interface cartridge, designed for the Electron). See the Customize
hardware section for how to enable this. This isn't supported by the
OS, but some Electron games do support it.

For Master Compact, the digital joystick is connected to the 9-pin
joystick connector on the back of the machine. This is supported by
the OS in the usual fashion.

## Mouse

If the current setup includes an emulated mouse (see the Customize
hardware section below), select `Capture mouse` to have mouse input go
to the emulated BBC. The system mouse cursor will be hidden,
indicating that mouse input is going to the BBC instead.

To cancel mouse capture, switch away from the b2 window using the
usual OS keyboard shortcut. You can also assign a keyboard shortcut to
the `Capture mouse` command - see the Customize keyboard keys section
below.

(Note that unlike other commands, `Capture mouse` is special, and its
keyboard shortcut will always be handled even if it would overlap with
ordinary BBC input.)

If `Capture on click` is ticked, the mouse will be captured
automatically if you click on the emulator display.

## Save states

Use `File` > `Save state` to save your place, and `File` > `Load last
state` to reload it. (All state is saved, including disc contents.)
Use `Tools` > `Saved states` to see the list of states saved; click
`Load` to reload one, or `Delete` to delete it.

Save states are only available when the emulator has complete control
over the entire state of the emulated BBC. That means the following
restrictions apply:

- all disk images loaded must be in-memory disk images

- the current hardware config must have BeebLink disabled

## Timeline

Use the timeline functionality to record a sequence of events for
later playback. *This functionality is a work in progress* - so it's
not super useful yet. But having recorded a timeline, you have the
option of creating a video from it.

(The timeline uses the save state functionality, so the same
restrictions apply.)

Use `Tools` > `Timeline...` to show the Timeline window. Click
`Record` to start recording; events are recorded to the timeline,
along with occasional saved states. Reload a saved state from the
timeline to rewind the timeline back to that point and continue
recording.

Having recorded a timeline, click the `Video` button to produce a
video starting from that point. Select the combination of resolution
and audio bitrate from the popup.

You'll always get two resolution options: 1:1 BBC pixels, and 2:1 BBC
pixels. (Both are 50 Hz.) The 2:1 option doesn't do anything remotely
clever, and exists only so that when uploaded to YouTube the video
comes out as HD 1080p50.

Depending on system, you may get multiple audio bitrate and/or output
format options (apologies for inconsistency - I hope to improve this).
FLAC is best quality, and failing that higher Kb/sec = better. Pick
the best output that works with whichever program you're using to play
back.

Click `Replay` to play the timeline back. 

## Screenshot

Use `File` > `Save screenshot...` to save a PNG format screenshot to
disk.

Use `Edit` > `Copy screenshot` to copy a screenshot to the clipboard.
(On Windows and macOS, this will just work. On Linux, this relies on
the [`xclip`](https://github.com/astrand/xclip) utility, which you
will need to have installed. Available from the package manager on
Ubuntu, and probably on most other distributions too.)

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

`Emulate interlace`, which you probably don't want to tick,
approximates the visual effect of an interlaced display when
interlacing is switched on.

### Screenshot options

`Correct aspect ratio` and `Filter display` have the same effect as
the corresponding Display options, but apply to screenshots only.

### Sound options

`BBC volume` controls BBC sound chip volume. `Disc volume` controls
volume of the disc drive noises.

When `Power-on tone` is unticked, the Brrrr... power-on tone will be
silenced. This is not very authentic! - but it might make things a bit
less annoying when you find yourself hearing it a lot. (You will still
get the beep.)

### UI options

If you find the UI text a bit small, use the GUI Font Size setting to
make it larger. (This only affects the emulator UI, not the BBC
display.)

### HTTP Server options

See [the file association section](./File-Association.md).

## Customize keyboard keys

`Keyboard` > `Command Keys...` lets you select shortcut keys for many of
the menu options and window buttons.

By default, BBC keys take priority, so if a key is both a shortcut key
and a BBC key, its shortcut will be ignored. Tick `Keyboard` >
`Prioritize command keys` to change this, so the emulator will process
shortcut keys before processing BBC keys.

The results aren't always perfect. For example, suppose you assign
Shift+F5 to a command, and then use that combination: when you press
Shift, the emulated BBC will see the Shift press, even though the F5
will then be ignored when the emulator recognises the combination.

(There's one exception to this rule: `Capture mouse`. If it has a
shortcut, that shortcut takes priority over any BBC input.)

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
different disk interfaces), B+, B+128, Master 128 with MOS 3.20 or MOS
3.50, Master Compact with MOS 5.00 or MOS 5.10, and Olivetti PC 128 S.
`Hardware` > `Configs` lets you edit this list and choose what each
config includes - ROMs, sideways RAM status, and extra hardware.

Select the config to modify in the list on the left hand side. The
right hand side will display the ROM slot contents, and tick boxes for
extra hardware.

Click the `...` button next to a sideways ROM slot/OS ROM to select
the ROM image. You can load a file off disk, or choose one of the
various standard ROMs that are supplied with the emulator.

Use the `Type` option in the `...` menu to select the ROM mapper type,
necessary if the ROM is larger than 16 KB. The following options are
available, each listed with a (non-exhaustive) selection of a few ROMs
that require that type.

- `16 KB` - ordinary ROM, 16 KB or smaller
- `Inter-Word (32 KB)` - Computer Concepts Inter-Word, AMX Design,
  Beebug Master ROM
- `Inter-Base (64 KB)` - Computer Concepts Inter-Base, PMS The
  Publisher,
- `Spellmaster (128 KB)` - Computer Concepts Spellmaster, Computer
  Concepts Mega3
- `Quest Paint (32 KB)` - Watford Quest Paint, Watford ConQuest,
  Watford PCB Designer
- `Wapping Editor (64 KB)` - The Wapping Editor
- `TED (32 KB)` - Watford TED (Teletext Editor)
- `PRES ABE+ (32 KB)` - PRES Advanced BASIC Editor Plus
- `PRES ABE (32 KB)` - PRES Advanced BASIC Editor

If the ROM type is 16 KB, you can tick the box in the RAM column to
make that sideways slot writeable.

Use the up/down arrows to rearrange the ROM contents, changing the
priorities.

Items of optional hardware are as follows:

- Check the `External memory` box to add a 16MByte paged RAM 1MHz bus
  device. Paging registers are at &FC00 (LSB) and &FC01 (MSB), and the
  corresponding page of the memory appears in page &FD.
  
  (The external RAM can't be enabled in conjunction with the Opus
  Challenger disc interface, as both devices use page &FD.)
  
- Tick the `Mouse` box to add an emulated AMX mouse to the user port.
  
- Tick the `BeebLink` box to enable support for
  [BeebLink](https://github.com/tom-seddon/beeblink). For more
  details, see the [BeebLink notes](./BeebLink.md).
  
- Tick the `Video NuLA` box to add a
  [Video NuLA](https://www.stardot.org.uk/forums/viewtopic.php?f=3&t=12150).
  (This is ticked by default, as it's very unlikely to cause a
  problem.)
  
- If using a Master 128, tick the `Retro Hardware ADJI` to add a Retro
  Hardware ADJI cartridge (upcoming modern remake of the
  [Slogger Switched Joystick Interface](https://www.computinghistory.org.uk/det/32296/Slogger%20Switched%20Joystick%20Interface/).
  Select the DIP switch settings from the list box. The ADJI will use
  the digital joystick selected in the joysticks menu.
  
There are also some second processor options, for models that support
this:

- `None` for no second processor

- `6502 cheese wedge` for an external 3 MHz 6502 second processor.
  (The Acorn documentation doesn't specify what specific type of CPU
  these cntain, but in practice they all seem to come with a Rockwell
  65C02)

- `Master Turbo` for a 4 MHz 65C102 second processor. (With a Master,
  this appears on the internal Tube, replicating the Master Turbo;
  with a B/B+, this setup corresponds to a
  [universal second processor](http://chrisacorns.computinghistory.org.uk/8bit_Upgrades/Acorn_ANC21_Uni2Proc.html)
  with a Master Turbo board fitted)
 
(In either case, as per the on-screen reminder: with a Master, a
`*CONFIGURE TUBE` may be required to get the OS to detect the second
processor. With a B/B+, be sure to install a ROM installed with the
Tube host code in it, such as the Acorn 1770 DFS.)

Changes to a configuration don't affect the running Beeb until you do
a `File` > `Hard Reset` (if you're editing the current config) or
select the updated configuration from the `Hardware' menu.

To create a new configuration, click the `New...` button to create one
based off one of the default configs, or the `Copy...` button to
create one that's a copy of one of the ones in the list.

The `Delete` button will delete the currently selected config.

## Non-volatile CMOS RAM/EEPROM

If you're using an emulated Master 128, Master Compact or PC 128 S,
use `File` > `Save CMOS/EEPROM contents` to save the current
CMOS/EEPROM contents for the current config. (For technical reasons,
this doesn't currently happen automatically - sorry!)

These affect the values used when using `File` > `Hard reset` or when
re-selecting the current config from the Hardware menu.

(Each hardware config on the Hardware menu has its own independent
set of CMOS/EEPROM contents.)

Use `Tools` > `Reset CMOS/EEPROM` to reset the saved settings to
reasonable default settings. Again, use `File` > `Hard reset` to see
the effect.

## Copy to clipboard

Copy text output using `Copy OSWRCH nexn output`. It works a bit like
`*SPOOL`, in that once activated it captures anything printed via
`OSWRCH` until deactivated.

It's explicitly described as "text output", because it strips out VDU
control codes and normalizes line endings. You stand a good chance of
being able to paste the result into a word processor or text editor or
what have you.

There are 3 text translation modes available on the `Copy options`
submenu:

- `No translation` - BBC ASCII chars come through as-is in the
  clipboard data. BBC £ chars will turn into `
  
- `Translate £ only` (default) - BBC £ chars will come through as £

- `Translate Mode 7 chars` - characters will be translated to symbols
  that resemble their Mode 7 appearance. (For example, `{` will come
  through as `¼`.) Perfect results from this are not guaranteed
  
When `Handle delete` is ticked (which is the default setting), the
emulator will try to handle delete (ASCII 127) chars properly and
remove previous chars when they're printed. This won't handle
everything perfectly, but if you're copying stuff you're typing in at
the BASIC prompt then it will do about the right thing.

(When unticked, the delete chars are stripped out entirely, same as
other control codes.)

## Paste from clipboard

Paste text from the clipboard to the BASIC prompt using `OSRDCH Paste`
and `OSRDCH Paste (+Return)`. The `(+Return)` version effectively
presses Return at the end, which is sometimes necessary when copying
and pasting BASIC listings.

This is intended for pasting in BASIC listings at the BASIC prompt. No
guarantees it will work properly anywhere else, but you might get
lucky...

To make it easy to paste text in from modern applications, newlines
(`CR LF`, `LF CR`, `LF`) are translated into `CR` (ASCII 13).

`£` is automatically translated into BBC-style £ (ASCII 96).

The Mode 7 characters produced by the `Translate Mode 7 chars` will
automatically be translated into the corresponding BBC chars, so the
data will round trip correctly.

Other non-ASCII characters are not currently supported.

## Printer

Click `Printer` > `Parallel printer` to attach an emulated printer to
the BBC. Printed data is buffered as it is printed; use `Printer` >
`Save printer buffer...` to save the raw data to a file. 

`Printer` > `Reset printer buffer` resets the printer buffer,
discarding the current contents.

`Printer` > `Copy printer buffer text` copies the printer buffer as
text, stripping out any BBC control codes and translating line
endings. Note that this won't properly strip out Epson-style ESC
control codes though!

As when copying text, there are copy options available on the `Copy
options` submenu. (See the Copy to clipboard section above.) One thing
to note though is that delete handling is not as useful as with text
output because 127 chars are stripped from the output unless
explicitly sent using VDU 1.
