# BeebLink support

[BeebLink](https://github.com/tom-seddon/beeblink) is a file storage
system for the BBC Micro - see the link for instructions and binary
releases.

To use BeebLink with b2, add the `--http` option when running the
server, so that it listens for connections from b2. Run b2 on the same
system, create a new config in b2 with the `BeebLink` box ticked and
the BeebLink ROM loaded, then use `File` > `Change configuration` to
select the new config. You should get the `BeebLink - OK` banner.

Notes:

* if BLFS is the default filing system, the emulated BBC won't boot if
  the server isn't running. Run the server and try again, or select
  another filing system (e.g., with D+BREAK)
* because the server's state is outside the emulator's control, save
  state and timeline functionality is disabled when BeebLink support
  is enabled

# Configuring BeebLink server

If you're not running the server and b2 on the same system, use the
`--http-all-interfaces` option when starting the BeebLink server. *Do
this at your own risk* - BeebLink's HTTP interface has no security.

In b2, use `Tools` > `BeebLink Options` to add the server's URL to the
list of URLs to try - probably along the lines of
`http://192.168.1.2:48875/request` or similar, as per the default.

b2 will try the servers in the order given.
