# Open disk images with b2

You can set b2 as the program to open disk images in the Windows file
explorer, or Finder on macOS, so you can double click a disk image
file and have b2 boot it straight away. Supported disk image
extensions are `.ssd`, `.dsd`, `.sdd`, `.ddd`, `.ads`, `.adm`, `.adl`
and `.adf`.

When set up this way, double clicking a disk image will load the disk
into drive 0, and attempt to auto-boot it. An existing copy of b2 will
be used to do this if there's one running; otherwise, a new copy will
open.

(You can choose to associate the disk images with either version of
b2, and that one will launch if there's no existing copy running. But
if there's an existing copy running, that one will be used, whichever
version it is.)

You can also drag and drop disk image files onto a b2 window, with the
same result.

## Windows setup

1. Right click on disk image file and select `Properties`

2. Click the `Change...` button next to the `Opens with:` line

3. Select `More apps` from the list, scroll to the bottom, and pick
   `Look for another app on this PC`
   
4. Find `b2.exe` or `b2_Debug.exe` and select that

## macOS setup

1. Right click on disk image file and select `Get Info`

2. In the `Open with` section, click on the dropdown list and select `Other...`

3. Find `b2` or `b2 Debug` using the file selector, and click `Add`

4. Click `Change All...`

# Running multiple instances?

If you run just one instance of b2 at a time, no problem.

But you can run several at once if you like, and in that situation
only one of them will deal with these requests. To tell which, look in
`Tools` > `Options`, `HTTP Server` section: if the HTTP server is
listening, that's the instance that will deal with these requests.

If necessary, you can use the `Stop HTTP server` button to stop the
HTTP server, and that will allow you to use the `Start HTTP server`
button on another instance.
