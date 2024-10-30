# Debug version

Run the debug version on Windows by running `b2_Debug.exe`, as
extracted from the distribution zip file.

Run the debug version on macOS by copying `b2 Debug` from the dmg file
to your Applications folder and running it.

When building from source - your only option on Linux, sorry about
that - get the debug version by building `Debug` (unoptimized) or
`RelWithDebInfo` (optimized). (As is probably obvious:
`RelWithDebInfo` will be more efficient, and `Debug` will be slower,
but easier to step through in a debugger. But either should maintain
100% BBC Micro speed on any decent modern PC.)

However you run it, the debug version has a range of extra
debug-related functionality, accessible from the `Debug` menu.

# Integrated debugger

Find all this stuff in the `Debug` menu.

## General debugging ##

Use `Stop` to stop the entire system in its tracks.

`Run` will set it going again.

The system is emulated as a unit: host processor, second processor, TV
output, disk drives, VIA timers, and so on. `Stop` stops absolutely
everything, and `Run` sets the entire system going again.

## Paging overrides

Where it's relevant, debug windows have a row of indicators along the
top, showing the current paging settings (selected ROM, shadow RAM,
etc.).

You can override these for the window, to see the state as if some
other paging setup were in effect. (This only affects the debug
window's view of memory. The emulated state doesn't change.) To do
this, click on one of the captions and select the override value you
want to use.

Overridden settings are displayed with a `!` suffix.

If there's a `MOS's view` checkbox, tick it to show the memory as
would be seen by the OS's VDU drivers rather than (as is the default)
by ordinary code. For technical reasons, this flag isn't integrated
very well into the debugger or the paging override mechanism.

## Byte popup ##

Most places where you see the value of a byte in the debugger UI, or
an address, you can right click the address or value to get a popup UI
relating to that byte.

Use the tickboxes to add/remove read, write or execute breakpoints.
Breakpoints can be set for the address or for the byte: address
breakpoints are hit when that address is read/written/executed,
regardless of paging settings, and byte breakpoints relate to the
specific byte in question, in whichever bank it is.

(When a byte's value is shown in the disassembly window, it will be
displayed with a coloured background if there's a breakpoint set for
that address or that byte.)

Tick `Reveal address...` to visit that address in a visible
disassembly or memory view window (select the desired window from the
list). The window in question will be made to view that address.

Tick `Reveal byte` to visit that particular byte in a visible
disassembly or memory view window (select the desired window from the
list). The window's paging overrides will be adjusted to ensure that
particular byte is visible.

The right click functionality is not yet 100% consistently available,
but this will improve.

## Address syntax

Addresses can be entered in decimal, hex, or octal.

Decimal is assumed, if no prefix is provided.

Prefix hex numbers with `&`, `$` or `0x`.

Prefix octal numbers with `0`, like C.

## Address suffixes

Addresses may be annotated with ` followed by a case-sensitive suffix,
indicating which paging settings are in force when deciding what's
visible at that address. The possible suffixes are as follows.

Host memory:

- `m` - main RAM ($0000...$7fff)
- `0` - `f` - paged ROM ($8000...$bfff)
- `s` - shadow RAM (B+/Master only) ($3000...$7fff)
- `n` - ANDY (B+/Master only) (B+: $8000...$afff; Master: $8000...$8fff)
- `h` - HAZEL (Master only) ($c000...$dfff)
- `o` - OS ROM ($c000...$ffff)
- `i` - I/O area ($fc00...$feff)
- 'w', 'x', 'y', 'z' - ROM mapper regions 0-3
- 'W', 'X', 'Y', 'Z' - ROM mapper regions 4-7

Parasite memory:

- `p` - parasite RAM ($0000...$ffff)
- `r` - parasite ROM ($f000...$ffff)

When entering an address with a suffix, appropriate paging overrides
will be selected to ensure the requested byte is visible.

You can supply multiple address suffix codes. Order is relevant, as
certain suffixes imply the settings for other sufixes. Override this
with further suffixes if required.

- ROM bank suffix ('0' - 'f') implies ROM mapper bank `w`
- (B+/Master) ROM bank suffix (`0` - `f`) implies ANDY disabled
- (Master) OS ROM (`o`) implies HAZEL disabled

Inappropriate suffixes are ignored.

## ROM Mappers

The MAME source is a quite readable definition of how the various ROM
mapper PLDs fundamentally work:
https://github.com/mamedev/mame/blob/master/src/devices/bus/bbc/rom/pal.cpp

The applicable address suffixes affect the mapped region as follows.

- `16 KB` - mapper region irrelevant
- `Inter-Word` - `w` - `x` select 16 KB region visible at $8000-$bfff
- `Inter-Base` - `w` - `z` select 16 KB region visible at $8000-$bfff
- `Spellmaster` - `w` - `z` and `W` - `Z` select 16 KB region visible
  at $8000-$bfff
- `Quest Paint` - `w` - `z` select 8 KB region visible at $a000-$bfff
  ($8000-$9fff is fixed)
- `Wapping Editor` - `w` - `z` and `W` - `Z` select 8 KB region
  visible at $a000-$bfff ($8000-$9fff is fixed)

# Debugger windows

## `Tracing` ##

The trace functionality records all CPU activity, and selected other
events of interest. The recorded data can be saved to a text file for
later perusal.

There are multiple options for starting the recording, indicating what
will happen when `Start` is clicked:

* `Immediate` - recording will start straight away
* `Return` - recording will start once the
  Return key is pressed
* `Execute $xxxx` - recording will start once the PC is equal to the
  given address. Note that this currently goes only by address -
  address suffixes aren't supported
* `Write $xxxx` - recording will start when the given address is
  written to. Writes to any address can be trapped, even if that write
  has no effect, e.g., because the area is ROM. Note that this
  currently goes only by address - address suffixes aren't supported

Once the trace starts, you can always click `Stop` to end it, but
there are additional options for the trace end condition:

* `By request` - stop only when `Stop` is clicked
* `OSWORD 0` - stop when the BBC executes an OSWORD with A=0 (read
  input line)
* `Cycle count` - stop when the trace has been going for a particular
  number of cycles
* `Write $xxxx` - recording will stop when the given address is
  written to. (Same rules as for the corresponding start condition.)
  When using `Write $xxxx` for start and end conditions, the same
  address can be used for both
* `BRK` - recording will stop the next time `BRK` (opcode $00) is
  encountered

(`Return` and `OSWORD 0` often go together, because this works well
for tracing code CALLed from the BASIC prompt.)

By default, only the last 25-30 seconds of activity will be kept
(usually corresponding to roughly a 1GByte output file). Tracing can
be left running indefinitely and the emulator will use a fixed amount
of memory.

Tick `Unlimited recording` to have the emulator store all events, so
longer periods can be traced. Each emulated second takes up ~10MBytes
of RAM and adds ~35MBytes to the output file.

On Windows, you can tick the Windows-specific `Unix line endings`
option to have the trace output saved with Unix-style line endings.
This means slightly quicker saving, and slightly better performance in
Emacs (since you can use the load literally option).

Tick `Include cycle count` to add a cycle count column in the output.
By default the cycle count relative to the start of the trace is
shown; tick `Absolute cycle count` to have the absolute count shown
(not typically terribly useful).

Tick `Include register names` to include register names in the output.
This takes up a few more columns but you may find it easier to read.

The `Cycles output` options let you specify how the cycle count is
displayed in the output. `Absolute` is the absolute number of emulated
cycles since the BBC was rebooted, probably not very useful;
`Relative` counts the start of the trace as cycle 0, and goes from
there; `None` omits the cycle count entirely, which can be useful if
trying to diff the output from multiple runs.

Tick `Auto-save on stop` to have the trace saved automatically when
the trace stops. Select the file name to save to. It will be
overwritten, no questions asked, if the trace finishes without being
cancelled.

Otherwise, the trace will be stored in memory, and you can click
`Save` to save it to a file.

The trace output is along these lines, here shown with relative cycle
count and register names included:

```
H      63  m`17cc: lda #$81                 A=81 X=65 Y=00 S=f0 P=Nvdizc (D=81)
H      65  m`17ce: ldy #$ff                 A=81 X=65 Y=ff S=f0 P=Nvdizc (D=ff)
H      67  m`17d0: jsr $1bba                A=81 X=65 Y=ff S=ee P=Nvdizc (D=17)
H      73  m`1bba: stx $1b8f [m`1b8f]       A=81 X=65 Y=ff S=ee P=Nvdizc (D=65)
H      77  m`1bbd: ldx #$04                 A=81 X=04 Y=ff S=ee P=nvdizc (D=04)
H      79  m`1bbf: jmp $1b75                A=81 X=04 Y=ff S=ee P=nvdizc (D=04)
H      82  m`1b75: sta $1b91 [m`1b91]       A=81 X=04 Y=ff S=ee P=nvdizc (D=81)
H      86  m`1b78: lda $f4 [m`00f4]         A=04 X=04 Y=ff S=ee P=nvdizc (D=04)
H      89  m`1b7a: pha                      A=04 X=04 Y=ff S=ed P=nvdizc (D=04)
H      92  m`1b7b: lda #$05                 A=05 X=04 Y=ff S=ed P=nvdizc (D=05)
H      94  m`1b7d: sta $f4 [m`00f4]         A=05 X=04 Y=ff S=ed P=nvdizc (D=05)
H      97  m`1b7f: sta $fe30 [i`fe30]       A=05 X=04 Y=ff S=ed P=nvdizc (D=05)
```

`H`/`P` indicates whether this line relates to the host or parasite.
Parasite trace output, when present, has the same format as the host
output, but offset so that it's easier... marginally... to scan by
eye:

```
P                                                                                       0  r`$f800: ldx #$00                 A=00 X=00 Y=00 S=fd P=nvdiZc (D=00)
P                                                                                       2  r`$f802: lda $ff00,X [r`$ff00]    A=ff X=00 Y=00 S=fd P=Nvdizc (D=ff)
P                                                                                       6  r`$f805: sta $ff00,X [r`$ff00]    A=ff X=00 Y=00 S=fd P=Nvdizc (D=ff)
H       3  o`$e364: lda #$40                 A=40 X=00 Y=00 S=fd P=nvdizc (D=40)
P                                                                                      11  r`$f808: dex                      A=ff X=ff Y=00 S=fd P=Nvdizc (D=d0)
H       5  o`$e366: sta $0d00 [m`$0d00]      A=40 X=00 Y=00 S=fd P=nvdizc (D=40)
P                                                                                      13  r`$f809: bne $f802                A=ff X=ff Y=00 S=fd P=Nvdizc (D=01)
P                                                                                      16  r`$f802: lda $ff00,X [r`$ffff]    A=fc X=ff Y=00 S=fd P=Nvdizc (D=fc)
H       9  o`$e369: sei                      A=40 X=00 Y=00 S=fd P=nvdIzc (D=a9)
P                                                                                      20  r`$f805: sta $ff00,X [r`$ffff]    A=fc X=ff Y=00 S=fd P=Nvdizc (D=fc)
H      11  o`$e36a: lda #$53                 A=53 X=00 Y=00 S=fd P=nvdIzc (D=53)
P                                                                                      25  r`$f808: dex                      A=fc X=fe Y=00 S=fd P=Nvdizc (D=d0)
P                                                                                      27  r`$f809: bne $f802                A=fc X=fe Y=00 S=fd P=Nvdizc (D=01)
H      13  o`$e36c: sta $fe8e [i`$fe8e]      A=53 X=00 Y=00 S=fd P=nvdIzc (D=53)
P                                                                                      30  r`$f802: lda $ff00,X [r`$fffe]    A=e6 X=fe Y=00 S=fd P=Nvdizc (D=e6)
P                                                                                      34  r`$f805: sta $ff00,X [r`$fffe]    A=e6 X=fe Y=00 S=fd P=Nvdizc (D=e6)
H      17  o`$e36f: jsr $e590                A=53 X=00 Y=00 S=fb P=nvdIzc (D=00)
P                                                                                      39  r`$f808: dex                      A=e6 X=fd Y=00 S=fd P=Nvdizc (D=d0)
P                                                                                      41  r`$f809: bne $f802                A=e6 X=fd Y=00 S=fd P=Nvdizc (D=01)
```

Host or parasite, the first column is the cycle count - then
instruction address, instruction, effective address, and register
values after the instruction is complete.

The bracketed `D` value is the value of the emulated CPU's internal
data register. For read or read-modify-write instructions, this is the
value read; for branch instructions, this is the result of the test
(1=taken, 0=not taken).

Addresses in the trace are annotated with an address suffix hint. Note
this is a hint, and while it's usually correct it may not actually
reflect the affected byte when the suffix is `r` (read parasite ROM,
write parasite RAM) or `i` on Master 128 (read either host I/O or MOS
ROM, write host I/O).

The `Other traces` checkboxes add additional hardware state output in
the file:

```
H   24939  m`21f5: stx $fe40 [i`fe40]       A=e3 X=00 Y=03 S=da P=nvdIZc (D=00)
H   24944  Port value now: 048 ($30) (%00110000) ('0')
H   24944  SystemVIA - Write ORB. Reset IFR CB2.
H   24945  m`21f8: lda $fe40 [i`fe40]       A=30 X=00 Y=03 S=da P=nvdIzc (D=30)
H   24946  PORTB - PB = $30 (%00110000): RTC AS=0; RTC CS=0; Sound Write=true
H   24950  SN76489 - write noise mode: periodic noise, 3 (unknown
H   24951  m`21fb: ora #$08                 A=38 X=00 Y=03 S=da P=nvdIzc (D=08)
H   24953  m`21fd: sta $fe40 [i`fe40]       A=38 X=00 Y=03 S=da P=nvdIzc (D=38)
```

The categories are hopefully mostly fairly obvious, except for perhaps
`Separators` (puts blank lines between 6845 scanline, so Emacs
paragraph commands can navigate between them), and the VIA `Extra`
flags (adds a huge pile of extra logging that you probably don't
want).

## `Pixel metadata` ##

Show a window that displays the RAM address of the pixel the mouse
cursor is over.

## `System Debug`

Shows cycle counts and execution state (running/halted) for the
system. The execution state covers the entire system (host processor,
second processor, hardware, TV output, disk access, etc.), which runs
as a unit.

## `Host 6502 Debug`, `Parasite 6502 Debug` ##

Show 6502 state: register info in the top half, and internal stuff
(cycle count, internal state, data bus address, etc.) in the bottom
half.

The absolute cycle count indicator always counts CPU cycles since the
emulated system was started. The relative cycle count indicator counts
at the same rate, but can also be reset to 0 using `Reset`.

When `Reset on breakpoint` is ticked (as is the default), the relative
cycle count is reset when a breakpoint is hit when running.
Breakpoints hit while single stepping won't affect the counter.

## `Host Memory Debug`, `Parasite Memory Debug` ##

Show a memory debug window. Click to edit memory.

To visit a new address, enter it in the `Address field` and press
Return or click `Go` to move the cursor there. Click `Go (realign)` to
also put that address in the left-hand column, changing the row
addresses - which may be more useful sometimes.

Click `Options` to get a popup options window.

From the options window, you can save a block of memory: specify start
address, then end address (exclusive, save as `*SAVE`) or size in
bytes, using the tick box to indicate which. Then click `Save
memory...` to specify the file to save it to.

Outsize end/size values will be clamped so that the saved region fits
in the $0000...$ffff (inclusive) region.

Memory is read with the paging overrides that are currently in effect,
with one restriction: memory-mapped I/O devices are bypassed, and you
see what's in the ROM or RAM behind them. (A future revision of the
emulator will fix this.)

## `Host Disassembly Debug`, `Parasite Disassembly Debug` ##

Show running disassembly.

With `Track PC` ticked - the default for disassembly window 1 - the
disassembly will track the program counter. Otherwise, enter an
address in the `Address` field to visit that address.

The disassembly window will make a stab at guessing effective
addresses, when not statically obvious. Its guesses are based on the
current state of the system, trying to take paging overrides into
account, and are for advisory purposes only.

It will also attempt to guess whether a branch will be taken or not
taken - taken branches have a `(taken)` indicator next to them. Again,
this is purely based on the current state of the system, so exercise
caution if the instruction in question is not the one about to be
executed.

Two step buttons allow instruction-resolution steeping: `Step In` will
run one instruction for the given CPU and then stop, and `Step Over`
will run until the given CPU reaches the next instruction visible.

As with `Run`/`Stop`, note that the the entire system runs (or not) as
a unit. When stepping one CPU, the other CPU will continue to run. It
isn't possible to step just one CPU at a time.

Also note that the system will stop when it hits a breakpoint on
either CPU, meaning a breakpoint for one CPU could interrupt a step
operation for the other. Debugging code on two CPUs simultaneously is
inevitably going to be a little inconvenient.

## `CRTC Debug`, `Video ULA Debug`, `System VIA Debug`, `User VIA Debug`, `NVRAM Debug`, `Analogue Debug` ##

Activate a debug window for the corresponding piece of BBC hardware
(if present), showing current register values and additional useful
info.

## `Paging debug`

Shows current paging settings for the host system.

## `Breakpoints`

Shows a list of all breakpoints. When you use the byte popup to set a
breakpoint on an address or byte, it will appear here.

If you ever use this window to alter the breakpoint flags for a
particular address or byte, that byte or address will continue to be
shown in the list even if all its breakpoint flags are cleared. When
this happens, click the `x` button to get rid of it.

## `Stack`, `Parasite Stack`

Shows a dump of the stack contents, byte by byte. Values below the
bottom of the stack are shown in a darker colour.

The `Addr` column shows the 2-byte address at that location in the
stack.

Assuming the `Addr` value was pushed by a `jsr` instruction, the `rts`
and `jsr` columns show the return address and `jsr` instruction
address respectively. These are shown in white if it looks like this
was the case (i.e., the `jsr` address actually points to a `jsr`
instruction), or in a darker column if not.

The paging override UI doesn't affect the values read from the stack,
but can affect whether a return address is detected as one or not.

## `Tube Debug`

Shows current Tube status: IRQ state, control register values,
contents and status for each FIFO.

## `Digital Joystick Debug`

Shows current digital joystick input state, if supported.

## `Keyboard Debug`

Lists current BBC keys pressed, keyboard scan state, and a diagram of
the keyborad matrix.

## `Mouse Debug`

Shows current mouse state. Four buttons permit generation of fake
mouse motion.

# Other debug-related options #

Additional debug options can be found in `Tools` > `Options` in the
`Display Debug Flags` section.

`Teletext debug` will overlay the teletext display with the value of
the character in each cell.

`Show TV beam position` will indicate where the TV beam is, with a
line starting just after its current position.

# Other debugger stuff

There may be other debug stuff that I haven't got round to documenting
yet.

# HTTP API

Use the HTTP API to control the emulator remotely. Use this from a
shell script or batch file, say, using `curl`, or from a program,
using an HTTP client library.

The emulator listens on port 48075 (0xbbcb) for connections from
localhost only.

The HTTP API also supports
[the file associaton mechanism](./File-Association.md), so the
documentation for that applies.

**The HTTP API is a work in progress, and may change.**

## HTTP endpoints

Names in capitals are argument placeholders. When listed as part of
the path, they are found by position, and are mandatory; those given
as part of the query string are found by name, and are optional.

Every method takes a window name, `WIN` - as seen in the title bar -
indicating which window to send the request to. In most cases, this
will probably be `b2`, the name of the initial window the emulator
creates on startup.

Parameters expecting numbers are typically hex values, e.g., `ffff`
(65535), or C-style literals, e.g., `65535` (65535), `0xffff` (65535),
`0177777` (65535).

Unless otherwise specified, the HTTP server assumes the request body's
`Content-Type` is `application/octet-stream`. And unless otherwise
specified, the response is an HTTP status code with no content.
Generally this will be one of `200 OK` (success), `400 Bad Request` or
`404 Not Found` (invalid request), or `503 Service Unavailable` (request
was valid but couldn't be fulfilled).

### `launch?path=PATH`

Launch the given file as if double clicked in Explorer/Finder/etc.

This endpoint is the one used by the file association mechanism, so
the same limitations apply. You don't get control over which window is
used, and the emulator auto-detects the file type from the name.

### `reset/WIN?config=CONFIG` ###

Reset the BBC. This is equivalent to a power-on reset. Memory is wiped
but mounted discs are retained.

`CONFIG`, if specified, is name of the config to used, as seen in the
`File` > `Change config` menu. The current config is used if not
specified.

Escaping the config name can be a pain. curl can do this for you on
the command line with the `-G` and `--data-urlencode` options, e.g.:

    curl -G 'http://localhost:48075/reset/b2' --data-urlencode "config=Master 128 (MOS 3.20)"

### `paste/WIN` ###

Paste text in as if by `Paste OSRDCH` from the `Edit` menu.

The text to paste is taken from the request body, which must be
`text/plain`, with `Content-Encoding` of `ISO-8859-1` (assumed if not
specified) or `utf-8`.

### `peek/WIN/BEGIN-ADDR/END-ADDR?s=SUFFIX&mos=MOS`; `peek/WIN/BEGIN-ADDR/+SIZE?s=SUFFIX&mos=MOS` ###

Retrieve memory from `BEGIN-ADDR` (16-bit hex, inclusive) to
`END-ADDR` (32-bit hex, exclusive) or `BEGIN-ADDR+SIZE` (exclusive -
`SIZE` is a C-style 32-bit literal). Respond with
`application/octet-stream`, a dump of the requested range.

You can't peek past 0xffff.

The `SUFFIX` argument is any debugger address suffixes (default:
none), and the `MOS` argument is the value for the `MOS's view` flag
(default: false). Both are discussed above.

### `poke/WIN/ADDR?s=SUFFIX&mos=MOS` ###

Store the request body into memory at `ADDR` (16-bit hex).

As with `peek`, you can't poke past 0xffff.

For info about `SUFFIX` and `MOS`, see the `peek` endpoint.

### `mount/WIN?drive=D&name=N` ###

Mount a disc image. `D` (default 0) is the drive, and `N` (default "")
is the file name of the disc image. The request body is the disc
image.

If a file name is provided, deduce the format from that, as if it were
loaded from disk via the GUI. Otherwise, deduce the disc image format
from the request content type, as follows:

* `application/x.acorn.disc-image.ssd` - `.ssd` file
* `application/x.acorn.disc-image.dsd` - `.dsd` file
* `application/x.acorn.disc-image.sdd` - `.sdd` file
* `application/x.acorn.disc-image.ddd` - `.ddd` file
* `application/x.acorn.disc-image.adm` - `.adm` file
* `application/x.acorn.disc-image.adl` - `.adl` file

In either case, read the disc image data from the request body.

### `run/WIN?name=N` ###

"Run" a file. The file type is deduced from the file name `N`, if
specified, or the request content type if not.

Currently, onle one file type is supported: a BBC disc image, as per
the list above. The image will be inserted in drive 0, as per `mount`
and the emulator reset.

## Using the HTTP API for developing BBC software

The process involves having the Makefile (or batch
file, if you prefer) invoke [curl](https://curl.se/) to use the
`reset` endpoint to reset the emulator with a specific configuration,
then the `run` endpoint to get it to boot the disk of interest. Two
commands will do:

    curl -G "http://localhost:48075/reset/b2" --data-urlencode "config=Master 128 (MOS 3.20)"
	curl -H "Content-Type:application/binary" --upload-file "my_program.ssd" "http://localhost:48075/run/b2?name=my_program.ssd"
	
Other examples from GitHub:

* https://github.com/kieranhj/scr-beeb/blob/03342c776bf489cbc8ba2c22e26baa5e3f8c3b1d/Makefile#L213
* https://github.com/kieranhj/stnicc-beeb/blob/a5df6b838a8183fe141d7a12cb5a7d167a6d912a/Makefile#L84
* https://github.com/tom-seddon/256_bytes/blob/b30ca923d1613642e89484faef9518d1b7c78c60/Makefile#L125

If you're working on a sideways ROM, you can make a hardware config
that refers to the ROM you're building, then use `reset` to reboot the
emulated BBC. `reset` will reload the paged ROMs from disk, so it'll
be running with the updated code.
