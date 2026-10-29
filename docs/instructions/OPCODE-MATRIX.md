---
hide:
  - navigation
  - toc
---

# ND-500 opcode matrix

Every opcode of the ND-500 in one view: the first byte 0x00 to 0xFF as a 16 by 16
map, and one more 16 by 16 map for each of the four prefix bytes 0xFC, 0xFD, 0xFE
and 0xFF. A cell is coloured by instruction class, shows the class code as text,
and is labelled with the data type, register and mnemonic, for example `W1 +` at
0x54. Select a cell for its opcode in hex, binary and decimal and a link to the
instruction page. The class buttons, the data type buttons (bit, byte, half-word,
word, float, double, no type) and the search box dim every other cell; they can be
combined.

[Open the matrix on its own page](opcode-matrix.html), full width.

<iframe src="../opcode-matrix.html" title="ND-500 opcode matrix" style="width:100%;height:1500px;border:0" loading="lazy"></iframe>

The matrix is built from [instructions.json](instructions.json) by
[tools/gen_opcode_matrix.py](../../tools/gen_opcode_matrix.py) when the site is
built, so it exists on the site only; this page on github.com has no matrix.
The foot of the matrix states how many labels were checked against the
conformance corpus. See also the [instruction set overview](../INSTRUCTION-SET-OVERVIEW.md)
and the [list of all instructions](ALL-INSTRUCTIONS.md).
