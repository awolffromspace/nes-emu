.PHONY: nes-emu clean

nes-emu:
	make -C src

install:
	sudo apt install clang
	sudo apt-get install libsdl2-dev

clean:
	make clean -C src