Some characters look different on screen in teletext Mode 7 from in
the other modes, and/or from the symbols shown on the keyboard. (See
the BBC Micro User Guide, p 18.)

| Actual char | Mode 7 appearance |
|-------------|-------------------|
| `_`         | `―`               |
| `~`         | `÷`               |
| `^`         | `↑`               |
| `\|`        | `‖`               |
| `\`         | `½`               |
| `{`         | `¼`               |
| `}`         | `¾`               |
| `[`         | `←`               |
| `]`         | `→`               |

This is purely a visual phenomenon. The character is treated the same
from the point of view of BBC programs.

# Copying Mode 7 chars

When copying text to the clipboard, b2 can optionally convert these
characters to ones that resemble their Mode 7 equivalent, if you'd
prefer. The goal is that the copied text then visually resembles what
you see in Mode 7 (which may or may not actually work, depending on
the program you're pasting into and the font it uses).

Note that this actually changes the characters in the data. If you
copy a `[` this way, for example, it wlil be converted to `←`, an
entirely different character from the point of view of modern
programs.

(If pasting into b2: it will apply the reverse translation
automatically, so text will round trip correctly in this case.)
