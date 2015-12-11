#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "huffman_io.h"

#define BIT_ZERO_0 0x7F  /* 0111 1111 */
#define BIT_ZERO_1 0xBF  /* 1011 1111 */
#define BIT_ZERO_2 0xDF  /* 1101 1111 */
#define BIT_ZERO_3 0xEF  /* 1110 1111 */
#define BIT_ZERO_4 0xF7  /* 1111 0111 */
#define BIT_ZERO_5 0xFB  /* 1111 1011 */
#define BIT_ZERO_6 0xFD  /* 1111 1101 */
#define BIT_ZERO_7 0xFE  /* 1111 1110 */

#define BIT_ONE_0 0x80   /* 1000 0000 */
#define BIT_ONE_1 0x40   /* 0100 0000 */
#define BIT_ONE_2 0x20   /* 0010 0000 */
#define BIT_ONE_3 0x10   /* 0001 0000 */
#define BIT_ONE_4 0x8    /* 0000 1000 */
#define BIT_ONE_5 0x4    /* 0000 0100 */
#define BIT_ONE_6 0x2    /* 0000 0010 */
#define BIT_ONE_7 0x1    /* 0000 0001 */

#define RESET_CHAR 0x0   /* 0000 0000 */

#define CHAR_POW(x, y) ((u8)(pow((double)(x), (double)(y))))

/* Not yet in use
static u8 zero_bit[8] = {BIT_ZERO_0, BIT_ZERO_1, BIT_ZERO_2, BIT_ZERO_3,
    BIT_ZERO_4, BIT_ZERO_5, BIT_ZERO_6, BIT_ZERO_7};
*/

static u8 one_bit[8] = {BIT_ONE_0,BIT_ONE_1, BIT_ONE_2, BIT_ONE_3, BIT_ONE_4,
    BIT_ONE_5, BIT_ONE_6, BIT_ONE_7};

/* Generic functions for allocating a new struct huff_io_t.
 * Used by huff_writer_alloc() and huff_reader_alloc()
 */
static void *huff_io_alloc(void)
{
    return (void *)calloc(1, sizeof(struct huff_io_t));
}

/* Allocates a new huff_writer_t */
static huff_writer_t *huff_writer_alloc(void)
{
    return ((huff_writer_t *)huff_io_alloc());
}

/* Allocates a new huff_reader_t */
static huff_reader_t *huff_reader_alloc(void)
{
    return ((huff_reader_t *)huff_io_alloc());
}

/* Generic function for freeing a struct huff_io_t.
 * Used by huff_writer_free() and huff_reader_free().
 */
static void huff_io_free(struct huff_io_t *ptr)
{
    free(ptr);
}

/* Free a huff_writer_t */
static void huff_writer_free(huff_writer_t *writer)
{
    huff_io_free((struct huff_io_t *)writer);
}

/* Free a huff_reader_t */
static void huff_reader_free(huff_reader_t *reader)
{
    huff_io_free((struct huff_io_t *)reader);
}

/* Read from a file into the reader's major buffer.
 * Return the number of u8s read.
 */
static int huff_read_major_buf(huff_reader_t *reader)
{
    reader->buf_length = fread(reader->major_buf, sizeof(u8),
	MAX_MAJOR_BUF_SIZE, reader->file);

    return reader->buf_length;
}

/* Read from reader's major buffer into its minor buffer.
 * Return 0 if successfuly reading a new value into reader->minor_buf. If the 
 * end of file was reached -1 is returned. */
static int huff_read_minor_buf(huff_reader_t *reader)
{
    if (!(reader->major_offset) && !huff_read_major_buf(reader))
	return -1;

    reader->minor_buf = reader->major_buf[reader->major_offset];
    reader->major_offset++;
    reader->major_offset %= reader->buf_length;
    return 0;
}

/* Create a new huff_reader_t for reading from rfile.
 * Return the new reader if successful in opening rfile and creating the
 * reader, otherwise return NULL.
 */
huff_reader_t *huff_reader_open(const char *rfile)
{
    FILE *fd = NULL;
    huff_reader_t *reader = NULL;

    if (!(fd = fopen(rfile, "rb")) || !(reader = huff_reader_alloc()))
    {
	printf("the file %s does not exist or can not be read\n", rfile);
	return NULL;
    }

    reader->file = fd;
    reader->major_offset = 0;
    reader->minor_offset = 0;
    return reader;
}

/* Close the file that reader reads from and delete reader.
 * Return 0 if successful in closing the file, otherwise -1.
 */
int huff_reader_close(huff_reader_t *reader)
{
    if (fclose(reader->file) == EOF)
	return -1;

    huff_reader_free(reader);
    return 0;
}

u8 huff_reader_reset(huff_reader_t *reader)
{
    reader->major_offset = 0;
    reader->minor_offset = 0;
    return fseek(reader->file, 0, SEEK_SET) ? HUFFMAN_EOF : 0;
}

/* Read one bit from the file read by reader. *bit will contain the value of the
 * bit read.
 * Return 0 if successful, otherwise -1.
 */
int huff_read_bit(huff_reader_t *reader, bit_t *bit)
{
    if (!reader->minor_offset && huff_read_minor_buf(reader))
	return -1;
	
    *bit = (reader->minor_buf & one_bit[reader->minor_offset]) ? ONE : ZERO;
    reader->minor_offset++;
    reader->minor_offset %= MAX_MINOR_BUF_SIZE;
    return 0;
}

/* Read one u8 from the file read by reader. *character will contain the value
 * of the u8 read.
 * Return 0 if successful, otherwise -1
 */
int huff_read_u8(huff_reader_t *reader, u8 *character)
{
    u8 temp_buf = reader->minor_buf;
    int shift_right, shift_left;

    if (huff_read_minor_buf(reader))
	return -1;

    *character = reader->minor_buf;

    shift_right = (MAX_MINOR_BUF_SIZE - reader->minor_offset) %
	MAX_MINOR_BUF_SIZE;
    shift_left = reader->minor_offset ? reader->minor_offset : 
	MAX_MINOR_BUF_SIZE;

    *character >>= shift_right;
    temp_buf <<= shift_left;

    *character |= temp_buf;
    return 0;
}

/* Read one u16 from the file read by reader. *srt will contain the value of
 * the u16 read.
 * Return 0 if successful, otherwise -1
 */
int huff_read_u16(huff_reader_t *reader, u16 *srt)
{
    u8 ch_hi, ch_lo;

    if (huff_read_u8(reader, &ch_lo) || huff_read_u8(reader, &ch_hi))
	return -1;

    *srt = ((u16)ch_hi << MAX_MINOR_BUF_SIZE) + ch_lo;
    return 0;
}

/* Read one u32 from the file read bye reader. *lng will contain the value of
 * the u32 read.
 * Return 0 if successful, otherwise -1.
 */
int huff_read_u32(huff_reader_t *reader, u32 *lng)
{
    u16 srt_hi, srt_lo;

    if (huff_read_u16(reader, &srt_lo) || huff_read_u16(reader, &srt_hi))
	return -1;

    *lng = ((u32)srt_hi << 2*MAX_MINOR_BUF_SIZE) + srt_lo;
    return 0;
}

/* Write writer's major buffer into the file it writes to.
 * Return 0 if successful, otherwise -1.
 */
static int huff_write_major_buf(huff_writer_t *writer)
{
    int i, ret;

    if (writer->major_offset)
	writer->major_offset = 0;

    ret = !(fwrite(writer->major_buf, sizeof(u8), writer->buf_length,
	writer->file) == writer->buf_length);

    writer->buf_length = 0;
    for (i = 0; i < MAX_MAJOR_BUF_SIZE; i++)
	writer->major_buf[i] = 0;

    return ret;
}

/* Write writer's minor buffer into it's major buffer.
 * Return 0 if successful, otherwise -1.
 */
static int huff_write_minor_buf(huff_writer_t *writer)
{
    writer->major_buf[writer->major_offset] = writer->minor_buf;
    writer->major_offset++;
    writer->major_offset %= MAX_MAJOR_BUF_SIZE;
    writer->buf_length++;
    writer->minor_buf = RESET_CHAR;

    if (!(writer->major_offset) && huff_write_major_buf(writer))
	return -1;

    return 0;
}

/* Create a new huff_writer_t for writing to wfile.
 * Return the new writer if successful in creating wfile and creating the
 * writer, otherwise return NULL.
 */
huff_writer_t *huff_writer_open(const char *wfile)
{
    FILE *fd = NULL;
    huff_writer_t *writer = NULL;
    
    if (!(fd = fopen(wfile, "wb")) || !(writer = huff_writer_alloc()))
    {
	printf("the file %s can not be created\n", wfile);
	return NULL;
    }

    writer->file = fd;
    return writer;
}

/* Close the file writer writes to and delete writer.
 * Return 0 if successful in closing the file, otherwise -1.
 */
int huff_writer_close(huff_writer_t *writer)
{
    if (writer->minor_offset && huff_write_minor_buf(writer))
	return -1;

    if (writer->buf_length && huff_write_major_buf(writer))
    {
	return -1;
    }

    if (fclose(writer->file) == EOF)
	return -1;

    huff_writer_free(writer);
    return 0;
}

/* Write one bit into the file that writer writes to.
 * Return 0 if successful, otherwise -1.
 */
int huff_write_bit(huff_writer_t *writer, bit_t bit)
{
    if (bit)
	writer->minor_buf |= one_bit[writer->minor_offset];

    writer->minor_offset++;
    writer->minor_offset %= MAX_MINOR_BUF_SIZE;
    if (!(writer->minor_offset))
	return huff_write_minor_buf(writer);

    return 0;
}

/* Write one u8 into the file that writer writes to.
 * Return 0 if successful, otherwise -1.
 */
int huff_write_u8(huff_writer_t *writer, u8 character)
{
    u8 temp_buf = character;

    temp_buf >>= writer->minor_offset;
    character <<= MAX_MINOR_BUF_SIZE - writer->minor_offset;

    writer->minor_buf |= temp_buf;
    if (huff_write_minor_buf(writer))
	return -1;

    writer->minor_buf = character;
    return 0;
}

/* Write one u16 into the file that writer writes to.
 * Return 0 if successful, otherwise -1.
 */
int huff_write_u16(huff_writer_t *writer, u16 srt)
{
    u8 ch;
    int i;

    for (i = 0; i < 2; i++)
    {
	ch = (u8)(srt >> i*MAX_MINOR_BUF_SIZE);
	if (huff_write_u8(writer, ch))
	    return -1;
    }

    return 0;
}

/* Write one u32 into the file that writer writes to.
 * Return 0 if successful, otherwise -1.
 */
int huff_write_u32(huff_writer_t *writer, u32 lng)
{
    u16 srt;
    int i;

    for (i = 0; i < 2; i++)
    {
	srt = (u16)(lng >> i*2*MAX_MINOR_BUF_SIZE);
	if (huff_write_u16(writer, srt))
	    return -1;
    }

    return 0;
}

