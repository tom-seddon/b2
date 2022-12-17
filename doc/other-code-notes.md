# Submodule remotes

GitHub mostly tracks the right forks. Find the `tom-seddon` copy of
the submodule and use the URL of the forked repo as the `upstream`
remote.

## `imgui`

This was originally a fork of the upstream repo, but it now tracks
somebody else's fork, which is where the docking UI stuff comes from:

    git remote add Flix01 ssh://git@github.com/Flix01/imgui

# Turbo disc (RIP)

After 1770 sets DRQ, wait for CPU to read/write data register. Once
read/written, wait for CPU to execute next RTI. Then go to the next
byte straight away. Also: reduce settle and seek times. (Don't omit
the inter-sector delay, as Watford DDFS needs it.)

This involved a bunch of code in a bunch of places, and ended up a
special case, being a runtime toggle (that affected reproducibility)
rather than part of the BeebConfig. And I never tested it enough to
promote it.

It could return, but probably won't... there are better ways of
providing fast file access, that b2 should probably support instead -
BeebLink (etc.), B-em's VDFS (etc.), (if you want ADFS) some kind of
hard disc emulation, and so on.

# Debugger paging override syntax

When providing an address, by default the debugger accesses whichever
byte of memory is paged in at the time. If you access address 8003,
for example, you could get any one of 16 paged ROMs, or (on B+/Master)
some of the extra RAM, depending on the value of ROMSEL.

If you want to refer to a particular byte, unambiguously, you can use
the paging override syntax. This consists of a prefix, specifying
which bits of paged memory are paged in, then a `.`, then the address.
For example, `5.8003`, to refer to address 8003 in ROM bank 5.

The available prefixes are as follows:

* `0`, `1`, `2`, `3`, `4`, `5`, `6`, `7`, `8`, `9`, `a`, `b`, `c`, `d`, `e`, `f`,  - the given ROM bank
* `n` - ANDY (Master), extra 12K RAM (B+)
* `m`, `s` - main RAM or shadow RAM
* `h` - HAZEL (Master)
* `i` - I/O area (only useful on Master)
* `o` - OS

prefix precedence? overlapping areas??


## B

| Address | What |
| --- | --- |
| 0000-7fff | always main RAM |
| 8000-bfff | current paged ROM as per ROMSEL, or `0123456789abcdef` for specific ROM bank |
| c000-fbff | always OS |
| fc00-feff | I/O area, or `o` for OS (not terribly useful) |
| ff00-ffff | always OS |

## B+

| Address | What |
| --- | --- |
| 0000-2fff | always main RAM |
| 3000-7fff | shadow/main RAM as per bit 7 of ACCON, or `m` for main RAM, or `s` for shadow RAM |
| 8000-afff | current paged ROM/extra RAM as per ROMSEL, or `0123456789abcdef` for specific ROM bank, or `n` for 12K extra RAM |
| b000-bfff | current paged ROM as per ROMSEL, or `0123456789abcdef` for specific ROM bank |
| c000-fbff | always OS |
| fc00-feff | I/O area, or `o` for OS (not terribly useful) |
| ff00-ffff | always OS |

Notes:

* `n` is for aNdy, which is what the RAM at 8000 is called on the Master

## Master

| Address | What |
| --- | --- |
| 0000-2fff | always main RAM |
| 3000-7fff | shadow/main RAM as per bit 2 of ACCCON, or `m` for main RAM, or `s` for shadow RAM |
| 8000-8fff | current paged ROM/ANDY as per ROMSEL, or `0123456789abcdef` for specific ROM bank, or `n` for ANDY |
| 9000-bfff | current paged ROM as per ROMSEL, or `0123456789abcdef` for specific ROM bank |
| c000-dfff | OS or HAZEL as per bit 3 of ACCCON, or `o` for OS, or `h` for HAZEL |
| e000-fbff | always OS |
| fc00-feff | OS or I/O area as per bit 6 of ACCCON, or `i` for I/O, or `o` for OS |
| ff00-ffff | always OS |

# agh

```
1948227  m`$4438: dec $fe65 [i`$fe65]      A=01 X=04 Y=00 S=fd P=Nvdizc (D=fe)
1948227  read $ce from $4438
1948228  CPU state before: tfn=T0_All t0fn=T0_RMW_ABS
1948228  CPU state after: tfn=T1_RMW_ABS ifn=DEC, abus=$4439, dbus=$ce
1948228  read $65 from $4439
1948229  CPU state before: tfn=T1_RMW_ABS ifn=DEC
1948229  CPU state after: tfn=T2_RMW_ABS ifn=DEC, abus=$443a, dbus=$65
1948229  read $fe from $443a
1948229  UserVIA - T1 timeout
1948230  CPU state before: tfn=T2_RMW_ABS ifn=DEC
1948230  CPU state after: tfn=T3_RMW_ABS ifn=DEC, abus=$fe65, dbus=$fe
1948230  Stretch #1
1948230  UserVIA - T1 IRQ. Continuous=false PB7=false
1948231  read $ff from $fe65
1948231  UserVIA - T1 reload: T1=$0004 (4)
1948232  CPU state before: tfn=T3_RMW_ABS ifn=DEC
1948232  CPU state after: tfn=T4_RMW_ABS ifn=DEC, abus=$fe65, dbus=$ff
1948232  Stretch #1
1948233  write $ff to $fe65
1948233  UserVIA - T1C-H write acknowledges T1 (IFR=$00)
1948233  UserVIA - T1 reload: T1=$ff04 (65284)
1948234  CPU state before: tfn=T4_RMW_ABS ifn=DEC
1948234  CPU state after: tfn=T5_RMW_ABS ifn=DEC, abus=$fe65, dbus=$fe

1948234  Checked for interrupts <---- this should happen just before the previous write!

1948234  Stretch #1
1948235  write $fe to $fe65
1948235  UserVIA - T1C-H write acknowledges T1 (IFR=$00)
1948235  UserVIA - T1 reload: T1=$fe04 (65028)
1948236  CPU state before: tfn=T5_RMW_ABS ifn=DEC
1948236  CPU state after: tfn=T0_All t0fn=?, abus=$443b, dbus=$fe
1948236  m`$443b: lda $1001 [m`$1001]      A=00 X=04 Y=00 S=fd P=nvdiZc (D=00)
1948236  read $ad from $443b
```
* Emacs setup

`.dir-locals.el`:

    ((nil . ((compile-command . "cd ~/b2/ && make tom_emacs"))))
	

# config versioning notes

Tag each default config with a feature name, a string

When saving: save off features seen in all configs

When loading: any default configs with an unseen feature are added to
the list. And the cycle repeats.

Configs predating the feature mechanism - i.e., the stock B/B+/M128
set - are marked by having an empty feature name., They never get
re-added.

