The default configs are set up as follows.

# BBC B

All configurations have OS 1.20, BASIC II, 16 KB sideways RAM, and a
ROM board (so you can use all 16 ROM banks).

There are six BBC B configs, corresponding to different disk interface
boards. Each one comes with the corresponding filing system ROM.

- `B/Acorn 1770` - standard Acorn 1770 board
- `B/Watford 1770 (DDB2)` - earlier Watford 1770 board
- `B/Watford 1770 (DDB3)` - later Watford 1770 board, hardware compatible
  with Acorn 1770
- `Opus 1770` - Opus 1770, as used by Opus DDOS
- `Opus CHALLENGER 256K` - Opus Challenger all-in-one disk drive/disk
  interface/256K RAM disk
- `Opus CHALLENGER 512K` - same again, but with a 512K RAM disk

An additional configuration adds a 6502 second processor:

- `B/Acorn 1770 + 6502 second processor` - as `B/Acorn 1770`, but with
  a 3 MHz external 6502 second processor

(In the interests of not having a huge long list, only one 6502 second
processor option is provided by default. You can configure additional
ones if necessary.)

# BBC B+

All configurations have OS 2.00, BASIC II, and the 1770 fitted, with
Acorn DFS.

- `B+` - standard 64 KB BBC B+
- `B+128` - B+ with the 64 KB sideways RAM addon

# BBC Master 128

b2 emulates two types of Master:

- Master 128
- Master Turbo (Master 128 with internal 4 MHz 6502 second processor)

There are two versions of the Master 128 MOS:

- MOS 3.20 (default one supplied with the system)
- MOS 3.50 (optional upgrade)

So there are 4 Master 128 configs, covering all 4 combinations of the
above:

- `Master 128 (MOS 3.20)`
- `Master 128 (MOS 3.50)`
- `Master Turbo (MOS 3.20)`
- `Master Turbo (MOS 3.50)`

(The Master Turbo variants start up with the second processor active,
but you can do a `*CONFIGURE NOTUBE` to change this.)

# BBC Master Compact

There are 3 Master Compact configs, one for each version of the Master
Compact OS:

- `Master Compact (MOS 5.00)`
- `Master Compact (MOS 5.10)`
- `Master Compact (MOS 5.11i+Arabic)` - also includes the Arabic
  language support ROMs

# Olivetti PC 128 S

There is one Olivetti PC 128 S config. There was only one version of
the hardware and MOS released.

- `Olivetti PC 128 S`

# Acorn Electron

The Electron configs cover some useful combinations:

- `Electron/Plus 1` - Electron with Plus 1
- `Electron/Plus 1/Plus 3` - Electron with Plus 1 and Plus 3
