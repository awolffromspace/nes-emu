# NES Emulator

## Introduction

This is a cycle-accurate NES emulator, developed with the goal of learning more about emulators and computer architecture. Currently, scrolling, audio, and mappers other than mapper 0 have not been implemented.

## Installation

The emulator is written in C++ and compiled with Clang. SDL is a dependency.

## Usage (Debian Linux)

Install dependencies and compile:

```
make install
make
```

Run an .NES file:

```
./nes-emu filename.nes
```

Controls:  
Z = A button  
X = B button  
Up arrow = D-pad up  
Down arrow = D-pad down  
Left arrow = D-pad left  
Right arrow = D-pad right  
Enter/return = start button  
Right shift = select button

Run unit tests from the test directory and the nestest.nes system test:

```
./nes-emu
```

Run debugger:

```
./nes-emu inst debug
```

Edit the `inst` file to change the instructions that are to be executed. The instructions are 6502 assembly that is represented in hexadecimal and separated by line. Anything after `/`, `//`, or a space will be ignored, so that comments can be made.

Enter `s` to step over the next cycle, `c` to continue until the end of the provided instructions, and `q` or any other character to exit.

Run debugger on an .NES file:

```
./nes-emu filename.nes debug
```

## Screenshots

![Donkey Kong GIF](/images/donkey-kong.gif)
![Donkey Kong Screenshot](/images/donkey-kong.png)
![Super Mario Bros. Screenshot](/images/super-mario-bros.png)
![Pac-Man Screenshot](/images/pac-man.png)
![Debug Screenshot 1](/images/debug1.png)
![Debug Screenshot 2](/images/debug2.png)

## Credits

Kevin Horton for [nestest.nes](https://github.com/christopherpow/nes-test-roms/blob/master/other/nestest.nes)  
NESdev wiki for thorough documentation of the NES