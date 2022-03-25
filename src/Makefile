CXX = clang++ $(CXXFLAGS)
CXXFLAGS = -Wall -O2 -std=c++11
OFILES = apu.o cpu.o driver.o io.o mmc.o op.o ppu.o ram.o

.SUFFIXES: .o .cpp

nes-emu: $(OFILES)
	$(CXX) $(OFILES) -o ../nes-emu

clean:
	-rm -f *.o *~ ../nes-emu

apu.o: apu.cpp apu.h
cpu.o: cpu.cpp cpu.h apu.h io.h mmc.h op.h ppu.h ram.h
driver.o: driver.cpp cpu.h apu.h io.h mmc.h op.h ppu.h ram.h
io.o: io.cpp io.h
mmc.o: mmc.cpp mmc.h
op.o: op.cpp op.h
ppu.o: ppu.cpp ppu.h
ram.o: ram.cpp ram.h