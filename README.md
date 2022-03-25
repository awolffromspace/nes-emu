# NES Emulator (WIP)

## Introduction

This is a cycle-accurate NES emulator, developed with the goal of learning more about emulators and computer architecture. Currently, all aspects of the CPU are complete except for some unofficial opcodes.

## Installation

The emulator is written in C++ and compiled with Clang.

## Usage (Linux)

```
make
./nes-emu inst
```

Edit the `inst` file to change the instructions that are to be executed. The instructions are represented in hexadecimal machine language and separated by line. Anything after `/`, `//`, or a space will be ignored, so that comments can be made.

Enter `s` to step over the next cycle, `c` to continue until the end of the provided instructions, and `q` or any other character to exit.

```
./nes-emu
```

Alternatively, run the emulator without arguments to run unit tests in the test directory as well as the nestest.nes ROM.

## Screenshots

![Screenshot1](/screenshot1.png)
![Screenshot2](/screenshot2.png)

## Credits

Kevin Horton for [nestest.nes](https://github.com/christopherpow/nes-test-roms/blob/master/other/nestest.nes).