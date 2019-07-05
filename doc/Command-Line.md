# Command line

You can use various command line options to control how the emulator
starts. Run b2 with `--help` to get a full list; the (probably) most
useful ones are described here.

## `-0 FILE`, `-1 FILE`

Load disc image `FILE` into drive 0 or drive 1, as if using `File` >
`Drive X` > `Disc image...`.

When using `-0` or `-1`, you can also supply `--0-direct` or
`--1-direct` respectively, indicating that direct disc image access
should be used, as if using `File` > `Drive X` > `Direct disc
image...`.

## `-b`

Tries to autoboot the disc in drive 0. The emulator just pretends
SHIFT is being held while starting up.

## `-c CONFIG`

Start up with config `CONFIG`. The name is exactly as seen in the list
of configs shown in `File` > `Change config` or `Tools` >
`Configurations`.

## `--reset-windows`

Reset window positions. The docking/tabbing system can occasionally
get itself into a mess, and this is a workaround for that...

## `--vsync`, `--timer`

Select screen update timing method. The default, `--vsync`, should
work fine, but try `--timer` if it feels like the display update rate
is poor even though the emulator claims to be running at ~1x real
speed.

Whichever you choose, the option is sticky, and will be used for
subsequent runs even when neither option is supplied.
