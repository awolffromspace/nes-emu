CXX = clang++ $(CXXFLAGS) ${SDL}
CXXFLAGS = -Wall -O2 -std=c++20
OBJECTS = apu.o cpu.o cpu-op.o emulator.o io.o mmc.o ppu.o ppu-op.o ram.o sprite.o
SDL = `sdl2-config --cflags --libs` -Wno-unused-command-line-argument

.PHONY: nes-emu clean
.SUFFIXES: .o .cpp

nes-emu: $(OBJECTS)
	$(CXX) $(OBJECTS) -o ../nes-emu

clean:
	-rm -f *.o *~ nes-emu a.out ../nes-emu

apu.o: apu.cpp apu.h
cpu.o: cpu.cpp cpu.h apu.h cpu-op.h io.h ppu.h mmc.h ppu-op.h sprite.h ram.h
cpu-op.o: cpu-op.cpp cpu-op.h
emulator.o: emulator.cpp cpu.h apu.h cpu-op.h io.h ppu.h mmc.h ppu-op.h sprite.h ram.h
io.o: io.cpp io.h
mmc.o: mmc.cpp mmc.h ppu.h ppu-op.h sprite.h
ppu.o: ppu.cpp ppu.h mmc.h ppu-op.h sprite.h
ppu-op.o: ppu-op.cpp ppu-op.h sprite.h
ram.o: ram.cpp ram.h
sprite.o: sprite.cpp sprite.h