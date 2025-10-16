#!/bin/bash
# Mount/unmount MMFS2 FAT32 disk images
# Usage: ./mount_mmfs2_image.sh <mount|umount> <image_file> [mount_point]

set -e

SCRIPT_NAME=$(basename "$0")
ACTION="$1"
IMAGE_FILE="$2"
MOUNT_POINT="${3:-mnt}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

usage() {
    echo "Usage: $SCRIPT_NAME <mount|umount> <image_file> [mount_point]"
    echo ""
    echo "Mount or unmount a FAT32 disk image for MMFS2"
    echo ""
    echo "Arguments:"
    echo "  mount|umount  Action to perform"
    echo "  image_file    Path to the .img file"
    echo "  mount_point   Directory to mount to (default: mnt)"
    echo ""
    echo "Examples:"
    echo "  $SCRIPT_NAME mount beeb.img          # Mount beeb.img to ./mnt"
    echo "  $SCRIPT_NAME mount beeb.img /mnt/beeb  # Mount to /mnt/beeb"
    echo "  $SCRIPT_NAME umount beeb.img         # Unmount beeb.img"
    echo ""
    exit 1
}

if [ "$#" -lt 2 ] || [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
    usage
fi

case "$ACTION" in
    mount)
        if [ ! -f "$IMAGE_FILE" ]; then
            echo -e "${RED}Error: Image file '$IMAGE_FILE' not found${NC}"
            exit 1
        fi

        if [ ! -d "$MOUNT_POINT" ]; then
            echo "Creating mount point: $MOUNT_POINT"
            mkdir -p "$MOUNT_POINT"
        fi

        if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
            echo -e "${YELLOW}Warning: $MOUNT_POINT is already mounted${NC}"
            exit 1
        fi

        echo "Mounting $IMAGE_FILE to $MOUNT_POINT..."
        sudo mount -o loop,uid=$(id -u),gid=$(id -g) "$IMAGE_FILE" "$MOUNT_POINT"
        
        echo -e "${GREEN}Mounted successfully!${NC}"
        echo ""
        echo "You can now copy .ssd/.dsd files:"
        echo "  cp /path/to/game.ssd $MOUNT_POINT/"
        echo ""
        echo "When done, unmount with:"
        echo "  $SCRIPT_NAME umount $IMAGE_FILE $MOUNT_POINT"
        ;;

    umount|unmount)
        if [ ! -d "$MOUNT_POINT" ]; then
            echo -e "${RED}Error: Mount point '$MOUNT_POINT' does not exist${NC}"
            exit 1
        fi

        if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
            echo -e "${YELLOW}Warning: $MOUNT_POINT is not mounted${NC}"
            exit 1
        fi

        echo "Unmounting $MOUNT_POINT..."
        sudo umount "$MOUNT_POINT"
        
        echo -e "${GREEN}Unmounted successfully!${NC}"
        ;;

    *)
        echo -e "${RED}Error: Unknown action '$ACTION'${NC}"
        echo "Use 'mount' or 'umount'"
        exit 1
        ;;
esac

