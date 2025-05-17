If updating the icons, re-export `icon.png` and then do `make icons`
from the root of the working copy. Commit the result.

On Windows, all required dependencies are included.

On macOS and Linux, ImageMagick is required: https://imagemagick.org/

The Makefile assumes this is on the PATH. Specify `MAGICK=<<path>>` on
the command line to use some other one.

# Using macOS + MacPorts?

For some reason, MacPorts doesn't put ImageMagick on the path. You can
probably find it in `/opt/local/lib/ImageMagick7/bin/magick`.
