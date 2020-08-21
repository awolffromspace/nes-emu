# NES Emulator (WIP)
## Introduction
This is a cycle-accurate NES emulator, developed with the goal of learning more about emulators and computer architecture. Currently, the CPU is implemented with opcode-specific execution remaining.
## Installation
The emulator is written in C++ and compiled with Clang.
## Usage
```
make
./a.out inst
```
Edit the `inst` file to change the instructions that are to be executed. The instructions are represented in hexadecimal machine language and separated by line.

Enter `s` to step over the next cycle, `c` to continue until the end of the provided instructions, and `q` or any other character to exit.