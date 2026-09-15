For more about printing from the BBC, see the BBC Micro User guide p
407. (This deals with both serial and parallel printers; the b2
emulated printer is the parallel type.)

By default, the emulated BBC behaves as if there's nothing attached to
the parallel printer port. Use `Printer` > `Parallel printer` to
attach a printer, of a sort.

![`Printer` > `Parallel printer`](./generated/menu.printer.parallel_printer.png)

The emulated b2 printer doesn't support turning printed data into
something that might resemble 1980s printer output, but it will
capture all printed bytes, for copying to the clipboard as text, or
saving to a file.

With printer attached, and printing enabled, any data sent to the
printer is buffered up internally.

![`Printer` > printer data size](./generated/menu.printer.printer_data_size.png)

There's an indicator in the `Printer` menu showing how much data has
been stored.

![`Printer` > `Copy printer buffer
text`](./generated/menu.printer.copy_printer_buffer.png)

`Copy printer buffer text` will copy printed text to the clipboard.
It's explicitly described as "text", because it strips out BBC Micro
VDU control codes, normalizez line endings, and discards any chars
with an ASCII value over 128. You stand a good chance of being able to
paste the result into a word processor or text editor or what have
you.

(Note that only BBC Micro VDU controls codes are stripped out.
Epson-style ESC codes are not specifically handled. The output may
still required some manual fixing up.)

![`Printer` > `Copy options`](./generated/menu.printer.printer_copy_options.png)

Use `Copy options` to configure the [character translation
options](./character_translation_options.md) for copying printer data.

![`Printer` > `Save printer buffer...`](./generated/menu.printer.save_printer_buffer.png)

Save the printer buffer data to a file. Unlike `Copy printer buffer
text`, this always saves every byte, for possible use with some other
program that can process the data.
