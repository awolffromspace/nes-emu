CXX = clang++ $(CXXFLAGS)
CXXFLAGS = -Wall -O2 -std=c++11
OFILES = cpu.o driver.o memory.o op.o

.SUFFIXES: .o .cpp

a.out: $(OFILES)
	$(CXX) $(OFILES) -o a.out

clean:
	-rm -f *.o *~ a.out

cpu.o: cpu.cpp cpu.h op.h memory.h
driver.o: driver.cpp cpu.h op.h memory.h
memory.o: memory.cpp memory.h
op.o: op.cpp op.h