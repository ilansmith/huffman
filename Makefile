huffman: huffman.o huffman_decoder.o huffman_encoder.o huffman_io.o
	gcc -Wall -o huffman huffman.o huffman_decoder.o huffman_encoder.o \
	             huffman_io.o -lm

huffman.o: huffman.c huffman.h
	gcc -Wall -c huffman.c

huffman_decoder.o: huffman_decoder.c huffman.h
	gcc -Wall -c huffman_decoder.c

huffman_encoder.o: huffman_encoder.c huffman.h
	gcc -Wall -c huffman_encoder.c

huffman_io.o: huffman_io.c huffman_io.h huffman.h
	gcc -Wall -c huffman_io.c

install: huffman.exe
	cp huffman.exe /usr/local/bin/huffman

uninstall:
	rm -f /usr/local/bin/huffman
clean:
	rm -f *.o

cleanall:
	rm -f *.o *.exe tags test

