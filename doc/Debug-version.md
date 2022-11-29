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

## **Second processor support is WIP**

- Examining the parasite state should work OK, but you may find that
  the host CPU state is affected if you try to change anything

- The paging prefix indicators in the parasite trace output are
  currently nonsense. The parasite paging is simple enough that things
  should still be comprehensible

This will improve!

## General debugging ##

Use `Stop` to stop the emulated BBC in its tracks.

`Run` will set it going again.

`Step In` will run one host instruction and then stop.

`Step Over` will run until the next host instruction visible.

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

## Address prefixes

Addresses are annotated with a prefix, indicating which paging
settings are in force when deciding what's visible at that address.The
possible prefixes are as follows.

Host memory:

- `m` - main RAM ($0000...$7fff)
- `0` - `f` - paged ROM ($8000...$bfff)
- `s` - shadow RAM (B+/Master only) ($3000...$7fff)
- `n` - ANDY (B+/Master only) (B+: $8000...$afff; Master: $8000...$8fff)
- `h` - HAZEL (Master only) ($c000...$dfff)
- `o` - OS ROM ($c000...$ffff)
- `i` - I/O area ($fc00...$feff)

Parasite memory:

- `p` - parasite RAM ($0000...$ffff)
- `r` - parasite ROM ($f000...$ffff)

The prefix is sometimes redundant. (For example, host address $0000 is
always shown as ``m`$0000``, even though there's no other prefix it
could have.)

When entering an address, you can usually supply an address prefix.
(For example, to view $8000 in ROM 4, you might enter ``4`$8000``.)
Appropriate paging overrides will be selected to ensure the requested
byte is visible.

You can supply multiple address prefixes, and they stack up in the
order given, though this isn't always especially useful.

Note that on B+ and Master, a ROM bank prefix (`0` - `f`) implies ANDY
is disabled, and on Master the OS ROM (`o`) implies HAZEL is disabled.
Supply further prefixes to change this if desired.

Inappropriate prefixes are ignored. 

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
  address prefixes aren't supported
* `Write $xxxx` - recording will start when the given address is
  written to. Writes to any address can be trapped, even if that write
  has no effect, e.g., because the area is ROM. Note that this
  currently goes only by address - address prefixes aren't supported

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

The trace output is along these lines:

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

Addresses in the trace are annotated with an address prefix hint. Note
this is a hint, and while it's usually correct it may not actually
reflect the affected byte when the prefix is `r` (read parasite ROM,
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

## `6502 Debug` ##

Show a 6502 debug window. Displays typical register info in the top
half, and internal stuff (cycle count, internal state, data bus
address, etc.) in the bottom half.

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

Memory is read with the paging overrides that are currently in effect.
As per the memory view window contents, any memory-mapped I/O devices
are bypassed and you see what's in the ROM or RAM behind them.

## `Host Disassembly Debug`, `Parasite Disassembly Debug` ##

Show running disassembly.

With `Track PC` ticked - the default for disassembly window 1 - the
disassembly will track the program counter. Otherwise, enter an
address in the `Address` field to visit that address.

The disassembly window will make a stab at guessing effective
addresses, when not statically obvious. Its guesses are based on the
current state of the system, trying to take paging overrides into
account, and are for advisory purposes only.

## `CRTC Debug`, `Video ULA Debug`, `System VIA Debug`, `User VIA Debug`, `NVRAM Debug` ##

Activate a debug window for the corresponding piece of BBC hardware,
showing current register values and additional useful info.

## `Paging debug`

Shows current paging settings for the host system.

## `Breakpoints`

Shows a list of all breakpoints. When you use the byte popup to set a
breakpoint on an address or byte, it will appear here.

If you ever use this window to alter the breakpoint flags for a
particular address or byte, that byte or address will continue to be
shown in the list even if all its breakpoint flags are cleared. When
this happens, click the `x` button to get rid of it.

## `Host Stack`, `Parasite Stack`

Shows a dump of the stack contents. Values below the bottom of the
stack are shown in a darker colour.

The `Addr` column is right-clickable. The address shown is exactly
what was pushed, but the right click popup includes the address after
it, for dealing with addresses pushed by `jsr`.

## `Tube`

Shows current Tube status: IRQ state, control register values,
contents and status for each FIFO.

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
localhost. **At your own risk**, you can have it accept connections
from anywhere - consult the `--help` output.

**The HTTP API is a work in progress, and may change.**

## HTTP Methods

Values in capitals indicate parameters. Parameters listed as part of
the path are found by position, and are mandatory; those listed as
part of the query string are found by name, and are optional.

Every method takes a window name, `WIN` - as seen in the title bar -
as a parameter, indicating which window to send the request to. In
most cases, this will probably be `b2`, the name of the initial window
the emulator creates on startup.

Parameters expecting numbers are typically hex values, e.g., `ffff`
(65535), or C-style literals, e.g., `65535` (65535), `0xffff` (65535),
`0177777` (65535).

Unless otherwise specified, the HTTP server assumes the request body's
`Content-Type` is `application/octet-stream`. And unless otherwise
specified, the response is an HTTP status code with no content.
Generally this will be one of `200 OK` (success), `400 Bad Request` or
`404 Not Found` (invalid request), or `503 Service Unavailable` (request
was valid but couldn't be fulfilled).

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

### `poke/WIN/ADDR` ###

Store the request body into memory at `ADDR` (a 32-bit hex value), the
address as per
[JGH's addressing scheme](http://mdfs.net/Docs/Comp/BBC/MemAddrs).

### `peek/WIN/BEGIN-ADDR/END-ADDR`; `peek/WIN/BEGIN-ADDR/+SIZE` ###

Retrieve memory from `BEGIN-ADDR` (32-bit hex, inclusive) to
`END-ADDR` (32-bit hex, exclusive) or `BEGIN-ADDR+SIZE` (exclusive -
`SIZE` is a C-style 32-bit literal). Respond with
`application/octet-stream`, a dump of the requested range.

There is a limit on the amount of data that can be peeked. Currently
this is 4MBytes.

Addresses are as per
[JGH's addressing scheme](http://mdfs.net/Docs/Comp/BBC/MemAddrs).

### `mount/WIN?drive=D&name=N` ###

Mount a disc image. `D` (default 0) is the drive, and `N` (default "")
is the file name of the disc image. The request body is the disc
image.

If a file name is provided, deduce the format from that, as if it were
loaded from disk via the GUI. Otherwise, deduce the disc image format
from the request content type, as follows:

* `application/vnd.acorn.disc-image.ssd` - `.ssd` file
* `application/vnd.acorn.disc-image.dsd` - `.dsd` file
* `application/vnd.acorn.disc-image.sdd` - `.sdd` file
* `application/vnd.acorn.disc-image.ddd` - `.ddd` file
* `application/vnd.acorn.disc-image.adm` - `.adm` file
* `application/vnd.acorn.disc-image.adl` - `.adl` file

In either case, read the disc image data from the request body.

### `run/WIN?name=N` ###

"Run" a file. The file type is deduced from the file name `N`, if
specified, or the request content type if not.

#### `application/x-c64-program`, `*.prg` - C64-style PRG ####

Load file at load address (taken from first 2 bytes of file) as per
`poke`, and start execution at that address as per `call`.

#### (content type or file extension as above) - disc image ####

Mount disc image in drive 0, as per `mount`, and reset emulator with
autoboot.

## HTTP Example

See `etc/http_api_example` in the repo. Run the makefile in Windows,
with `curl` on the path, using `bin\snmake.exe` from the repo.

(It ought to work on macOS and Linux without too much effort, but
that's never been tested.)

The makefile assembles the example code using 64tass (supplied),
producing a C64-style .PRG file. It then resets the emulator with a
`reset` request, and runs the assembled code using the `run` request.
(All these requests are sent to the `b2` window, which will typically
exist as it's the window created when the emulator starts.)

When using Emacs, M-x compile will assemble the code and set it
running in the emulator straight away. Instant turnaround! Other
editors can be configured similarly.

This demo is not amazing or anything, but it might at least
demonstrate the process...
