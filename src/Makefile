CXX = clang++ $(CXXFLAGS)
CXXFLAGS = -Wall -O2 -std=c++11
OFILES = driver.o cpu.o ram.o op.o

.SUFFIXES: .o .cpp

nes-emu: $(OFILES)
	$(CXX) $(OFILES) -o ../nes-emu

clean:
	-rm -f *.o *~ ../nes-emu

cpu.o: cpu.cpp cpu.h ram.h op.h
driver.o: driver.cpp cpu.h ram.h op.h
op.o: op.cpp op.h
ram.o: ram.cpp ram.h