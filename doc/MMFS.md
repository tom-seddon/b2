# MMFS Integration

## What is it

MMFS is an SD card interface for BBC Micro computers that uses a memory-mapped I/O port at address `0xFE1C`. The b2 implementation emulates this hardware, allowing the BBC Micro ROM to communicate with virtual MMC/SD card storage.

Two versions are supported:

- **MMFS v1**: Uses MMB (proprietary) files containing up to 511 disc images in a single file
- **MMFS v2**: Uses FAT32 disk images containing individual `.ssd`/`.dsd` files as regular filesystem entries

The implementation is based on [MMBeebXL](https://github.com/mmbeeb/MMBeebXL), which provides the same functionality for the BeebEm emulator on Windows.

## Creating MMFS2 disks on Linux

Helper scripts are provided in `bin/` for managing FAT32 disk images. These tools are Linux-specific.

### Recommended: mmfs2_manager.py

This Python script requires `mtools` but does not require sudo for file operations:

```bash
# Install mtools (one time)
sudo apt install mtools

# Create a 512MB FAT32 image
./bin/mmfs2_manager.py create mybeeb.img --size 512

# Add .ssd/.dsd files
./bin/mmfs2_manager.py add mybeeb.img /path/to/elite.ssd /path/to/repton.ssd

# List contents
./bin/mmfs2_manager.py list mybeeb.img

# Remove files
./bin/mmfs2_manager.py remove mybeeb.img elite.ssd
```

### Alternative: Bash scripts

Two bash scripts are also provided:

1. `create_mmfs2_image.sh` - Creates blank FAT32 images (requires `mkfs.vfat`)
2. `mount_mmfs2_image.sh` - Mounts/unmounts images for traditional file copying (requires sudo)

Example using mount approach:

```bash
./bin/create_mmfs2_image.sh mybeeb.img 1024
./bin/mount_mmfs2_image.sh mount mybeeb.img mnt/
cp /path/to/games/*.ssd mnt/
./bin/mount_mmfs2_image.sh umount mybeeb.img mnt/
```

### Image size recommendations

- Minimum: 32MB (FAT32 requirement)
- Recommended: 512MB-1GB
- Maximum: 8GB (FAT32 limit)

## How to setup in b2

### 1. Enable MMFS hardware

1. Go to **Hardware** → **Configs** → Select/Edit your config
2. Scroll to the **MMFS** section
3. Check the **MMFS** checkbox
4. Click **...** to browse for your image file:
   - For MMFS v1: Select an `.mmb` file
   - For MMFS v2: Select a FAT32 `.img` file
5. Optional: Enable **Enable debug logging** for diagnostics

### 2. Add appropriate ROM

Add the correct MMFS ROM to an available sideways ROM slot:

- For **MMB files** (v1): Use `MMFS/M/MMFS.rom`
- For **FAT32 images** (v2): Use `MMFS2/M/MMFS.rom`

These ROMs are from different subdirectories in the [MMFS releases](https://github.com/hoglet67/MMFS/releases).

### 3. Save and restart

Save the configuration and restart the emulator.

## How to use MMFS and MMFS2 commands

### MMFS v1 (MMB files)

```
*HELP          Show MMFS commands
*DIN 0         Mount disc 0 from MMB
*CAT           Show catalog
*DCAT          List available discs in MMB
```

### MMFS v2 (FAT32 images)

```
*DCAT          List all .ssd files on the FAT32 image
*DIN TEST.SSD  Mount a specific disk image file
*.             Show catalog of currently mounted disc
*RUN <file>    Run a program
CHAIN "<file>" Chain a BASIC program
```

Note: For MMFS v2, you must use `*DIN <filename>` to mount a disk image before accessing its files.

## Brief architecture

MMFS is implemented as additional hardware in b2, similar to SCSI:

- **MMFS Class** (`src/beeb/src/MMFS.cpp`) - Emulates MMC/SD card via SPI protocol
- **Memory-mapped I/O** - Registers at different addresses depending on machine type:
  - BBC B: `0xFE1C`
  - Master: `0xFEDC`
  - (The ROM has these addresses compiled in; the emulator auto-detects the machine type)
- **Configuration** (`src/b2/MMFSConfig.h`) - Stores image path and debug settings
- **UI Integration** (`src/b2/ConfigsUI.cpp`) - Checkbox and file browser

### SPI Protocol

The implementation handles standard MMC commands:

- **CMD0**: Reset card (returns `0x01`)
- **CMD1**: Initialize card (must be called twice: first returns `0x01`, second returns `0x00`)
- **CMD10**: Read card ID (returns 20-byte identification)
- **CMD16**: Set block length (typically 512 bytes)
- **CMD17**: Read data block (512-byte sector)
- **CMD24**: Write data block (512-byte sector)

### Initialization flow

```
User enables MMFS in UI
    ↓
Config saved with mmfs_enabled=true and image_path
    ↓
BeebThread creates BBCMicro with BBCMicroInitFlag_MMFS
    ↓
BBCMicroState creates MMFS instance
    ↓
BBCMicro detects machine type and registers MMIO handlers
  (0xFE1C for BBC B, 0xFEDC for Master)
    ↓
MMFS ROM communicates with emulated card
```

The emulation is ROM-agnostic and works with both MMFS v1 and v2 ROMs by providing block-level access to the image file. The correct memory-mapped I/O address is automatically selected based on the emulated machine type to match the address compiled into the corresponding ROM.
