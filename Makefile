APP_DIR=/usr/local/bin
MAN_DIR=/usr/local/man/man1

huffman: huffman.o huffman_decoder.o huffman_encoder.o huffman_io.o
	gcc -Wall -o $@ huffman.o huffman_decoder.o huffman_encoder.o \
	             huffman_io.o -lm

huffman.o: huffman.c huffman.h
	gcc -Wall -c huffman.c

huffman_decoder.o: huffman_decoder.c huffman.h
	gcc -Wall -c huffman_decoder.c

huffman_encoder.o: huffman_encoder.c huffman.h
	gcc -Wall -c huffman_encoder.c

huffman_io.o: huffman_io.c huffman_io.h huffman.h
	gcc -Wall -c huffman_io.c

install:
	install -C --strip --mode=755 huffman $(APP_DIR)
	install -C man1/huffman.1 $(MAN_DIR)

uninstall:
	rm -f $(APP_DIR)/huffman
	rm -f $(MAN_DIR)/huffman.1

clean:
	rm -rf *.o

cleanall:
	rm -rf *.o tags huffman

