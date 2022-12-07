# Install on Windows

Requires a fast PC running Windows 7 or better.

Get a binary .zip release from the
[b2 GitHub releases page](https://github.com/tom-seddon/b2/releases).
Get the latest one with files attached - probably the one at the top!

Unzip to a folder of your choice and run `b2.exe`.

## Open disk images with b2

You can set b2 as the program to open disk images in Windows Explorer:

1. Right click on disk image file and select `Properties`

2. Click the `Change...` button next to the `Opens with:` line

3. Select `More apps` from the list, scroll to the bottom, and pick
   `Look for another app on this PC`
   
4. Find `b2.exe` and select that

When opening disk images in this way, b2 will launch, load the disk
into drive 0, and attempt to auto-boot it. An existing copy of b2 will
be used to do this if there's one running.

(You don't get any control over which window is used if you've got
more than one copy running, or multiple windows open.)

## Windows 7 performance

If you have trouble with slow startup and poor performance on Windows
7, try running `b2.exe` from the command line with the `--timer`
option: `b2 --timer`.

This setting is sticky, and will be saved on exit for future runs. So
after you've done this once, you can just run it from Windows Explorer
in future.
