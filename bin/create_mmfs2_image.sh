#!/bin/bash
# Create a FAT32 disk image for MMFS2
# Usage: ./create_mmfs2_image.sh <image_name> <size_in_mb>

set -e

SCRIPT_NAME=$(basename "$0")
IMAGE_NAME="${1:-beeb_mmfs2.img}"
SIZE_MB="${2:-512}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $SCRIPT_NAME [image_name] [size_in_mb]"
    echo ""
    echo "Creates a FAT32-formatted disk image for use with MMFS2"
    echo ""
    echo "Arguments:"
    echo "  image_name    Name of the image file to create (default: beeb_mmfs2.img)"
    echo "  size_in_mb    Size in megabytes (default: 512, max: 8192 for FAT32)"
    echo ""
    echo "Examples:"
    echo "  $SCRIPT_NAME                          # Create 512MB image named beeb_mmfs2.img"
    echo "  $SCRIPT_NAME my_beeb.img 1024         # Create 1GB image named my_beeb.img"
    echo ""
    exit 1
}

if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
    usage
fi

# Validate size
if [ "$SIZE_MB" -lt 32 ]; then
    echo -e "${RED}Error: Size must be at least 32MB${NC}"
    exit 1
fi

if [ "$SIZE_MB" -gt 8192 ]; then
    echo -e "${RED}Error: Size must be 8GB or less for FAT32${NC}"
    exit 1
fi

echo -e "${GREEN}Creating MMFS2 FAT32 disk image...${NC}"
echo "  Image name: $IMAGE_NAME"
echo "  Size: ${SIZE_MB}MB"
echo ""

# Check if file exists
if [ -f "$IMAGE_NAME" ]; then
    echo -e "${YELLOW}Warning: $IMAGE_NAME already exists${NC}"
    read -p "Overwrite? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
fi

# Create the image file
echo "Creating image file..."
dd if=/dev/zero of="$IMAGE_NAME" bs=1M count="$SIZE_MB" status=progress

# Format as FAT32
echo ""
echo "Formatting as FAT32..."
mkfs.vfat -F 32 -n "MMFS2" "$IMAGE_NAME"

echo ""
echo -e "${GREEN}Success!${NC} Created $IMAGE_NAME"
echo ""
echo "Next steps:"
echo "  1. Mount the image:"
echo "     mkdir -p mnt"
echo "     sudo mount -o loop,uid=\$(id -u),gid=\$(id -g) $IMAGE_NAME mnt/"
echo ""
echo "  2. Copy .ssd/.dsd files to the mounted image:"
echo "     cp /path/to/your/*.ssd mnt/"
echo ""
echo "  3. Unmount when done:"
echo "     sudo umount mnt/"
echo ""
echo "  4. Use in b2 by calling:"
echo "     SetMMFSMMBPath(state, \"$(pwd)/$IMAGE_NAME\");"
echo ""

