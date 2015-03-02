# The Fungus Virtual Machine

Several years ago, [Alexios Chouchoulas](http://www.bedroomlan.org/~alexios/coding.html#befunge)
wrote a paper describing **Fungus, a physical machine especially designed for the Funge
programming paradigm.** I have a software implementation of that specification, with some
useful extensions; and also a two-dimensional assembler for creating binary programs to
run on the virtual machine.

- Read Chouchoulas' [original Fungus specification](https://quuxplusone.github.io/Fungus/docs/fungus.pdf)
  <small>[PDF, 170K]</small>.
- Read my [Fungus virtual machine documentation](https://quuxplusone.github.io/Fungus).
- Browse a [snapshot of the current sources](https://github.com/Quuxplusone/Fungus/tree/master/src)
  for the Fungus virtual machine and assembler. The accompanying Makefile builds the following programs: 
  - <tt>fungasm</tt>, the Fungus assembler.
  - <tt>simfunge</tt>, the Fungus simulator.
  - <tt>bef2elf</tt>, a utility program that converts arbitrary ASCII text blocks into FungELF program images.
  - <tt>elf2ppm</tt>, a utility program that converts arbitrary FungELF images into 512x512 bitmap images for browsing and debugging.
