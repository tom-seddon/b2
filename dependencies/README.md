Folder for any dependencies of b2 that aren't usable as submodules.

# llhttp-release-v9.1.3

https://github.com/nodejs/llhttp/releases/tag/release%2Fv9.1.3

Not realistically useable as a submodule, it seems.

# ProcessorTests

Optional dependency, as it's enormous: 13 GBytes. Clone it into this
folder:

    git clone https://github.com/TomHarte/ProcessorTests
	
Then re-run the initial build setup, which should discover it and add
some additional tests.
