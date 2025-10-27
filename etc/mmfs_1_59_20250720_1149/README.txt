              MMFS and MMFS2 Release Package
              ==============================

              Version:                  1_59
              Date:            20250720_1149

(c) 2011-2020 Martin Mather (mm67) (original developer)
(c) 2014-2020 David Banks (hoglet) (current maintainer)

This package contains binaries for MMFS and MMFS2 for a variety of
hardware configurations.

The top level directory is MMFS vs MMFS2:

- MMFS is the original version of MMFS that uses a single BEEB.MMB
file on the SD Card to store upto 511 200KB .SSD disk images.

- MMFS2 is a more recent development that dispenses with the BEEB.MMB
container, allowing direct access to .SSD and .DSD disk files.

In both cases, the SD Card should be formatted as FAT32.

Within the MMFS/MMFS2 directory is a further level of directory that
contain versions for specific types of SD Card interface hardware:

- U: User Port connected "Non-Turbo" interface
- T: User Port connected "Turbo" interface
- E: Electron Plus One Printer Port interface
- P: Beeb/Master Printer Port interface
- M: MemoryMapped IO interface (typically &FE18, for BeebEm)
- G: Mega Games Cartridge MKII (in development)

Turbo and Non-Turbo interfaces are incompatible with each other and
need different versions of MMFS. If you are not sure if you have a
Turbo or Non-Turbo interface, ask the vendor, or post a picture on
Stardot.

Interfaces sold on eBay by ctorwy31 (Steve Picton) are Turbo, so
use the versions in the T directory.

Interfaces sold on eBay by clothes_fairys are Non-Turbo, so
use the versions in the U directory.

In these directories you will find a variety of versions of MMFS:

MMFS     Model B/B+: normal ROM version of MMFS           (PAGE=&1900)
MMFSDBG  Model B/B+: - with extra logging enabled         (PAGE=&1900)
MMFS2    Model B/B+: - for a second user port at &FE80    (PAGE=&1900)
MMFS3    Model B/B+: - for a second user port at &FEA0    (PAGE=&1900)
SWMMFS   Model B/B+: - uses sideways RAM as workspace     (PAGE=&0E00)
SWMMFS2  Model B/B+:   - for a second user port at &FE80  (PAGE=&0E00)
ZMMFS    Modem B/B+: - ROM bootloader into sideways RAM   (PAGE=&0E00)

SWMMFS+  Model B+only: uses 12K of private workspace      (PAGE=&0E00)

MAMMFS   Master: normal ROM version of MMFS               (PAGE=&0E00)
MAMMFS2  Master: - for a second user port at &FE80        (PAGE=&0E00)
MAMMFS3  Master: - for a second user port at &FEA0        (PAGE=&0E00)

EMMFS    Electron: normal ROM version of MMFS             (PAGE=&1900)
EMMFSDB  Electron: - with extra logging enabled           (PAGE=&1900)
ESWMMFS  Electron: - uses sideways RAM as workspace       (PAGE=&0E00)
ZEMMFS   Electron: - ROM bootloader into sideways RAM     (PAGE=&0E00)

Pick one that matches your machine, and how you would like to use
MMFS (i.e. program into ROM, or soft-load into sideways RAM).

If in doubt, start with the "normal" version for your machine, and
either program this into a ROM, or load it into a sideways RAM slot.

Some versions use sideways RAM as workspace, resulting in a lower
value of PAGE, leaving more space for programs to use. These versions
must run from sideways RAM that is write-enabled. Otherwise you wll
get a Card? error.
