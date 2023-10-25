# NES Emulator

## Introduction

This aims to be a cycle-accurate NES emulator, developed with the goal of learning more about emulators and computer architecture. Currently, the following aspects of the NES have not been implemented yet: audio; mappers other than 0, 1, 2, 3, and 7; rarely used CPU opcodes that have unpredictable behavior; and rarely used PPU features, such as OAMADDR, OAMDATA, and the sprite overflow flag. Also, VSync with a 60 Hz monitor is currently required for proper frame timing.

## Usage (Debian/Ubuntu Linux)

Install the dependencies (Clang and SDL) and compile:

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
| X            | A             |
| Z            | B             |
| up arrow     | D-pad up      |
| down arrow   | D-pad down    |
| left arrow   | D-pad left    |
| right arrow  | D-pad right   |
| enter/return | start         |
| right shift  | select        |

To increase the window size, change the `WINDOW_SIZE_MULTIPLIER` in emulator.cpp to a larger integer and run `make` again.

Run the unit and system tests:

```
./nes-emu
```

Run the debugger:

```
./nes-emu inst debug
```

Edit the `inst` file to change the instructions that are to be executed. The instructions are 6502 assembly that is represented in hexadecimal and separated by line. Anything after `/`, `//`, or a space will be ignored, so that comments can be made.

Enter `s` to step over the next cycle, `c` to continue until the end of the provided instructions (i.e., until it reaches a BRK instruction), `b` followed by a 16-bit hexadecimal address (e.g., `e4fd`) to continue until the program counter reaches that location, `p` to toggle printing the PPU's state, and `q` or any other character to exit.

Run the debugger on an .NES file:

```
./nes-emu filename.nes debug
```

## Screenshots

![Super Mario Bros. GIF](/screenshots/super-mario-bros.gif)  
![Donkey Kong Screenshot](/screenshots/donkey-kong.png)
![Pac-Man Screenshot](/screenshots/pac-man.png)
![The Legend of Zelda Screenshot](/screenshots/legend-of-zelda.png)
![Metroid Screenshot](/screenshots/metroid.png)
![Castlevania Screenshot](/screenshots/castlevania.png)
![Battletoads Screenshot](/screenshots/battletoads.png)
![Debug CPU Screenshot](/screenshots/debug-cpu.png)
![Debug PPU Screenshot](/screenshots/debug-ppu.png)

## Test Checklist

- [x] custom instruction tests
- [x] nestest
- [x] instr_test-v5 (official_only)
- [x] cpu_timing_test6
- [x] branch_timing_tests
- [x] vbl_nmi_timing
- [x] ppu_vbl_nmi
- [x] sprite_hit_tests_2005.10.05
- [ ] ppu_sprite_hit (passes all but 09-timing)

## Credits

- Kevin Horton for nestest
- blargg for the following tests: branch_timing_tests, cpu_timing_test6, instr_test-v5, ppu_sprite_hit, ppu_vbl_nmi, sprite_hit_tests_2005.10.05, and vbl_nmi_timing
- NESdev wiki for thorough documentation of the NES
- FCEUX for having a robust debugger to compare with