# Debug version

Run the debug version on Windows by running `b2_Debug.exe`, as
extracted from the distribution zip file.

Run the debug version on OS X by copying `b2 Debug` from the dmg file
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

Use `Stop` to stop the emulated BBC in its tracks.

`Run` will set it going again.

`Step In` will run one instruction and then stop.

`Step Over` will run until the next instruction visible.

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
when an address, you can right click the address or value to get a
popup UI relating to that byte.

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

## Address prefixes

Addresses are annotated with a prefix, indicating which bank they come
from. The possible prefixes are as follows:

- `m` - main RAM
- `0` - `f` - paged ROM
- `s` - shadow RAM (B+/Master only)
- `n` - ANDY (B+/Master only)
- `h` - HAZEL (Master only)
- `o` - OS ROM
- `i` - I/O area

The prefix is sometimes redundant.

Prefixes are not currently supported very consistently, but this will
improve.

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
* `Instruction` - recording will start once the PC is equal to the
  given address. Note that this currently goes only by address -
  address prefixes aren't supported

Once the trace starts, you can always click `Stop` to end it, but
there are additional options for the trace end condition:

* `By request` - stop only when `Stop` is clicked
* `OSWORD 0` - stop when the BBC executes an OSWORD with A=0 (read
  input line)
* `Cycle count` - stop when the trace has been going for a particular
  number of cycles

(`Return` and `OSWORD 0` often go together, because this works well
for tracing code CALLed from the BASIC prompt.)

By default, only the last 25-30 seconds of activity will be kept
(usually corresponding to roughly a 1GByte output file). Tracing can
be left running indefinitely and the emulator will use a fixed amount
of memory.

Tick `Unlimited recording` to have the emulator store all events, so
longer periods can be traced. Each emulated second takes up ~10MBytes
of RAM and adds ~35MBytes to the output file.

Once a trace has been started and stopped, it can be saved to a file
using `Save` or `Save (no cycles)`. The output is along these lines:

```
      63  m.17cc: lda #$81                 A=81 X=65 Y=00 S=f0 P=Nvdizc (D=81)
      65  m.17ce: ldy #$ff                 A=81 X=65 Y=ff S=f0 P=Nvdizc (D=ff)
      67  m.17d0: jsr $1bba                A=81 X=65 Y=ff S=ee P=Nvdizc (D=17)
      73  m.1bba: stx $1b8f [m.1b8f]       A=81 X=65 Y=ff S=ee P=Nvdizc (D=65)
      77  m.1bbd: ldx #$04                 A=81 X=04 Y=ff S=ee P=nvdizc (D=04)
      79  m.1bbf: jmp $1b75                A=81 X=04 Y=ff S=ee P=nvdizc (D=04)
      82  m.1b75: sta $1b91 [m.1b91]       A=81 X=04 Y=ff S=ee P=nvdizc (D=81)
      86  m.1b78: lda $f4 [m.00f4]         A=04 X=04 Y=ff S=ee P=nvdizc (D=04)
      89  m.1b7a: pha                      A=04 X=04 Y=ff S=ed P=nvdizc (D=04)
      92  m.1b7b: lda #$05                 A=05 X=04 Y=ff S=ed P=nvdizc (D=05)
      94  m.1b7d: sta $f4 [m.00f4]         A=05 X=04 Y=ff S=ed P=nvdizc (D=05)
      97  m.1b7f: sta $fe30 [i.fe30]       A=05 X=04 Y=ff S=ed P=nvdizc (D=05)
```

The first column is the cycle count - then instruction address,
instruction, effective address, and register values after the
instruction is complete.

The bracketed `D` value is the value of the emulated CPU's internal
data register. For read or read-modify-write instructions, this is the
value read; for branch instructions, this is the result of the test
(1=taken, 0=not taken).

Tick the `Flags` checkboxes to get additional hardware state output in
the file:

```
   24939  m.21f5: stx $fe40 [i.fe40]       A=e3 X=00 Y=03 S=da P=nvdIZc (D=00)
   24944  Port value now: 048 ($30) (%00110000) ('0')
   24944  SystemVIA - Write ORB. Reset IFR CB2.
   24945  m.21f8: lda $fe40 [i.fe40]       A=30 X=00 Y=03 S=da P=nvdIzc (D=30)
   24946  PORTB - PB = $30 (%00110000): RTC AS=0; RTC CS=0; Sound Write=true
   24950  SN76489 - write noise mode: periodic noise, 3 (unknown
   24951  m.21fb: ora #$08                 A=38 X=00 Y=03 S=da P=nvdIzc (D=08)
   24953  m.21fd: sta $fe40 [i.fe40]       A=38 X=00 Y=03 S=da P=nvdIzc (D=38)
```

`Save (no cycles)` produces output with no cycle count column. This
can be more useful for diffs.

## `Pixel metadata` ##

Show a window that displays the RAM address of the pixel the mouse
cursor is over.

## `6502 Debug` ##

Show a 6502 debug window. Displays typical register info in the top
half, and internal stuff (cycle count, internal state, data bus
address, etc.) in the bottom half.

## `Memory Debug` ##

Show a memory debug window. Click to edit memory.

## `Disassembly Debug` ##

Show running disassembly.

With `Track PC` ticked - the default for disassembly window 1 - the
disassembly will track the program counter. Otherwise, enter an
address in the `Address` field to visit that address.

The disassembly window will make a stab at guessing effective
addresses, when not statically obvious. It guesses based on the
current state of the system, trying to take paging overrides into
account, and are for advisory purposes only.

## `CRTC Debug`, `Video ULA Debug`, `System VIA Debug`, `User VIA Debug`, `NVRAM Debug` ##

Activate a debug window for the corresponding piece of BBC hardware,
showing current register values and additional useful info.

## `Paging debug`

Shows current paging settings.

## `Breakpoints`

Shows a list of all breakpoints. When you use the byte popup to set a
breakpoint on an address or byte, it will appear here.

If you use this window to alter the breakpoint flags for a particular
address or byte, that byte or address will continue to be shown in the
list even if all its breakpoint flags are cleared. When this happens,
click the `x` button to get rid of it.

# Other debug-related options #

Additional debug options can be found in `Tools` > `Options` in the
`Display Debug Flags` section.

`Teletext debug` will overlay the teletext display with the value of
the character in each cell.

`Show TV beam position` will indicate where the TV beam, with a line
starting just after its current position.

# Other debugger stuff

There may be other debug stuff that I haven't got round to documenting
yet.

# HTTP API

Use the HTTP API to control the emulator remotely. Use this from a
shell script or batch file, say, using `curl`, or from a program,
using an HTTP client library.

The emulator listens on localhost on port 48075 (0xbbcb).

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

### `call/WIN/ADDR?a=A&x=X&y=Y&c=C` ###

Call a routine at `ADDR` (16-bit hex). Load registers from `A`, `X`
and `Y` (8-bit C-style literals, default 0 if not specified) first,
and load the carry flag from `C` (`0` or `1`, again assumed to be 0 if
not provided).

The routine is called on exit from the next invocation of the IRQ
handler, so interrupts must be enabled if it is to be actually called.

There's no control over the paging. The address just has to be valid
when the time comes.

Respond with `200 OK` if the call was initiated within half a second
or so, or `503 Service Unavailable` if not, e.g., because interrupts
were disabled for the whole period.

(There's no information available about when the routine returns. It
might even not return at all.)

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
with `curl` on the path, using `snmake.exe` from the root of the repo.

(It ought to work on OS X and Linux without too much effort, but
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
