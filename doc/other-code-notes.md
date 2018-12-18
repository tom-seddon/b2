# Submodule remotes

GitHub mostly tracks the right forks. Find the `tom-seddon` copy of
the submodule and use the URL of the forked repo as the `upstream`
remote.

## `imgui`

This was originally a fork of the upstream repo, but it now tracks
somebody else's fork, which is where the docking UI stuff comes from:

    git remote add Flix01 ssh://git@github.com/Flix01/imgui

# Turbo disc (RIP)

After 1770 sets DRQ, wait for CPU to read/write data register. Once
read/written, wait for CPU to execute next RTI. Then go to the next
byte straight away. Also: reduce settle and seek times. (Don't omit
the inter-sector delay, as Watford DDFS needs it.)

This involved a bunch of code in a bunch of places, and ended up a
special case, being a runtime toggle (that affected reproducibility)
rather than part of the BeebConfig. And I never tested it enough to
promote it.

It could return, but probably won't... there are better ways of
providing fast file access, that b2 should probably support instead -
BeebLink (etc.), B-em's VDFS (etc.), (if you want ADFS) some kind of
hard disc emulation, and so on.
