#ifndef _HUFFMAN_IO_H_
#define _HUFFMAN_IO_H_

#include <stdio.h>
#include "huffman.h"

#define MAX_MAJOR_BUF_SIZE 1024
#define MAX_MINOR_BUF_SIZE 8

typedef struct huff_io_t {
    FILE *file;
    u8 major_buf[MAX_MAJOR_BUF_SIZE];
    u8 minor_buf;
    u16 major_offset;
    u8 minor_offset;
    size_t buf_length;
} huff_writer_t, huff_reader_t;

huff_reader_t *huff_reader_open(const char *rfile);
int huff_reader_close(huff_reader_t *reader);
u8 huff_reader_reset(huff_reader_t *reader);
int huff_read_bit(huff_reader_t *reader, bit_t *bit);
int huff_read_u8(huff_reader_t *reader, u8 *character);
int huff_read_u16(huff_reader_t *reader, u16 *srt);
int huff_read_u32(huff_reader_t *reader, u32 *lng);

huff_writer_t *huff_writer_open(const char *wfile);
int huff_writer_close(huff_writer_t *writer);
int huff_write_bit(huff_writer_t *writer, bit_t bit);
int huff_write_u8(huff_writer_t *writer, u8 character);
int huff_write_u16(huff_writer_t *writer, u16 srt);
int huff_write_u32(huff_writer_t *writer, u32 lng);

#endif

