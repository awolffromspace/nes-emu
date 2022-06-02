# NES Emulator

## Introduction

This aims to be a cycle-accurate NES emulator, developed with the goal of learning more about emulators and computer architecture. Currently, audio and mappers other than mapper 0 and 1 have not been implemented. Also, VSync with a 60 Hz monitor is currently required for proper frame timing.

## Usage (Debian Linux)

Install dependencies (Clang and SDL) and compile:

```
make install
make
```

Run an .NES file:

```
./nes-emu filename.nes
```

Controls:  
| Keyboard     | Joystick      |
| ---          | ---           |
| Z            | A             |
| X            | B             |
| up arrow     | D-pad up      |
| down arrow   | D-pad down    |
| left arrow   | D-pad left    |
| right arrow  | D-pad right   |
| enter/return | start         |
| right shift  | select        |

Run unit tests from the test directory and the nestest.nes system test:

```
./nes-emu
```

Run debugger:

```
./nes-emu inst debug
```

Edit the `inst` file to change the instructions that are to be executed. The instructions are 6502 assembly that is represented in hexadecimal and separated by line. Anything after `/`, `//`, or a space will be ignored, so that comments can be made.

Enter `s` to step over the next cycle, `c` to continue until the end of the provided instructions (i.e., until it reaches a BRK instruction), `b` followed by a 16-bit hexadecimal address (e.g., `e4fd`) to continue until the program counter reaches that location, `p` to toggle printing the PPU's state, and `q` or any other character to exit.

Run debugger on an .NES file:

```
./nes-emu filename.nes debug
```

## Screenshots

![Donkey Kong GIF](/screenshots/donkey-kong.gif)
![Donkey Kong Screenshot](/screenshots/donkey-kong.png)
![Super Mario Bros. Screenshot](/screenshots/super-mario-bros.png)
![Pac-Man Screenshot](/screenshots/pac-man.png)
![Debug Screenshot 1](/screenshots/debug1.png)
![Debug Screenshot 2](/screenshots/debug2.png)

## Credits

Kevin Horton for [nestest.nes](https://github.com/christopherpow/nes-test-roms/blob/master/other/nestest.nes)  
NESdev wiki for thorough documentation and [other tests](https://www.nesdev.org/wiki/Emulator_tests) (especially blargg's)  
FCEUX for having a robust debugger to compare with