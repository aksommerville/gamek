# Gamek Map Format

A Map is basically an Image, with a few quirks:

- Each cell is 8 bits.
- Size can't be more than 255x255.
- Stride must be minimal.
- Extra data attachments.

Each cell presumably names a tile in some tilesheet.

Our `mkdata` tool can generate maps from sensible text, or you can compose them yourself binary.

## Binary format

Content is self-terminated, with a minimum length of 3.

```
0000   1 Width in cells
0001   1 Height in cells
0002 ... Cells, LRTB, length=w*h
.... ... Commands
....   1 0x00 Commands terminator
```

Commands begin with a 1-byte opcode followed by payload.
Opcodes below 0xe0 have a knowable length and are safe to ignore if unknown.

- 0x00..0x1f: Length 0
- 0x20..0x3f: Length 1
- 0x40..0x5f: Length 2
- 0x60..0x7f: Length 4
- 0x80..0x9f: Length 6
- 0xa0..0xbf: Length 8
- 0xc0..0xdf: First byte of payload is the remaining length 0..255
- 0xe0..0xff: No generically-knowable length. Decoder must fail if unknown.

Games are free to use the map commands however they like.
I'll predefine some for convenience (eg might add generic tooling in the future). TODO

## Text format

Line-oriented text in distinct sections.
Comments and extra whitespace are *not* permitted everywhere, depends on the section.
Each section begins with a line containing only the section name.

`LEGEND`

Comments and whitespace not permitted.
Each line is the 2-byte identifier, one space, then the integer cell value 0..255.

`MAP`

Comments and whitespace not permitted.
Each line is either empty or an array of identifiers defined in LEGEND.
You may also use hexadecimal bytes as identifiers, as long as they aren't defined in LEGEND.
Each line must be exactly the same length.

`COMMANDS`

Comments permitted and extra whitespace ignored.
Must come after MAP section.

TODO If I define some default commands, make friendly text formats for them.

Each line is one command, and each token in that line is a word.
Words in hexadecimal, we select a default output length based on the token length (0x00=>1, 0x0000=>2, 0x00000000=>4).
Multibyte words are big-endian if unspecified.
You can append an underscore and format flags:
- b: big-endian (default)
- l: little-endian
- 1: one byte
- 2: two bytes
- 3: three bytes
- 4: four bytes

See discussion under "Binary format"; we validate command lengths during compilation if possible.
