# Release procedure

For OS X/Windows... Linux users can build it their damn selves.

1. Write code, test on all platforms, etc., commit and push to the
   github

2. Branch from tested commit. Start branch name with `v` and append
   version number, e.g., `v1.0`. Push to github again so the branch is
   there too
   
3. Run `make rel` from clean working copy. This will assemble
   `0Rel\win32\b2-XXX.zip` (Windows) or `0Rel\darwin\b2-XXX.dmg`,
   where `XXX` is the version number from the branch name
   
4. 
