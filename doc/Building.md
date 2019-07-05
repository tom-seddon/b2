# Building

This repo has submodules, so you have to clone it to build it - the
source code archives in the releases page won't work. (This is a
GitHub bug and there doesn't appear to be any way around it.)

To clone the repo:

    git clone --recursive https://github.com/tom-seddon/b2.git
	
If you're reading this after already cloning it:

    git submodule init
	git submodule update

Regarding branches, `master` should always build, and shouldn't
contain anything outrageously half-baked - it's the branch used by the
CI servers to prepare releases.

`wip/*` is stuff that's being worked on.

Once you're all set up:

[Instructions for building on Windows](doc/Building-on-Windows.md).

[Instructions for building on OS X](doc/Building-on-OSX.md).

[Instructions for building on Linux/OS X](doc/Building-on-Unix.md).

## Submodule URLs

The submodules are referred to by https. Before cloning, you can use
`git config --global url.ssh://git@github.com/.insteadOf
https://github.com/` to have them cloned over SSH instead, if you have
a GitHub login.
