# sim8086

This is an [Intel 8086](https://en.wikipedia.org/wiki/Intel_8086) simulator.
Casey Muratori recommended coding this as an exercise in his [Performance-Aware Programming](https://www.computerenhance.com/p/welcome-to-the-performance-aware) videos.
The only resource that I used is the original Intel 8086 manual.

Features:
- Disassembly of executables for the 8086. Not all instructions are supported yet, but quite a few already. This was tested by running the output through the [Netwide Assembler](https://www.nasm.us/) and verifying that the result matches the original executable exactly.
