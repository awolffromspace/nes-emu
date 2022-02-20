CXX = clang++ $(CXXFLAGS)
CXXFLAGS = -Wall -O2 -std=c++11
OFILES = cpu.o driver.o memory.o op.o

.SUFFIXES: .o .cpp

nes-emu: $(OFILES)
	$(CXX) $(OFILES) -o ../nes-emu

clean:
	-rm -f *.o *~ ../nes-emu

cpu.o: cpu.cpp cpu.h op.h memory.h
driver.o: driver.cpp cpu.h op.h memory.h
memory.o: memory.cpp memory.h
op.o: op.cpp op.h