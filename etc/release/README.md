# Release process

This probably isn't of much interest for anybody other than me.

# Prerequisites

## All

- Python 2.7+

## Windows

- `7z.exe` on the PATH

# Process

1. Prepare code:

   1.1. Test on all platforms, push to github
   
   1.2. Create branch for version, named after the version, with a `v`
   prefix. For example, for version 0.0.1, `v0.0.1`.
   
        `git branch ver/VERSION`
   
   1.3. Push version branch to origin
   
        `git push origin ver/VERSION`

2. Build on Mac:

   3.1. `git pull`
   
   3.2. `git checkout ver/VERSION`
   
   3.3. `make rel`
   
   (this will produce a dmg in the `build/Rel.r.osx` folder)
   
3. Build on Windows:

   4.1. `git pull`
   
   4.2. `git checkout ver/VERSION`
   
   4.3. `make rel`
   
   (this will produce a zip in the `0Rel` folder)

4. Repeat steps 2 and 3 until everything seems to be in order and both
   platforms have built correctly from the same commit.

5. Do the release on github. Don't include the `ver/` prefix - git
   becomes annoying to use when branches and tags have the same name,
   and SourceTree seems to get very confused.

   5.1. `git tag -a VERSION`
   
   5.2. `git push origin VERSION`
   
   5.3. add assets to the release via https://github.com/tom-seddon/b2/releases

6. Cherry-pick/merge any useful fixes into `master`
