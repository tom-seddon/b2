# BeebLink support

[BeebLink](https://github.com/tom-seddon/beeblink) is a file storage
system for the BBC Micro - see the link for instructions and binary
releases.

To use BeebLink with b2, add the `--http` option when running the
server, so that it listens for connections from b2. Then create a new
config in b2 with the `BeebLink` box ticked and the BeebLink ROM
loaded. Then restart with the new config.

Notes:

* if BLFS is the default filing system, the emulated BBC won't boot if
  the server isn't running. Run the server and try again, or select
  another filing system (e.g., with D+BREAK)
* because the server's state is outside the emulator's control, save
  state and timeline functionality is disabled when BeebLink support
  is enabled
* b2 and the server must be running on the same PC; for safety's sake,
  the server only accepts connections from localhost
