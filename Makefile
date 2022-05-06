.PHONY: nes-emu clean

nes-emu:
	make -C src

clean:
	make clean -C src