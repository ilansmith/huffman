APP_DIR=/usr/local/bin
MAN_DIR=/usr/local/man/man1
CFLAGS=-Wall -Werror

ifeq ($(DEBUG),y)
CFLAGS+=-g
endif

%.o: %.c huffman.h
	gcc $(CFLAGS) -c $<

huffman: huffman.o huffman_decoder.o huffman_encoder.o huffman_io.o
	gcc -o $@ huffman.o huffman_decoder.o huffman_encoder.o huffman_io.o \
	-lm

install:
	install --strip --mode=755 huffman $(APP_DIR)
	install man1/huffman.1 $(MAN_DIR)

uninstall:
	rm -f $(APP_DIR)/huffman
	rm -f $(MAN_DIR)/huffman.1

clean:
	rm -rf *.o

cleanall:
	rm -rf *.o tags huffman

