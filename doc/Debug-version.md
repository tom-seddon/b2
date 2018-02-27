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

Use the 6502 debugger to debug programs running on the emulated BBC.
The following `Debug` entries are relevant.

## Debugger windows

### `Tracing`

The trace functionality records all CPU activity, and selected other
events of interest. The recorded data can be saved to a text file for
later perusal.

There are two options for starting the recording.

* `Immediate` - when `Start` is clicked, recording will start straight away
* `Return` - when `Start` is clicked, recording will start once the
  Return key is pressed

There are two options for stopping the recording.

* `By request` - recording will stop when `Stop` is clicked
* `OSWORD 0` - recording will stop automatically when the BBC executes
  an OSWORD with A=0 (read input line)

(`Return` and `OSWORD 0` often go together, because this works well
for tracing code CALLed from the BASIC prompt.)

Once a trace has been started and stopped, it can be saved to a file
using `Save` or `Save (no cycles)`. The output is along these lines:

```
2862490128  1591: ldy #$28                 A=c0 X=0a Y=28 S=f2 P=nVdizc (D=28)
2862490130  1593: lda ($92),Y [$6719]      A=ca X=0a Y=28 S=f2 P=NVdizc (D=ca)
2862490136  1595: and #$f0                 A=c0 X=0a Y=28 S=f2 P=NVdizc (D=f0)
2862490138  1597: sta ($92),Y [$6719]      A=c0 X=0a Y=28 S=f2 P=NVdizc (D=c0)
2862490144  1599: inc $92                  A=c0 X=0a Y=28 S=f2 P=NVdizc (D=f2)
2862490149  159b: lda #$07                 A=07 X=0a Y=28 S=f2 P=nVdizc (D=07)
2862490151  159d: bit $92                  A=07 X=0a Y=28 S=f2 P=NVdizc (D=f2)
2862490154  159f: bne $15b3                A=07 X=0a Y=28 S=f2 P=NVdizc (D=01)
2862490157  15b3: dex                      A=07 X=09 Y=28 S=f2 P=nVdizc (D=f0)
2862490159  15b4: beq $15b9                A=07 X=09 Y=28 S=f2 P=nVdizc (D=00)
2862490161  15b6: jmp $1569                A=07 X=09 Y=28 S=f2 P=nVdizc (D=00)
```

The first column is the cycle count - then address, instruction,
effective address (when not staticaly obvious), and register values
after the instruction is complete.

The bracketed `D` value is the value of the emulated CPU's internal
data register. For read or read-modify-write instructions, this is the
value read; for branch instructions, this is the result of the test
(1=taken, 0=not taken).

Tick the `Flags` checkboxes to get additional hardware state output in
the file:

```
2862491406  6845 - hblank begin.
2862491407  1689: bne $2604                A=00 X=0f Y=00 S=f1 P=NVdIZc (D=09)
2862491408  UserVIA - T1 timed out (continuous=1). T1 new value: 15070 ($3ADE)
2862491408  UserVIA - IRQ. IFR: t1; IER: t1 t2
```

`Save (no cycles)` produces output with no cycle count column. This
can be more useful for diffs.

### `Pixel metadata`

Show a window that displays the RAM address of the pixel the mouse
cursor is over.

### `6502 Debug` ###

Show a 6502 debug window. Displays typical register info in the top
half, and internal stuff (cycle count, internal state, data bus
address, etc.) in the bottom half.

### `Memory Debug` ###

Show a memory debug window. Click to edit memory.

### `Disassembly Debug` ###

Show running disassembly.

With `Track PC` ticked, the disassembly will track the program
counter. Otherwise, enter an address in the `Address` field to visit
that address.

Click the box next to each instruction to set or unset a breakpoint at
that specific address.

#### Address annotations ####

Addresses are annotated to indicate which memory bank they come from:

* `r` - normal RAM
* `x` - extra B+/M128 12K RAM (unlikely)
* `s` - shadow RAM (unlikely)
* `0`-`9`, `a`-`f` - sideways ROM/RAM
* `i` - I/O area (unlikely)
* `o` - OS

(These annotations are only used by the disassembly window, and will
probably be replaced in the long run by pervasive use of
[JGH's addressing scheme](http://mdfs.net/Docs/Comp/BBC/MemAddrs).)

### `CRTC Debug`, `Video ULA Debug`, `System VIA Debug`, `User VIA Debug`, `NVRAM Debug` ###

Activate a debug window for the corresponding piece of BBC hardware,
showing current register values and additional useful info.

## Debugger step

Use `Stop` to stop the emulated BBC in its tracks.

`Run` will set it going again.

`Step In` will run one instruction and then stop.

`Step Over` will run until the next instruction visible.

# Other debug-related options #

Additional debug options can be found in `Tools` > `Options` in the
`Display Debug Flags` section.

`Teletext debug` will overlay the teletext display with the value of
the character in each cell.

`Show TV beam position` will show (with a little white dot) where the
TV beam is when the emulator is stopped in the debugger.

# Other debugger stuff

There is other debug stuff, but it's all undocumented...

# HTTP API

Use the HTTP API to control the emulator remotely. Use this from a
shell script or batch file, say, using `curl`, or from a program,
using an HTTP client library.

The emulator listens on localhost on port 48075 (0xbbcb). Request
paths are of the form `/w/WIN/TYPE...`, where `WIN` is the window name
(as per the title bar) and `TYPE` the request type. Some requests take
additional mandatory parameters, which are part of the URL path, and
some requests take additional optional parameters, which are part of
the URL query parameters. See each individual request type for the
details.

Parameters expecting numbers are typically hex values, e.g., `ffff`
(65535), or C-style literals, e.g., `65535` (65535), `0xffff` (65535),
`0177777` (65535).

Unless otherwise specified, the HTTP server assumes the request body's
`Content-Type` is `application/octet-stream`. And unless otherwise
specified, the response is an HTTP status code with no content.
Generally this will be one of `200 OK` (success), `400 Bad Request` or
`404 Not Found` (invalid request), or `503 Service Unavailable` (request
was valid but couldn't be fulfilled).

## HTTP Methods

### `reset` ###

Reset the BBC. This is equivalent to a power-on reset. Memory is wiped
but mounted discs are retained.

### `paste` ###

Paste text in as if by `Paste OSRDCH` from the `Edit` menu.

The request body must be `text/plain`, with `Content-Encoding` of
`ISO-8859-1` (assumed if not specified) or `utf-8`.

### `poke/ADDR` ###

Store the request body into memory at `ADDR` (a 32-bit hex value), the
address as per
[JGH's addressing scheme](http://mdfs.net/Docs/Comp/BBC/MemAddrs).

### `peek/BEGIN-ADDR/END-ADDR`; `peek/BEGIN-ADDR/+SIZE` ###

Retrieve memory from `BEGIN-ADDR` (32-bit hex, inclusive) to
`END-ADDR` (32-bit hex, exclusive) or `BEGIN-ADDR+SIZE` (exclusive -
`SIZE` is a C-style 32-bit literal). Respond with
`application/octet-stream`, a dump of the requested range.

There is a limit on the amount of data that can be peeked. Currently
this is 4MBytes.

Addresses are as per
[JGH's addressing scheme](http://mdfs.net/Docs/Comp/BBC/MemAddrs).

### `call/ADDR?a=A&x=X&y=Y&c=C` ###

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

### `mount?drive=D&name=N` ###

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

### `run?name=N` ###

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
