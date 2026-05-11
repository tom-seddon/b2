Work in progress updated docs. See
https://github.com/tom-seddon/b2/issues/183

# b2

b2 lets you run BBC Micro software on your modern system. Use it run
old games, applications and ROMs.

IMAGE: b2 running something

b2 runs on Windows, macOS and Linux. To install, follow the relevant
installation instructions:

* [Install on Windows](./install_on_windows.md)
* [Install on macOS](./install_on_macos.md)
* [Install on Linux](./install_on_linux.md)

Once up and running:

* [Walkthrough of some of b2's features](./walkthrough.md)
* [Some notes about b2's UI](./ui.md)
* [Keyboard layouts](./keyboard.md)
* [Configuring emulated hardware](./configs.md)

\# Running BBC games

BBC Micro games usually come as disk images, typically .ssd (single
sided, single density) or .dsd (double sided, single density). Game
disks are usually auto booting, and you can run them from b2 using
`File` \> `Run` \> `Disc image...`.

![File > Run > Disc image...](./file_run_disc_image.png)

Select a disk image using the file selector. (A good place to find
games would be https://bbcmicro.co.uk/)

![Selecting Repton disk image file](./repton.ssd.png)

The disk should boot, and the game should run.

![Repton running](./repton.png)
