# MMFS Integration

## What is it

MMFS is an SD card interface for BBC Micro computers that uses a memory-mapped I/O port at address `0xFE1C`. The b2 implementation emulates this hardware, allowing the BBC Micro ROM to communicate with virtual MMC/SD card storage.

Two versions are supported:

- **MMFS v1**: Uses MMB (proprietary) files containing up to 511 disc images in a single file
- **MMFS v2**: Uses FAT32 disk images containing individual `.ssd`/`.dsd` files as regular filesystem entries

The implementation is based on [MMBeebXL](https://github.com/mmbeeb/MMBeebXL), which provides the same functionality for the BeebEm emulator on Windows.

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

