# Prerequisites

## All

- Python 2.7+

## Windows

- Info-Zip on the PATH

# Clean release

1. Write code, test on all platforms, etc., commit and push to the
   github

2. Branch from tested commit. Start branch name with `v` and append
   version number, e.g., `v1.0`. Push to github again so the branch is
   there too
   
3. Run `make rel` from clean working copy. This will assemble
   `0Rel\win32\b2-XXX.zip` (Windows) or `0Rel\darwin\b2-XXX.dmg` (OS
   X), where `XXX` is the version number from the branch name
   
# Test release

1. Change to working copy folder and run .py file by hand:

   `./etc/release/release.py --ignore-working-copy --ignore-branch`
   
   (The two switches stop it complaining when the working copy isn't
   clean and/or you're not on a version branch. There are other
   switches too; use `-h` to see the full set.)
   
   This will assemble a .zip (Windows) or .dmg (OS X) file, with a
   `-local` suffix.
   
