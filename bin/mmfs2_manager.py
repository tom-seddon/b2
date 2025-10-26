#!/usr/bin/env python3
"""
MMFS2 Image Manager
Manage FAT32 disk images for MMFS2 without requiring sudo mounting
Uses mtools (mcopy, mdir, mdel) for FAT access
"""

import argparse
import subprocess
import sys
import os
from pathlib import Path


class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'


def check_mtools():
    """Check if mtools is installed"""
    try:
        subprocess.run(['mdir', '--version'], 
                      stdout=subprocess.DEVNULL, 
                      stderr=subprocess.DEVNULL)
        return True
    except FileNotFoundError:
        return False


def create_image(image_path, size_mb=512):
    """Create a new FAT32 formatted disk image"""
    if size_mb < 32:
        print(f"{Colors.RED}Error: Size must be at least 32MB{Colors.NC}")
        return False
    
    if size_mb > 8192:
        print(f"{Colors.RED}Error: Size must be 8GB or less for FAT32{Colors.NC}")
        return False
    
    image_path = Path(image_path)
    
    if image_path.exists():
        response = input(f"{Colors.YELLOW}Warning: {image_path} exists. Overwrite? (y/N) {Colors.NC}")
        if response.lower() != 'y':
            print("Aborted.")
            return False
    
    print(f"{Colors.GREEN}Creating MMFS2 FAT32 disk image...{Colors.NC}")
    print(f"  Image: {image_path}")
    print(f"  Size: {size_mb}MB")
    
    # Create blank image
    try:
        subprocess.run(['dd', 'if=/dev/zero', f'of={image_path}', 
                       'bs=1M', f'count={size_mb}', 'status=progress'],
                      check=True)
        
        # Format as FAT32
        print(f"\n{Colors.BLUE}Formatting as FAT32...{Colors.NC}")
        subprocess.run(['mkfs.vfat', '-F', '32', '-n', 'MMFS2', str(image_path)],
                      check=True)
        
        print(f"\n{Colors.GREEN}Success!{Colors.NC} Created {image_path}")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"{Colors.RED}Error: {e}{Colors.NC}")
        return False


def list_files(image_path):
    """List files in the disk image"""
    image_path = Path(image_path)
    
    if not image_path.exists():
        print(f"{Colors.RED}Error: {image_path} not found{Colors.NC}")
        return False
    
    print(f"{Colors.BLUE}Contents of {image_path}:{Colors.NC}\n")
    
    # Use mdir to list files (no sudo needed with mtools!)
    try:
        result = subprocess.run(['mdir', '-i', str(image_path), '-/'],
                              check=True, capture_output=True, text=True)
        print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"{Colors.RED}Error: {e}{Colors.NC}")
        return False


def add_files(image_path, files):
    """Add files to the disk image using mcopy"""
    image_path = Path(image_path)
    
    if not image_path.exists():
        print(f"{Colors.RED}Error: {image_path} not found{Colors.NC}")
        return False
    
    success_count = 0
    
    for file_path in files:
        file_path = Path(file_path)
        
        if not file_path.exists():
            print(f"{Colors.YELLOW}Warning: {file_path} not found, skipping{Colors.NC}")
            continue
        
        print(f"Copying {file_path.name}...")
        
        try:
            # mcopy doesn't need sudo!
            subprocess.run(['mcopy', '-i', str(image_path), str(file_path), '::'],
                         check=True)
            success_count += 1
        except subprocess.CalledProcessError as e:
            print(f"{Colors.RED}Error copying {file_path}: {e}{Colors.NC}")
    
    if success_count > 0:
        print(f"\n{Colors.GREEN}Successfully copied {success_count} file(s){Colors.NC}")
        return True
    else:
        print(f"\n{Colors.RED}No files were copied{Colors.NC}")
        return False


def remove_files(image_path, files):
    """Remove files from the disk image"""
    image_path = Path(image_path)
    
    if not image_path.exists():
        print(f"{Colors.RED}Error: {image_path} not found{Colors.NC}")
        return False
    
    success_count = 0
    
    for filename in files:
        print(f"Removing {filename}...")
        
        try:
            subprocess.run(['mdel', '-i', str(image_path), f'::{filename}'],
                         check=True)
            success_count += 1
        except subprocess.CalledProcessError as e:
            print(f"{Colors.RED}Error removing {filename}: {e}{Colors.NC}")
    
    if success_count > 0:
        print(f"\n{Colors.GREEN}Successfully removed {success_count} file(s){Colors.NC}")
        return True
    else:
        print(f"\n{Colors.RED}No files were removed{Colors.NC}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Manage FAT32 disk images for MMFS2',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s create mybeeb.img --size 1024    # Create 1GB image
  %(prog)s list mybeeb.img                  # List contents
  %(prog)s add mybeeb.img game1.ssd game2.ssd  # Add files
  %(prog)s remove mybeeb.img game1.ssd      # Remove files
        """
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # Create command
    create_parser = subparsers.add_parser('create', help='Create a new disk image')
    create_parser.add_argument('image', help='Path to image file')
    create_parser.add_argument('--size', type=int, default=512,
                             help='Size in MB (default: 512)')
    
    # List command
    list_parser = subparsers.add_parser('list', help='List files in disk image')
    list_parser.add_argument('image', help='Path to image file')
    
    # Add command
    add_parser = subparsers.add_parser('add', help='Add files to disk image')
    add_parser.add_argument('image', help='Path to image file')
    add_parser.add_argument('files', nargs='+', help='Files to add')
    
    # Remove command
    remove_parser = subparsers.add_parser('remove', help='Remove files from disk image')
    remove_parser.add_argument('image', help='Path to image file')
    remove_parser.add_argument('files', nargs='+', help='Files to remove')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    # Check for mtools if needed
    if args.command in ['list', 'add', 'remove']:
        if not check_mtools():
            print(f"{Colors.RED}Error: mtools not found{Colors.NC}")
            print("Install with: sudo apt install mtools")
            return 1
    
    # Execute command
    if args.command == 'create':
        success = create_image(args.image, args.size)
    elif args.command == 'list':
        success = list_files(args.image)
    elif args.command == 'add':
        success = add_files(args.image, args.files)
    elif args.command == 'remove':
        success = remove_files(args.image, args.files)
    else:
        parser.print_help()
        return 1
    
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())

