# BeebEm UEF save state format

How BeebEm (Windows) writes and reads its UEF save state files, `.uefstate` (or `.uef`). Everything here comes from the BeebEm source code.

* **Source analysed:** <https://codeberg.org/chrisn/beebem-windows>, `master` at commit
  `763539a120d1dc90c49be0d0af55193a5e109ce9` (11 Oct 2026). `UEFSTATE_VERSION = 17`.
* All source links point to that commit on Codeberg, with line anchors.



---

## 1. Conventions

| Notation | Meaning |
|---|---|
| `u8` / `u16` / `u32` / `u64` | Unsigned little-endian integer. Written by `UEFWrite8/16/32/64` ([UefState.cpp:56-121](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L56)) |
| `s32` | Same bytes as `u32`, but BeebEm holds the value in an `int`, so it can be negative |
| `bool` | 1 byte. BeebEm writes `0` or `1` and reads any non-zero value as true ([UefState.cpp:125](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L125), [:240](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L240)) |
| `str` | **v14+:** `u32` length followed by that many bytes, with **no NUL terminator** ([UefState.cpp:137](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L137)). The reader truncates to a 256-byte buffer (`UEFSTATE_MAX_PATH`) and discards any excess ([UefState.cpp:247](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L247)). **v≤13:** a fixed 256-byte NUL-padded buffer (but see §6, item 6). |
| `raw[n]` | `n` bytes copied straight from memory. Where the source is an array of multi-byte ints, the bytes come out in x86 order, which is little-endian. |
| "cycles" | 2 MHz 6502 cycles |
| `TMAX` | `0x7FFFFFFF` (`CycleCountTMax = INT_MAX`, [Port.h:38](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Port.h#L38)). Means "no event scheduled". |

**"Version"** in this document is the save-state version number BeebEm stores in the UEF header (see §2). This one number is the only versioning. No chunk carries its own version byte. Each load function receives the header's version and changes its parsing to match.

### Version history

| Version | Written by | Layout changes |
|---|---|---|
| ≤8 | very old builds | In 0x046A, machine type `1` meant Master 128 |
| 10 | (by 3.7, 2007) | 1770 chunk gained a separate `DiskDensity[1]` (v≤9 reused drive 0's) |
| 11 | | 0x046A gained the keyboard-mapping fields |
| 12 | BeebEm 4.12 – 4.15 | |
| 13 | BeebEm 4.16 – 4.19 | 0x0468 video gained the full raster state. 0x0477 Music 5000 gained the JIM page register, replacing the separate chunk 0x0476. |
| 14 | development builds only (Dec 2023 – Aug 2024) | Strings became length-prefixed. 8271 gained FSD and head-timing fields. Serial/tape gained carrier, play state and CSW fields. Co-processor chunks 0x0478 – 0x047B were added. PAL ROM chunk 0x047C was added in Mar 2024 with no version bump. |
| 15 | BeebEm 4.20 | 0x0479 ARM gained an `armModel` byte |
| 16 | BeebEm 4.21 – 4.23 | 0x0467 VIA timer counters became `s32` at 2 MHz, plus `SRTrigger`. Speech chunk 0x047D was added in Apr 2026 with no version bump. |
| 17 | current `master` (unreleased) | **No byte-layout change.** The video byte that held `DoCA1Int` is now always written as 0 and ignored. |

> **Compatibility tip:** BeebEm refuses any file whose version is greater than its own ([UefState.cpp:426](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L426)). Version 16 and version 17 files are byte-identical in layout. Write **16** to stay loadable by released BeebEm 4.21 – 4.23.

---

## 2. File header and chunk framing

Save: `SaveUEFState` [UefState.cpp:291](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L291). Load: `LoadUEFState` [UefState.cpp:398](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L398).

| Offset | Size | Content |
|---|---|---|
| 0 | 10 | `"UEF File!"` followed by `0x00` |
| 10 | 1 | "minor version" = save-state version (currently 17) |
| 11 | 1 | "major version" = 0 |
| 12 | … | Chunks |

The reader reads offsets 10–11 as one `u16` (minor | major << 8) and rejects the file if that value is greater than `UEFSTATE_VERSION`.

Each chunk is `u16 id`, `u32 length`, then `length` bytes. The writer uses a generic helper that writes the header with a placeholder length and back-patches it afterwards ([UefState.cpp:274](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L274)). The exception is the memory chunks, which `SaveMemUEF` writes with hard-coded lengths.

Reader loop ([UefState.cpp:432-574](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L432)):

* Repeat while `ftell < file length`: read the id and length, dispatch to a handler, then `fseek(start_of_body + length)`.
* Unknown chunks are skipped. Handlers that read fewer bytes than the chunk holds are fine.
* **There are no bounds checks.** A handler that reads more than `length` bytes reads into the next chunk's header and body, and the loop then seeks back. If that over-read runs past end-of-file, the whole load fails with `ReadFailed`.
* Loading is not transactional. Chunks are applied in file order as they are read.

### Save-state detection

When the file extension is `.uefstate`, BeebEm always treats the file as a save state. When the extension is `.uef`, `IsUEFSaveState` ([UefState.cpp:590](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L590), called from [BeebWin.cpp:5784](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebWin.cpp#L5784)) decides: the file is a save state only if bytes 12–13 are `6C 04`. In other words, **chunk 0x046C must be the first chunk**. Otherwise BeebEm loads the file as a tape.

### Chunk order written by BeebEm

1. `0x046C` BeebEm ID
2. `0x046A` emulator/machine config
3. `0x0460` 6502
4. Memory chunks, in this order: `0x0461`, `0x0462`, then model-specific chunks from `0x0463`/`0x0464`/`0x0465`/`0x046D`, then 16 bank chunks, one per bank 0..15, each `0x0466`, `0x0475` or `0x047C`
5. `0x0468` video
6. `0x0467` System VIA
7. `0x0467` User VIA
8. `0x046B` sound
9. Disc: `0x046E` (8271) when `MachineType != Master128 && NativeFDC`, otherwise `0x046F` (1770)
10. `0x0470` Tube ULA, when a co-processor is enabled
11. Co-processor: `0x0471` + `0x0472` (65C02), `0x047B` (Master 512), `0x0478` (Acorn or Torch Z80), `0x0479` (ARM Evaluation System), or `0x047A` (Sprow ARM)
12. `0x0473` serial/tape, **only when a tape is loaded**
13. `0x0474` A/D converter (always written)
14. `0x0477` Music 5000, when enabled
15. `0x047D` speech, when speech is enabled

**Order matters on load.** The 0x046A handler calls `ResetBeebSystem()`, which reloads ROMs from BeebEm's own configuration and resets every device. The memory loaders branch on `MachineType`. So 0x046A must come before every other state chunk.

---

## 3. Chunk summary

| ID | Name | Written | Read | Length written |
|---|---|---|---|---|
| 0x046C | BeebEm ID | ✓ | ✗ (detection only) | 16 |
| 0x046A | Emulator/machine config | ✓ | ✓ | 262, or 10 + n (user keymap) |
| 0x0460 | 6502 CPU | ✓ | ✓ | 16 |
| 0x0461 | Memory control (ROMSEL/ACCCON) | ✓ | ✓ | 2 (Integra-B: 3) |
| 0x0462 | Main RAM | ✓ | ✓ | 32768 |
| 0x0463 | Shadow RAM | B+/Integra-B/Master | ✓ | 32768 / 20480 / 32770 |
| 0x0464 | Private RAM | B+/Integra-B/Master | ✓ | 12288 / 12288 / 4096 |
| 0x0465 | Master filing-system RAM | Master | ✓ | 8192 |
| 0x046D | Integra-B hidden RAM | Integra-B | ✓ | 256 |
| 0x0466 | Sideways RAM bank | per RAM bank | ✓ | 16385 |
| 0x0475 | Sideways ROM / empty bank | per ROM or empty bank | ✓ | 16386 or 2 |
| 0x047C | PAL (banked) sideways ROM | per PAL ROM bank | ✓ | 131076 |
| 0x0476 | JIM page register (legacy) | ✗ (written before Nov 2019) | ✓ | 1 |
| 0x0467 | 6522 VIA (Sys or User) | ✓ ×2 | ✓ | Sys 26, User 30 (v<16: 22) |
| 0x0468 | CRTC + Video ULA + raster state | ✓ | ✓ | 112 (v≤12: 43) |
| 0x046B | SN76489 sound | ✓ | ✓ | 20 |
| 0x046E | Intel 8271 FDC | ✓ | ✓ | 126 + n0 + n1 (v≤13: 613) |
| 0x046F | WD1770 FDC | ✓ | ✓ | 101 + n0 + n1 + n_dll |
| 0x0470 | Tube ULA | ✓ | ✓ | 45 |
| 0x0471 | 65C02 co-processor CPU | ✓ | ✓ | 16 |
| 0x0472 | 65C02 co-processor RAM | ✓ | ✓ | 65536 |
| 0x0473 | Serial ULA / ACIA / tape | ✓ | ✓ | 61 + n (+19 for CSW) |
| 0x0474 | uPD7002 A/D converter | ✓ | ✓ | 8 |
| 0x0477 | Music 5000 | ✓ | ✓ | 2125 |
| 0x0478 | Z80 co-processor (Acorn or Torch) | ✓ | ✓ | 65573 |
| 0x0479 | ARM Evaluation System | ✓ | ✓ | 4194578 |
| 0x047A | Sprow ARM7TDMI co-processor | ✓ | ✓ | 67109511 + 8·k |
| 0x047B | Master 512 (80186) | ✓ | ✓ | 1048710 |
| 0x047D | TMS5220 speech + PHROM | ✓ | ✓ | 262348 (or 0, see §6) |

Offsets in the tables below are from the start of the chunk **body**.

---

## 4. Chunk layouts

### 0x046C — BeebEm ID

Save: `BeebWin::SaveBeebEmID` [BeebWinIo.cpp:1454](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebWinIo.cpp#L1454). There is no load function.

| Off | Size | Content |
|---|---|---|
| 0 | 14 | `"BeebEm"`, NUL-padded |
| 14 | u8 | `VERSION_MAJOR` (currently 4) |
| 15 | u8 | `VERSION_MINOR` (currently 23 = 0x17) |

### 0x046A — Emulator / machine config

Save: `BeebWin::SaveEmuUEF` [BeebWinIo.cpp:1466](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebWinIo.cpp#L1466). Load: `BeebWin::LoadEmuUEF` [BeebWinIo.cpp:1490](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebWinIo.cpp#L1490).

| Off | Size | Content |
|---|---|---|
| 0 | u8 | Machine: 0 = Model B, 1 = B + Integra-B, 2 = B+, 3 = Master 128, 4 = Master ET ([Model.h](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Model.h)). If version ≤ 8, the value 1 means Master 128. |
| 1 | bool | 1 = **not** native FDC: Model B / B+ / Integra-B using a 1770 board. 0 = 8271. |
| 2 | u8 | Tube device: 0 none, 1 Acorn 65C02, 2 Master 512, 3 Acorn Z80, 4 Torch Z80, 5 Acorn ARM, 6 Sprow ARM ([Tube.h:29](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Tube.h#L29)). v≤12 builds stored a `TubeEnabled` bool here, and 1 still means 65C02. |
| 3 | u16 | *(v11+)* Keyboard mapping: 0 user, 1 default, 2 logical. The legacy value 40060 (an old menu ID) also means user. |
| 5 | str or raw[256] | *(v11+)* If the mapping is user: the key-map file path (`str` for v14+, 256 bytes for v11–13). Otherwise BeebEm writes **256 zero bytes, not a `str`**. The reader skips them because it only reads this field when the mapping is user. |
| … | u8 | 0 (unused trailer) |

When loading, BeebEm calls `ResetBeebSystem(MachineType)`.

### 0x0460 — 6502 CPU (and 0x0471 — 65C02 co-processor CPU)

Host CPU: Save `Save6502UEF` [6502core.cpp:3376](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/6502core.cpp#L3376), Load `Load6502UEF` [6502core.cpp:3391](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/6502core.cpp#L3391).
Co-processor CPU (identical layout): Save `Save65C02UEF` [Tube.cpp:2699](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Tube.cpp#L2699), Load `Load65C02UEF` [Tube.cpp:2745](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Tube.cpp#L2745).

| Off | Size | Content |
|---|---|---|
| 0 | u16 | PC |
| 2 | u8 | A |
| 3 | u8 | X |
| 4 | u8 | Y |
| 5 | u8 | S |
| 6 | u8 | P. Standard 6502 layout: C=0x01, Z=0x02, I=0x04, D=0x08, B=0x10, V=0x40, N=0x80. |
| 7 | u32 | `TotalCycles` (or `TotalTubeCycles`). Written but **ignored on load**. |
| 11 | u8 | IRQ sources (`IntStatus`): 0x01 Sys VIA, 0x02 User VIA, 0x04 serial, 0x08 Tube, 0x10 teletext, 0x20 hard disc, 0x80 Master ACCCON IRR ([6502core.h:32](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/6502core.h#L32)) |
| 12 | u8 | NMI sources (`NMIStatus`): 0x01 floppy, 0x02 Econet |
| 13 | bool | `NMILock`: an NMI is being serviced, and the lock is released by RTI |
| 14 | u16 | 0 (padding, not read) |

### 0x0461 — Memory control

Save: `SaveMemUEF` [BeebMem.cpp:1434](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1434). Load: `LoadRomRegsUEF` [BeebMem.cpp:1554](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1554).

| Model | Byte 0 | Byte 1 | Byte 2 |
|---|---|---|---|
| B | ROMSEL (bank 0–15) | ACCCON (always 0) | – |
| Master 128 / ET | ROMSEL as written to &FE30. Bit 7 = ANDY (private RAM) select, bits 0–3 = bank. | ACCCON (&FE34) with bit 7 (IRR) masked off. Bit 0 = D, bit 1 = E, bit 2 = X, bit 3 = Y. | – |
| B+ | bits 0–3 bank, bit 7 = `MemSel` (paged RAM) | bit 7 = `Sh_Display` (shadow display) | – |
| Integra-B | bits 0–3 bank, bit 7 `MemSel`, bit 6 `PrvEn` | bit 7 `ShEn`, bit 6 `Prvs8`, bit 5 `Prvs4`, bit 4 `Prvs1` | `HidAdd` |

### 0x0462 – 0x0465, 0x046D — RAM blocks

Save: `SaveMemUEF` [BeebMem.cpp:1434](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1434). Loaders: [`LoadMainMemUEF` :1594](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1594), [`LoadShadMemUEF` :1599](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1599), [`LoadPrivMemUEF` :1625](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1625), [`LoadFileMemUEF` :1648](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1648), [`LoadIntegraBHiddenMemUEF` :1653](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1653).

| ID | Model | Body |
|---|---|---|
| 0x0462 | all | raw[32768]: main RAM &0000–&7FFF |
| 0x0463 | Integra-B | raw[20480]: shadow RAM (&3000–&7FFF) |
| 0x0463 | B+ | raw[32768]: shadow RAM buffer |
| 0x0463 | Master | u16 offset (always 0, read and ignored), then raw[32768]: LYNNE shadow RAM |
| 0x0464 | Integra-B | raw[12288]: private RAM |
| 0x0464 | B+ | raw[12288]: paged RAM at &8000–&AFFF |
| 0x0464 | Master | raw[4096]: ANDY (&8000–&8FFF) |
| 0x0465 | Master | raw[8192]: HAZEL (&C000–&DFFF) |
| 0x046D | Integra-B | raw[256]: hidden RAM |

The loaders interpret 0x0463 and 0x0464 according to the current `MachineType`. A Model B ignores them.

### 0x0466 / 0x0475 / 0x047C — Sideways banks (one chunk per bank, 16 total)

Save: `SaveMemUEF` [BeebMem.cpp:1511](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1511). Loaders: [`LoadSWRamMemUEF` :1658](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1658), [`LoadSWRomMemUEF` :1666](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1666), [`LoadPALRomUEF` :1688](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/BeebMem.cpp#L1688).

**0x0466 — RAM bank:** u8 bank, raw[16384]. The loader marks the bank as writable RAM.

**0x0475 — ROM or empty bank:**

| Off | Size | Content |
|---|---|---|
| 0 | u8 | bank 0–15 |
| 1 | u8 | type: 0 = ROM, 2 = empty (1 = RAM is never written) |
| 2 | raw[16384] | ROM image, **only if type = 0** |

If the type is empty, the loader zeroes the bank. If the type is 1, the loader reads no data and keeps the bank's current contents as RAM.

**0x047C — PAL-banked ROM** (InterWord, Spellmaster and similar):

| Off | Size | Content |
|---|---|---|
| 0 | u8 | bank 0–15 |
| 1 | u8 | type (0 = ROM) |
| 2 | u8 | PAL type: 0 None, 1 InterWord, 2 InterBase, 3 SpellMaster, 4 ViewTrilogy, 5 AdvancedBasicEditor, 6 AdvancedBasicEditorPlus, 7 Quest, 8 WappingEditor, 9 TeletextEditor, 10 WordwisePlus2 ([PALRom.h:33](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/PALRom.h#L33)) |
| 3 | u8 | Currently selected 16K sub-bank |
| 4 | raw[131072] | Full ROM image, always 128 KB, padded for 32K or 64K ROMs |

The loader only reads the image if the chunk length is exactly 131076. It updates the bank type and the PAL type/bank fields before it checks the length.

### 0x0476 — JIM page register (legacy, read only)

Load: `LoadMusic5000JIMPageRegUEF` [Music5000.cpp:353](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Music5000.cpp#L353). Body: u8, the last value written to &FCFF. Since v13 this value lives in 0x0477 instead.

### 0x0467 — 6522 VIA

Save: [`SaveSysVIAUEF` SysVia.cpp:965](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/SysVia.cpp#L965) and [`SaveUserVIAUEF` UserVia.cpp:772](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UserVia.cpp#L772), both through [`SaveVIAUEF` Via.cpp:91](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Via.cpp#L91).
Load: the dispatcher reads byte 0 ([UefState.cpp:478](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UefState.cpp#L478)), then calls [`LoadSysVIAUEF` SysVia.cpp:974](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/SysVia.cpp#L974) or [`LoadUserVIAUEF` UserVia.cpp:783](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/UserVia.cpp#L783), both through [`LoadVIAUEF` Via.cpp:113](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Via.cpp#L113).

**v16+ (and v17):**

| Off | Size | Content |
|---|---|---|
| 0 | u8 | 0 = System VIA, 1 = User VIA (the reader treats any non-zero value as User) |
| 1 | u8 | ORB |
| 2 | u8 | IRB |
| 3 | u8 | ORA |
| 4 | u8 | IRA |
| 5 | u8 | DDRB |
| 6 | u8 | DDRA |
| 7 | s32 | T1 counter in **2 MHz** ticks (BeebEm's internal counter: 1 MHz value × 2, plus the phase) |
| 11 | u16 | T1 latch (1 MHz value) |
| 13 | s32 | T2 counter, 2 MHz ticks |
| 17 | u16 | T2 latch |
| 19 | u8 | ACR |
| 20 | u8 | PCR |
| 21 | u8 | IFR |
| 22 | u8 | IER |
| 23 | bool | T1 has already fired in one-shot mode |
| 24 | bool | T2 has already fired |
| 25 | u8 | System VIA: IC32 addressable-latch state. User VIA: 0 (unused). |
| 26 | u32 | **User VIA only:** shift-register `SRTrigger`, an **absolute** cycle count (see §6, item 1) |

Lengths: System 26, User 30.

**v≤15:** offsets 0–6 as above. Then: 7 u16 T1 counter (1 MHz, i.e. internal ÷ 2), 9 u16 T1 latch, 11 u16 T2 counter (1 MHz), 13 u16 T2 latch, 15 ACR, 16 PCR, 17 IFR, 18 IER, 19 bool T1 fired, 20 bool T2 fired, 21 u8 IC32 or 0. Length 22 for both VIAs.

The loader derives the CA2/CB2 output levels and the shift-register mode from PCR and ACR.

### 0x0468 — Video (6845 CRTC + Video ULA)

Save: `SaveVideoUEF` [Video.cpp:1790](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Video.cpp#L1790). Load: `LoadVideoUEF` [Video.cpp:1846](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Video.cpp#L1846).

| Off | Size | Content |
|---|---|---|
| 0–17 | 18×u8 | CRTC R0–R17 in register order (R0 H total … R12/R13 start address high/low, R14/R15 cursor high/low, R16/R17 light pen high/low) |
| 18 | u8 | Video ULA control register (&FE20) |
| 19–34 | 16×u8 | Palette. Entry *i* = the low nibble last written to &FE21 for logical colour *i*: bits 0–2 = physical colour XOR 7, bit 3 = flash. BeebEm stores `internal ^ 7`, which is the real ULA value. |
| 35 | u16 | `ActualScreenWidth` (BeebEm render width in host pixels) |
| 37 | u32 | `ScreenAdjust` (BeebEm bitmap offset) |
| 41 | u8 | CRTC address register (last value written to &FE00) |
| 42 | u8 | `TeletextStyle` (1 or 2, a BeebEm rendering mode) |
| *v13+:* | | |
| 43 | s32 | `Addr`: 6845 address of the next character row |
| 47 | s32 | `StartAddr`: address of the first character row this frame |
| 51 | s32 | `PixmapLine` (host bitmap line) |
| 55 | s32 | `FirstPixmapLine` |
| 59 | s32 | `PreviousFirstPixmapLine` |
| 63 | s32 | `LastPixmapLine` |
| 67 | s32 | `PreviousLastPixmapLine` |
| 71 | s32 | `CharLine`: character row counter, −1 before the display starts |
| 75 | s32 | `InCharLineUp`: scanline within the character row |
| 79 | s32 | `VSyncState`: > 0 while VSync is high |
| 83 | bool | `IsNewTVFrame` |
| 84 | bool | `InterlaceFrame` (odd/even field) |
| 85 | bool | Was `DoCA1Int`. Always 0 since v17, ignored on load. |
| 86 | s32 | `ova`: memory address used for the current row's data pointer |
| 90 | s32 | `ovn`: byte count for the same pointer |
| 94 | s32 | `CursorFieldCount`: cursor blink countdown in fields |
| 98 | bool | `CursorOnState` |
| 99 | s32 | `CurY`: pixmap line of the cursor, −1 if none |
| 103 | s32 | `Mode7FlashTrigger`: fields until the next teletext flash toggle |
| 107 | bool | `Mode7FlashOn` |
| 108 | s32 | Cycles until the next video (scanline) event, relative to now |

Version ≤12 files end at offset 43. Their loader calls `VideoInit()` after offset 43, so for those files BeebEm rebuilds the raster state itself.

### 0x046B — SN76489 sound

Save: `SaveSoundUEF` [Sound.cpp:891](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Sound.cpp#L891). Load: `LoadSoundUEF` [Sound.cpp:848](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Sound.cpp#L848). On load, BeebEm replays the values as latch/data writes into `Sound_RegWrite`.

"Tone 0" is the channel latched with `&80`/`&90`, "tone 1" with `&A0`/`&B0`, and "tone 2" with `&C0`/`&D0`.

| Off | Size | Content |
|---|---|---|
| 0 | u16 | Tone 0 period (10-bit) |
| 2 | u16 | Tone 1 period |
| 4 | u16 | Tone 2 period |
| 6 | u8 | Tone 0 attenuation (0 = loud … 15 = off) |
| 7 | u8 | Tone 1 attenuation |
| 8 | u8 | Tone 2 attenuation |
| 9 | u8 | Noise control: bits 0–1 rate, bit 2 FB (white noise) |
| 10 | u8 | Noise attenuation |
| 11 | u8 | `LastToneFreqSet`: the channel latched last, for data bytes. **Internal encoding:** 2 = tone 0, 1 = tone 1, 0 = tone 2, 3 = noise. |
| 12 | 4×u16 | `GenIndex[0..3]`: host-sample-rate waveform phase counters. Index 0 = noise, 1 = tone 2, 2 = tone 1, 3 = tone 0. BeebEm-specific. Writing 0 is safe. |

The noise LFSR state is not saved.

### 0x046E — Intel 8271 FDC

Save: `Save8271UEF` [Disc8271.cpp:2871](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Disc8271.cpp#L2871). Load: `Load8271UEF` [Disc8271.cpp:2960](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Disc8271.cpp#L2960).

| Off | Size | Content |
|---|---|---|
| 0 | str | Drive 0 disc image path (absolute host path). Empty if no disc. |
| … | str | Drive 1 disc image path |

Then, relative to the end of the strings:

| Rel | Size | Content |
|---|---|---|
| 0 | s32 | `Disc8271Trigger`: cycles until the next FDC event, or `TMAX` |
| 4 | u8 | Result register |
| 5 | u8 | Status register |
| 6 | u8 | Data register |
| 7 | u8 | `ScanSectorNum` |
| 8 | u32 | `ScanCount` |
| 12 | u8 | Mode register |
| 13 | u8 | Current track, surface 0 |
| 14 | u8 | Current track, surface 1 |
| 15 | u8 | Drive control output port |
| 16 | u8 | Drive control input port |
| 17–20 | 4×u8 | Bad tracks [0][0], [0][1], [1][0], [1][1] |
| 21 | u32 | Command (low byte used) |
| 25 | u32 | Command parameter count |
| 29 | u32 | Current parameter index |
| 33 | raw[16] | Parameter bytes |
| 49 | 2×u32 | NumHeads for drives 0 and 1 |
| 57 | 2×u32 | Drive select 0/1 (0 or 1) |
| 65 | 2×u32 | Disc writeable 0/1 (0 or 1) |
| 73 | u32 | `FirstWriteInt` (0 or 1) |
| 77 | u32 | `NextInterruptIsErr` (low byte used) |
| 81 | u32 | `TrackAddr` (low byte used) |
| 85 | u32 | `CurrentSector` (low byte used) |
| 89 | u32 | `SectorLength` |
| 93 | u32 | `SectorsToGo` |
| 97 | u32 | `ByteWithinSector` |
| *v14+:* | | |
| 101 | u8 | Step rate |
| 102 | u8 | Head settling time |
| 103 | u8 | Index count before head unload |
| 104 | u8 | Head load time |
| 105 | bool | Drive head loaded |
| 106 | bool | Drive head unload pending |
| 107 | 2×u8 | Drive head position 0/1 (FSD) |
| 109 | 2×u8 | Position in track 0/1 (FSD) |
| 111 | bool | `SectorOverRead` |
| 112 | bool | `UsingSpecial` |
| 113 | u8 | `DRDSC` |
| 114 | 2×u8 | FSD logical track 0/1 |
| 116 | 2×u8 | FSD physical track 0/1 |

On load, BeebEm reopens the named images and picks the format from the file extension (`.dsd`, `.fsd`, else SSD). It reads everything after the strings **only if at least one disc was named and the load reported success**. Only the last attempted drive's result is checked.

### 0x046F — WD1770 FDC (Master, or Model B / B+ / Integra-B with a 1770 board)

Save: `Save1770UEF` [Disc1770.cpp:1285](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Disc1770.cpp#L1285). Load: `Load1770UEF` [Disc1770.cpp:1355](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Disc1770.cpp#L1355).

| Off | Size | Content |
|---|---|---|
| 0 | u8 | Drive 0 disc type: 0 SSD, 1 DSD, 2 ADFS, 3 IMG, 4 DOS, 5 FSD ([DiscType.h](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/DiscType.h)) |
| 1 | u8 | Drive 1 disc type |
| 2 | str | Drive 0 image path ("" if none) |
| … | str | Drive 1 image path |

Then, relative to the end of the strings:

| Rel | Size | Content |
|---|---|---|
| 0 | u8 | Status register |
| 1 | u8 | Data register |
| 2 | u8 | Track register |
| 3 | u8 | `ATrack`: actual head track |
| 4 | u8 | Sector register |
| 5 | bool | `HeadDir`: 1 = step in |
| 6 | u8 | Current command |
| 7 | u8 | Next command (pending after spin-up or settle) |
| 8 | s32 | `LoadingCycles`: spin-up/settle countdown, in cycles |
| 12 | 2×s32 | `SpinDown[0..1]`: per-drive spin-down countdown |
| 20 | bool | `UpdateTrack` (command flag) |
| 21 | bool | `MultiSect` |
| 22 | u8 | Step rate in **ms** (6/12/20/30) |
| 23 | bool | `SpinUp` |
| 24 | bool | `Verify` |
| 25 | 2×bool | Drive LEDs 0/1 |
| 27 | s32 | `ByteCount` (bytes remaining in the transfer) |
| 31 | u32 | 0 (was `DataPos`, ignored) |
| 35 | u8 | Board drive-control register (`ExtControl`) |
| 36 | u8 | Current drive |
| 37 | 2×s32 | `HeadPos[0..1]`: file offset of the head for each drive |
| 45 | 2×u8 | Current head (side) for each drive |
| 47 | 2×s32 | `DiscStep[0..1]`: image track stride (single- or double-sided) |
| 55 | 2×s32 | `DiscStrt[0..1]`: image side start offset |
| 63 | 2×u8 | Sectors per track |
| 65 | 2×u32 | `DefStart`: offset of side 1 |
| 73 | 2×u32 | `TrkLen`: track length in bytes |
| 81 | 2×bool | Writeable |
| 83 | bool | Disc density, drive 0 (1 = double) |
| 84 | bool | Disc density, drive 1 *(v10+; absent in v≤9)* |
| 85 | bool | Selected density |
| 86 | u8 | `RotSect` (fake rotating sector counter for Read Address) |
| 87 | str | FDC board DLL: `"None"` or a path relative to BeebEm's folder, such as `Hardware\Acorn1770.dll` |

As with the 8271, everything after the strings is applied **only if a disc was named and both reopened successfully**. On non-Master machines the board DLL is then reloaded from the stored name.

### 0x0470 — Tube ULA

Save: `SaveTubeUEF` [Tube.cpp:2673](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Tube.cpp#L2673). Load: `LoadTubeUEF` [Tube.cpp:2719](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Tube.cpp#L2719).

Status bytes use 0x80 = data available, 0x40 = not full. `R1Status` holds the control flags Q=1, I=2, J=4, M=8, V=16, P=32.

| Off | Size | Content |
|---|---|---|
| 0 | u8 | `R1Status` (control flags) |
| 1 | raw[24] | R1 parasite→host FIFO |
| 25 | u8 | R1 P→H FIFO pointer |
| 26 | u8 | R1 host status |
| 27 | u8 | R1 H→P data |
| 28 | u8 | R1 parasite status |
| 29 | u8 | R2 P→H data |
| 30 | u8 | R2 host status |
| 31 | u8 | R2 H→P data |
| 32 | u8 | R2 parasite status |
| 33 | 2×u8 | R3 P→H data |
| 35 | u8 | R3 P→H pointer |
| 36 | u8 | R3 host status |
| 37 | 2×u8 | R3 H→P data |
| 39 | u8 | R3 H→P pointer |
| 40 | u8 | R3 parasite status |
| 41 | u8 | R4 P→H data |
| 42 | u8 | R4 host status |
| 43 | u8 | R4 H→P data |
| 44 | u8 | R4 parasite status |

### 0x0472 — 65C02 co-processor RAM

Save: [`Save65C02MemUEF` Tube.cpp:2714](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Tube.cpp#L2714). Load: [`Load65C02MemUEF` Tube.cpp:2760](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Tube.cpp#L2760). Body: raw[65536].

### 0x0473 — Serial ULA, 6850 ACIA and tape

Save: `SaveSerialUEF` [Serial.cpp:1259](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Serial.cpp#L1259). Load: `LoadSerialUEF` [Serial.cpp:1304](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Serial.cpp#L1304). CSW sub-block: [`SaveCSWState`/`LoadCSWState` Csw.cpp:593](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Csw.cpp#L593).

BeebEm writes this chunk only when a tape is loaded. The loader closes the current tape, then reads the rest **only if the file name is non-empty**.

| Off | Size | Content |
|---|---|---|
| 0 | bool | Serial ULA: 1 = RS423 selected, 0 = cassette |
| 1 | str | Tape image path (absolute host path) |

Then, relative to the end of the string:

| Rel | Size | Content |
|---|---|---|
| 0 | bool | Cassette motor relay |
| 1 | u32 | ACIA Tx baud rate |
| 5 | u32 | ACIA Rx baud rate |
| 9 | u8 | ACIA clock divide |
| 10 | u8 | Parity |
| 11 | u8 | Stop bits |
| 12 | u8 | Data bits |
| 13 | bool | RIE |
| 14 | bool | TIE |
| 15 | u8 | TxD (transmit destination) |
| 16 | u8 | RxD |
| 17 | u8 | RDR |
| 18 | u8 | TDR |
| 19 | u8 | RDSR (Rx shift register) |
| 20 | u8 | TDSR |
| 21 | u8 | ACIA status register |
| 22 | u8 | ACIA control register |
| 23 | u8 | Serial ULA control register (&FE10) |
| 24 | 3×bool + u8 | 0, 0, 0, 0 (old DCD fields, ignored) |
| 28 | s32 | Tape clock (position) |
| 32 | s32 | Tape clock speed at save time. The loader rescales the position if its own speed differs. |
| *v14+:* | | |
| 36 | bool | Tape carrier tone detected |
| 37 | s32 | Carrier cycle countdown (to assert DCD) |
| 41 | bool | Playing |
| 42 | bool | Recording |
| 43 | bool | Unlock (tape protection-bypass option) |
| 44 | s32 | `UEFBuf` (current tape bit buffer) |
| 48 | s32 | `OldUEFBuf` |
| 52 | s32 | `OldClock` |
| 56 | | **CSW only.** Present if the reopened tape is a `.csw` file; the chunk length is not checked. |
| 56 | u8 | `csw_state`: 0 waiting for tone, 1 tone, 2 data, 3 undefined |
| 57 | u8 | `csw_datastate`: 0 receiving data, 1 stop bits, 2 tone or start bit |
| 58 | u8 | `csw_bit` |
| 59 | s32 | `csw_pulselen` |
| 63 | s32 | `csw_ptr` |
| 67 | s32 | `csw_byte` |
| 71 | s32 | `bit_count` |

### 0x0474 — uPD7002 A/D converter

Save: `SaveAtoDUEF` [AtoDConv.cpp:127](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/AtoDConv.cpp#L127). Load: `LoadAtoDUEF` [AtoDConv.cpp:141](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/AtoDConv.cpp#L141).

| Off | Size | Content |
|---|---|---|
| 0 | u8 | Data latch (last value written to &FEC0) |
| 1 | u8 | Status register |
| 2 | u8 | Result high byte |
| 3 | u8 | Result low byte |
| 4 | s32 | Cycles until conversion completes, or `TMAX` |

### 0x0477 — Music 5000

Save: `SaveMusic5000UEF` [Music5000.cpp:323](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Music5000.cpp#L323). Load: `LoadMusic5000UEF` [Music5000.cpp:336](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Music5000.cpp#L336).

| Off | Size | Content |
|---|---|---|
| 0 | raw[2048] | Wave RAM (wavetables plus both register sets) |
| 2048 | 16×u32 | Phase accumulators, one per channel |
| 2112 | u8 | Current channel |
| 2113 | u8 | Cycle counter |
| 2114 | u8 | Active register set |
| 2115 | u8 | 0 (padding) |
| 2116 | s32 | Left sample accumulator |
| 2120 | s32 | Right sample accumulator |
| 2124 | u8 | *(v13+)* JIM page select register (&FCFF) |

### 0x0478 — Z80 co-processor (Acorn Z80 and Torch Z80 share this ID)

Save: `SaveZ80UEF` [Z80Support.cpp:323](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Z80Support.cpp#L323). Load: `LoadZ80UEF` [Z80Support.cpp:354](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Z80Support.cpp#L354). The tube device byte in 0x046A tells the two machines apart.

| Off | Size | Content |
|---|---|---|
| 0 | raw[65536] | Z80 RAM |
| 65536 | u16 | AF bank 0 (A in the high byte) |
| 65538 | u16 | AF bank 1 |
| 65540 | u8 | Active AF bank (0 or 1) |
| 65541 | 3×u16 | BC, DE, HL bank 0 |
| 65547 | 3×u16 | BC, DE, HL bank 1 |
| 65553 | u8 | Active BC/DE/HL bank |
| 65554 | u16 | IR (I in the high byte, R in the low byte) |
| 65556 | u16 | IX |
| 65558 | u16 | IY |
| 65560 | u16 | SP |
| 65562 | u16 | PC |
| 65564 | u16 | IFF1 |
| 65566 | u16 | IFF2 |
| 65568 | u16 | PC again (a duplicate; the loader uses this value) |
| 65570 | u16 | `PreZPC` (previous PC) |
| 65572 | bool | `inROM`: the boot ROM is paged in |

The interrupt mode is not saved.

### 0x0479 — Acorn ARM Evaluation System

Save: `CArm::SaveState` [Arm.cpp:4678](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Arm.cpp#L4678). Load: `CArm::LoadState` [Arm.cpp:4714](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Arm.cpp#L4714).

| Off | Size | Content |
|---|---|---|
| 0 | u32 | PC |
| 4 | u8 | Processor mode (0 USR, 1 FIQ, 2 IRQ, 3 SVC) |
| 5 | u8 | I flag. **Always 0** because of truncation (§6, item 3). |
| 6 | u8 | F flag. **Always 0**. |
| 7 | u8 | NZCV in bits 3..0 |
| 8 | bool | Prefetch invalid |
| 9 | u32 | Prefetched instruction |
| 13 | u32 | Current instruction |
| 17 | 16×u32 | Current register bank `r[0..15]` |
| 81 | 16×u32 | User-mode registers |
| 145 | 16×u32 | SVC-mode registers |
| 209 | 16×u32 | IRQ-mode registers |
| 273 | raw[4194304] | 4 MB RAM |
| 4194577 | u8 | *(v15+)* `armModel`: 1 = ARM1 only, ≥2 enables ARM2 extensions, ≥3 enables ARM3 extensions. Defaults to 1 when absent. |

FIQ-mode registers are not saved.

### 0x047A — Sprow ARM7TDMI co-processor

Save: `CSprowCoPro::SaveState` [SprowCoPro.cpp:843](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/SprowCoPro.cpp#L843). Load: `CSprowCoPro::LoadState` [SprowCoPro.cpp:916](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/SprowCoPro.cpp#L916). The fields are an ARMulator `ARMul_State` dump ([ARMulator/armdefs.h](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/ARMulator/armdefs.h)).

| Off | Size | Content |
|---|---|---|
| 0 | s32 | `m_CycleCount` |
| 4 | u8 | `Emulate` |
| 5 | u32 | `EndCondition` |
| 9 | 16×u32 | `Reg[16]` |
| 73 | 7×16×u32 | `RegBank[7][16]` |
| 521 | u64 | `Accumulator` |
| 529 | u32 | `Cpsr` |
| 533 | 7×u32 | `Spsr[7]` |
| 561 | 9×u8 | NFlag, ZFlag, CFlag, VFlag, IFFlags, SFlag, TFlag, Bank, Mode |
| 570 | 11×u32 | instr, pc, temp, loaded, decoded, NumScycles, NumNcycles, NumIcycles, NumCcycles, NumFcycles, NextInstr |
| 614 | raw[67108864] | 64 MB memory |
| 67109478 | 5×u32 | remapControlRegister, romSelectRegister, LastTime, CP14R0_CCD, Now |
| 67109498 | 9×u8 | Exception, NresetSig, NfiqSig, NirqSig, abortSig, NtransSig, Aborted, Base, AbortAddr (truncated, §6) |
| 67109507 | u32 | *k*: number of peripheral registers |
| 67109511 | *k*×(u32,u32) | ML675001 peripheral I/O registers as (address, value) pairs |

### 0x047B — Master 512 (80186)

Save: `Master512CoPro::SaveState` [Master512CoPro.cpp:3838](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Master512CoPro.cpp#L3838). Load: `Master512CoPro::LoadState` [Master512CoPro.cpp:3905](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Master512CoPro.cpp#L3905). The CPU core is derived from MAME's i186.

| Off | Size | Content |
|---|---|---|
| 0 | raw[1048576] | 1 MB RAM |
| 1048576 | 8×u16 | AX, CX, DX, BX, SP, BP, SI, DI |
| 1048592 | 4×u16 | ES, CS, SS, DS |
| 1048600 | 6×u8 | TF, IF, DF, IOPL, NT, MF |
| 1048606 | u16 | IP |
| 1048608 | u16 | Previous IP |
| 1048610 | 6×u32 | Lazy flag values: SignVal, AuxVal, OverVal, ZeroVal, CarryVal, ParityVal |
| 1048634 | 3×u32 | pending_irq, nmi_state, irq_state |
| 1048646 | 3×u8 | no_interrupt, fire_trap, test_state |
| 1048649 | u32 | icount |
| 1048653 | u32 | prefix_seg |
| 1048657 | 2×bool | seg_prefix, seg_prefix_next |
| 1048659 | u32 | ea |
| 1048663 | u16 | eo |
| 1048665 | u32 | easeg |
| 1048669 | u8 | modrm |
| 1048670 | 3×u32 | dst, src, pc (linear) |
| 1048682 | bool | halt |
| 1048683 | 2×(bool, u32, u32, u16, u16) | DMA channels 0 and 1: drq_state, source, dest, count, control |
| 1048709 | bool | last_dma |

### 0x047D — TMS5220 speech system (with PHROM contents)

Save: `SaveSpeechUEF` [Speech.cpp:2263](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Speech.cpp#L2263) → `TMS5220StreamState::SaveState` [Speech.cpp:2020](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Speech.cpp#L2020) → `TMS5220::SaveState` [Speech.cpp:1842](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Speech.cpp#L1842).
Load: `LoadSpeechUEF` [Speech.cpp:2253](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Speech.cpp#L2253) → [Speech.cpp:2008](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Speech.cpp#L2008) → `TMS5220::LoadState` [Speech.cpp:1777](https://codeberg.org/chrisn/beebem-windows/src/commit/763539a120d1dc90c49be0d0af55193a5e109ce9/Src/Speech.cpp#L1777). The core is derived from MAME's tms5220.

| Off | Size | Content |
|---|---|---|
| 0 | raw[16] | FIFO |
| 16 | 4×u8 | fifo_head, fifo_tail, fifo_count, fifo_bits_taken |
| 20 | 9×bool | previous_TALK_STATUS, SPEN, DDIS, TALK, TALKD, buffer_low, buffer_empty, irq_pin, ready_pin |
| 29 | 2×bool | OLDE, OLDP |
| 31 | u8 | new_frame_energy_idx |
| 32 | u8 | new_frame_pitch_idx |
| 33 | raw[10] | new_frame_k_idx[10] |
| 43 | s16 | current_energy |
| 45 | s16 | current_pitch |
| 47 | 10×s16 | current_k |
| 67 | u16 | previous_energy |
| 69 | 3×u8 | subcycle, subc_reload, PC |
| 72 | u8 | IP |
| 73 | bool | inhibit |
| 74 | u8 | uv_zpar |
| 75 | bool | pitch_zero |
| 76 | u16 | pitch_count |
| 78 | 11×s32 | u[11] (lattice filter) |
| 122 | 10×s32 | x[10] |
| 162 | u16 | RNG |
| 164 | s16 | excitation_data |
| 166 | bool | schedule_dummy_read |
| 167 | u8 | data_register |
| 168 | bool | RDB_flag |
| 169 | bool | digital_select |
| 170 | bool | io_ready |
| 171 | s32 | ready_count |
| 175 | u32 | 262144 (PHROM length; ignored on load) |
| 179 | u32 | speechROMaddr |
| 183 | s32 | load_pointer |
| 187 | u8 | ROM_bits_count |
| 188 | raw[262144] | Speech PHROM data: 16 slots × 16 KB |
| 262332 | 4×s32 | Stream resampler state: last_sample, curr_sample, source_step, source_pos |

---

## 5. Departures from the UEF specification (0.10)

The UEF spec is at <http://electrem.emuunlim.com/UEFSpecs.html>.

1. **The header version field holds the save-state version, not the spec version.** BeebEm writes minor = 17, major = 0 ("UEF 0.17"), which is a UEF version that doesn't exist. On load it compares the 16-bit value against its own save-state version. A strict UEF reader may reject these files or misinterpret them. When reading, treat bytes 10–11 as BeebEm's save-state version once you've identified the file as a BeebEm state (first chunk 0x046C).
2. **The chunk IDs are BeebEm-private.** 0x0460 – 0x047D sit inside the spec's `&04xx` "state snapshot" range, but the spec doesn't define them; it defines only &0400 – &0403, &0410 – &0412 and &0420. BeebEm neither writes nor reads the standard chunks: &0400 6502, &0402 WD1770, &0403 JIM, &0410 memory. So a BeebEm state isn't usable by another UEF-snapshot reader, and the reverse is also true.
3. **No "update byte".** Spec `&04xx` chunks start with an update byte. BeebEm chunks don't.
4. **Strings.** The spec says strings are NUL-terminated. BeebEm v14+ uses a `u32` length prefix with no terminator, and v≤13 used fixed 256-byte fields.
5. **No gzip.** The spec allows the whole file to be gzip-compressed. BeebEm's save-state loader uses plain `fopen`/`fread` and rejects a gzipped state as "not a UEF file". (Gzip is only handled on the separate tape-UEF path.) Write save states uncompressed.
6. **No origin chunk (&0000).** BeebEm identifies itself with private chunk 0x046C instead. For `.uef` files, 0x046C must be the very first chunk, or BeebEm loads the file as a tape.
7. **Repeated IDs with a discriminator.** 0x0467 appears twice; its first byte selects the System or User VIA. 0x0466, 0x0475 and 0x047C appear once per sideways bank, with the bank number in the first byte. The spec has no such convention, though nothing forbids it.
8. **Chunk order is significant.** 0x046A resets the machine and must precede the other chunks. The meanings of 0x0461 – 0x0464 depend on the machine type set by 0x046A. The spec's model is order-independent.
9. **Host-specific content.** Disc and tape images are referenced by absolute host path, not embedded. Several fields are BeebEm renderer or audio internals: the `ActualScreenWidth`, `ScreenAdjust`, `TeletextStyle` and `*PixmapLine` fields in 0x0468, and `GenIndex` in 0x046B.

---

## 6. BeebEm bugs and quirks to accommodate

1. **The VIA shift-register trigger (`SRTrigger`, v16+) is broken.** Only applies when the version is ≥ 16.
   * **System VIA:** the writer never writes `SRTrigger` (the chunk is 26 bytes), but the reader reads a `u32` after IC32. That read takes the first four bytes of the following chunk's header. In practice the following chunk is the User VIA, so the value is `67 04 1E 00` = 0x001E0467. The reader adds `TotalCycles`, which schedules a spurious SR event about 1 s later. If the System VIA chunk were the last chunk in the file, the load would fail.
   * **User VIA:** the writer writes a padding byte (offset 25) **and then** an **absolute** `SRTrigger` (offset 26). The reader doesn't skip the padding byte, so it reads offsets 25–28 as the value, then adds `TotalCycles` as if it were relative. When the trigger is idle (`TMAX`, bytes `FF FF FF 7F`), the misaligned read gives `0xFFFFFF00` (−256), which is not `TMAX`. So the trigger always appears active.
   * **To be read correctly by BeebEm:** write exactly BeebEm's layout (System 26 bytes, User 30 bytes). Your `SRTrigger` value will be misread whatever you write, so `TMAX` is fine.
   * **When reading:** ignore the System VIA's missing field. For the User VIA, read `SRTrigger` at offset 26. If it isn't `TMAX`, you can turn it into a relative delay with `SRTrigger − TotalCycles`, using the `TotalCycles` stored in 0x0460 (both are 32-bit `int` values that wrap). Or just recompute it from ACR/SR state.
2. **0x0460's `TotalCycles` is written but ignored on load.** All other timers are stored relative to it, except the User VIA `SRTrigger` (item 1). You can write any value, but writing your current cycle count keeps the relationship with `SRTrigger` meaningful.
3. **The ARM (0x0479) I and F interrupt-disable flags are always 0.** BeebEm holds them at bit positions 27 and 26 but writes them with `UEFWrite8`, which truncates them. FIQ-bank registers are not saved. When reading a BeebEm file, you can't recover I and F; assume both are clear, as BeebEm does.
4. **Sprow `AbortAddr` is truncated to 8 bits.** It is a word address in ARMulator.
5. **Disc and tape state is conditional.**
   * 8271 and 1770 controller state is only applied if at least one image path is non-empty and the images reopen successfully. For the 8271 only the last attempted drive's result is checked.
   * On a Model B with a 1770 board and no disc loaded, the board choice (FDC DLL name) is **not** restored from the file. It comes from BeebEm's preferences, plus the "not native FDC" byte in 0x046A.
   * Serial/tape state is only written when a tape is loaded, and only read when the stored path is non-empty. The CSW tail is read according to the type of the reopened tape, not the chunk length.
   * A Master ET with `NativeFDC` set writes an 8271 chunk.
6. **v≤13 FDC DLL field:** the old writer wrote 256 bytes, but the current reader reads `MAX_PATH` = 260 bytes, so it reads 4 bytes past the chunk end. This is harmless unless 0x046F is the last chunk in the file.
7. **Speech:** when speech is enabled but the PHROMs failed to start, the writer emits a **zero-length** 0x047D chunk. If the loading BeebEm has speech running, it will read into the next chunk.
8. **0x047C:** when the length isn't exactly 131076, the loader skips the ROM image but still sets that bank's type and PAL fields.
9. **0x046A key map:** for non-user mappings the writer emits 256 zero bytes rather than a `str`. Legacy pre-v14 keyboard-mapping values other than 0 or 40060 (old menu IDs) are passed through unvalidated.
10. **Z80:** PC appears twice and the second copy wins. The interrupt mode is not saved.
11. **Version 17 files won't load in released BeebEm (≤ 4.23, version 16)**, even though the layout is identical. Version 15 (BeebEm 4.20) uses the old 16-bit VIA timer layout.

---

## 7. Checklist for a BeebEm-compatible writer

* Header: `"UEF File!\0"`, then `0x10 0x00` (version 16) for the widest compatibility.
* First chunk: 0x046C (16 bytes: `"BeebEm"`, padding, then two version bytes; any values are accepted).
* Then 0x046A, then 0x0460, then 0x0461, 0x0462 and the model's RAM chunks, then all 16 bank chunks.
* Then 0x0468, 0x0467 (System, 26 bytes), 0x0467 (User, 30 bytes), 0x046B, and 0x046E or 0x046F.
* Then any tube/co-processor chunks, 0x0473 if a tape is loaded, 0x0474, and optionally 0x0477 and 0x047D.
* All scheduled-event fields (8271 trigger, A/D trigger, video trigger) are cycles **relative to now**, with `0x7FFFFFFF` meaning none. The 1770 `LoadingCycles` and `SpinDown` fields are already countdowns.
* VIA counters (v16) are 2 MHz ticks; latches are 1 MHz values.
* Palette bytes are the raw `&FE21` low nibbles.
* Disc and tape fields hold host paths; BeebEm tries to open them.
