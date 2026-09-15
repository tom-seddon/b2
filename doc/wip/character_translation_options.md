For copying text data to the clipboard, there are 3 options for
translating BBC Micro characters to the characters used by modern
systems:

- `No translation` - characters are passed through as-is, which will
  be mostly unsurprising, but note that BBC ASCII 96 (`£`) will be
  turned into U+0060 GRAVE ACCENT (`` ` ``)
- `Translate £ only` - BBC ASCII 96 (`£`) will be converted to Unicode
  U+00A3 POUND SIGN, so it'll actually show up as `£`
- `Translate Mode 7 chars` - convert £ as above, and also [convert
  chars so they resemble the Mode 7 character set](./mode_7_chars.md)

There's also an option for handling deletion: `Handle delete`, ticked
by default.

When ticked, the emulator will try to handle delete (ASCII 127) chars
properly, by removing the preceding char from the copied data. This
can't handle every possible case perfectly, but it'll do the right
thing for stuff typed in at the BASIC prompt, where you might have
made a typo, pressed DELETE to delete the problem character(s)s, then
re-typed.

(With `Handle delete` unticked, the copied data will include every
character typed, plus ASCII 127 chars corresponding to each press of
DELETE. This typically isn't what you want - but, if it is, you can
have it.)
