APP_DIR=/usr/local/bin
MAN_DIR=/usr/local/man/man1
CFLAGS=-Wall -Werror
APP=huffman

ifeq ($(DEBUG),y)
CFLAGS+=-g
endif

OBJS=huffman.o huffman_decoder.o huffman_encoder.o huffman_io.o

%.o: %.c huffman.h
	gcc $(CFLAGS) -c $<

$(APP): $(OBJS)
	gcc -o $@ $^ -lm

install:
	install --strip --mode=755 $(APP) $(APP_DIR)
	install man1/huffman.1 $(MAN_DIR)

uninstall:
	rm -f $(APP_DIR)/$(APP)
	rm -f $(MAN_DIR)/huffman.1

clean:
	rm -rf *.o

cleanall: clean
	rm -rf tags $(APP)

