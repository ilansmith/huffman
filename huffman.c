#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "huffman.h"

#define HUFFMAN_OPTIONS "hpksve:d:"
#define HUFFMAN_OPT_FAIL 0x00
#define HUFFMAN_OPT_ENCODE 0x01
#define HUFFMAN_OPT_DECODE 0x02
#define HUFFMAN_OPT_STATISTICS 0x04
#define HUFFMAN_OPT_VERBOSE 0x08
#define HUFFMAN_OPT_HELP 0x10
#define HUFFMAN_OPT_PRINT_TREE 0x20
#define HUFFMAN_OPT_KEEP_FILE 0x40

#define KILO 1000
#define KILO_BYTE 1024
#define MEGA 1000000
#define MEGA_BYTE 1048576
#define GIGA 1000000000
#define GIGA_BYTE 1073741824

#define HUFFMAN_SUFFIX ".huf"
#define HUFFMAN_SUFFIX_LENGTH (strlen(HUFFMAN_SUFFIX))
#define ALFA_UNDERSCORE ('_')
#define ALFA_MIN ('A')
#define ALFA_MAX ('z')

#define MAX(X,Y) ((X < Y) ? Y : X)
#define MIN(X,Y) ((X < Y) ? X : Y)

char compressed_file_name[MAX_FILE_NAME_SIZE];
char uncompressed_file_name[MAX_FILE_NAME_SIZE];
u32 frequency[ANSI_CHAR_SET_CARDINALITY];
u8 character_set_cardinality;
u8 representation_length[ANSI_CHAR_SET_CARDINALITY];
u32 uncompressed_file_length;
int huffman_print_tree;
huff_tree_node_t *tree_root;
bit_t *dictionary[ANSI_CHAR_SET_CARDINALITY];

int huffman_keep_file;
u32 compressed_file_length;
u32 header_length;
u32 coded_length;

int huffman_encode(void);
int huffman_decode(void);

/* Allocates a new node for the huffman tree. */
huff_tree_node_t *huff_tree_node_alloc(u8 character, int freq)
{
	huff_tree_node_t *node = (huff_tree_node_t*)calloc(1,
		sizeof(huff_tree_node_t));

	if (!node)
		return NULL;

	HUFF_NODE_CHAR(node)=character;
	HUFF_NODE_FREQ(node)=freq;

	return node;
}

/* Frees a huffman tree node. */
void huff_tree_node_free(huff_tree_node_t *node)
{
	free(node);
}

/* Insert node into the minimum priority queue rooted at tree_root. */
void huff_tree_insert_node(huff_tree_node_t *node)
{
	huff_tree_node_t **tree_ptr = &tree_root;

	while (*tree_ptr) {
		if (HUFF_NODE_FREQ(node) < HUFF_NODE_FREQ(*tree_ptr))
			break;
		tree_ptr=&HUFF_NODE_NEXT(*tree_ptr);
	}

	HUFF_NODE_NEXT(node)=*tree_ptr;
	*tree_ptr=node;
}

void huff_delete_tree(huff_tree_node_t *node) 
{
	if (HUFF_NODE_LSON(node))
		huff_delete_tree(HUFF_NODE_LSON(node));

	if (HUFF_NODE_RSON(node))
		huff_delete_tree(HUFF_NODE_RSON(node));

	if (HUFF_NODE_NEXT(node))
		huff_delete_tree(HUFF_NODE_NEXT(node));

	huff_tree_node_free(node);
}

static void huff_print_tree_rec(huff_tree_node_t *node, int offset,
	char node_char)
{
#define NODE_OFFSET 3
#define CHARACTER_BUF_LEN 10
#define CHAR_ZERO 0
#define CHAR_SLASH_A 7
#define CHAR_SLASH_B 8
#define CHAR_TAB 9
#define CHAR_NEWLINE 10
#define CHAR_RETURN 13
#define CHAR_BACKSPACE 27
#define CHAR_SPACE 32
#define CHARACTER(X) ('A' <= X && X <= 'Z')

	int i;

	for (i = 0; i < offset; i++)
		printf(" ");

	printf("%c", node_char);
	if (HUFF_NODE_ISLEAF(node))
		printf("(%i)", representation_length[node->character]);
	printf("->");

	if (!node) {
		printf("NULL\n");
		goto Exit;
	}

	if (HUFF_NODE_ISLEAF(node)) {
		static char ch[10];

		switch(node->character) {
		case CHAR_ZERO:
			snprintf(ch, CHARACTER_BUF_LEN, "\\0");
			break;
		case CHAR_SLASH_A:
			snprintf(ch, CHARACTER_BUF_LEN, "\\a");
			break;
		case CHAR_SLASH_B:
			snprintf(ch, CHARACTER_BUF_LEN, "\\b");
			break;
		case CHAR_TAB:
			snprintf(ch, CHARACTER_BUF_LEN, "\\tab");
			break;
		case CHAR_NEWLINE:
			snprintf(ch, CHARACTER_BUF_LEN, "\\n");
			break;
		case CHAR_RETURN:
			snprintf(ch, CHARACTER_BUF_LEN, "\\r");
			break;
		case CHAR_BACKSPACE:
			snprintf(ch, CHARACTER_BUF_LEN, "\\backspace");
			break;
		case CHAR_SPACE:
			snprintf(ch, CHARACTER_BUF_LEN, "\\space");
			break;
		default:
			snprintf(ch, CHARACTER_BUF_LEN, "%c", node->character);
			break;
		}

		/* printing character */
		printf("'%s'", ch);
		printf(" (%lu)", frequency[node->character]);
		printf("\n");
		goto Exit;
	}

	printf("\n");
	huff_print_tree_rec(HUFF_NODE_RSON(node), offset + NODE_OFFSET,
		HUFF_PRINT_RIGHT);
	huff_print_tree_rec(HUFF_NODE_LSON(node), offset + NODE_OFFSET,
		HUFF_PRINT_LEFT);

Exit:
	return;
}

void huff_print_tree(void)
{
	huff_print_tree_rec(tree_root, 0, HUFF_PRINT_ROOT);
	printf("\n");
}

bit_t *bit_stack_alloc(int num)
{
	int i;
	bit_t *stack = NULL;

	if (!(stack = (bit_t*)calloc(num, sizeof(bit_t))))
		return NULL;

	for (i = 0; i<num; i++)
		stack[i] = NO_BIT;

	return stack;
}

bit_t *bit_stack_clone(bit_t *stack, int offset)
{
	int i;
	bit_t *new_stack = NULL;

	if (!(new_stack = calloc(offset + 1, sizeof(bit_t))))
		return NULL;

	for (i = 0; i < offset; i++)
		new_stack[i] = stack[i];
	new_stack[offset] = NO_BIT;
	return new_stack;
}

void bit_stack_free(bit_t *ptr)
{
	free(ptr);
}

static void huff_usage(char* argv[])
{
#define ASCII_COPYRIGHT 169

	printf("Usage: %s [-p] [-k] [-s | -v] <-e file_name | " 
			"-d file_name.huf>\n", argv[0]);
	printf("       %s [-h]\n\n", argv[0]);
	printf("  Where -p   print the corresponding huffman tree\n");
	printf("        -k   keep the original file\n");
	printf("        -s   display general statistics\n");
	printf("        -v   display verbose output (implies -s)\n");
	printf("        -e   encode the text file 'file_name'\n");
	printf("        -d   decode the compressed file 'file_name.huf'\n");
	printf("        -h   print this message and exit\n\n");
	printf("NOTE:\n");
	printf("- All compressed files must have a '.huf' suffix.\n");
	printf("- Only 128 bit standard ASCII character set is supported.\n");
	printf("\n%c IAS, October 2003\n", ASCII_COPYRIGHT);
}

static int huff_validate_file_name(char *f_name, int f_type)
{
	char *ch_ptr = f_name;
	int ch_length, f_nmlen = strlen(f_name);
	int min_fnlen = f_type == HUFFMAN_OPT_ENCODE ?
		1 : 1 + HUFFMAN_SUFFIX_LENGTH;
	int max_fnlen = f_type == HUFFMAN_OPT_DECODE ? MAX_FILE_NAME_SIZE :
		MAX_FILE_NAME_SIZE - HUFFMAN_SUFFIX_LENGTH;

	while (*ch_ptr == ALFA_UNDERSCORE)
		ch_ptr++;

	ch_length = strlen(ch_ptr);

	if ((ch_length < min_fnlen) || (max_fnlen < f_nmlen))
		return 0;

	if ((*ch_ptr < ALFA_MIN) || (ALFA_MAX < *ch_ptr))
		return 0;

	switch (f_type) {
	case HUFFMAN_OPT_DECODE:
		ch_ptr = f_name + (f_nmlen - HUFFMAN_SUFFIX_LENGTH);
		if (strncmp(ch_ptr, HUFFMAN_SUFFIX, HUFFMAN_SUFFIX_LENGTH))
			return 0;
		break;	    
	}

	return 1;
}

static char *huff_compress_file_name(char *uncompressed_fn)
{
	static char name_buf[MAX_FILE_NAME_SIZE];
	int uncompressed_length = strlen(uncompressed_fn);

	if (!huff_validate_file_name(uncompressed_fn, HUFFMAN_OPT_ENCODE))
		goto Error;
	strncpy(name_buf, uncompressed_fn, uncompressed_length);
	strncpy(name_buf + uncompressed_length, HUFFMAN_SUFFIX,
			HUFFMAN_SUFFIX_LENGTH);

	return name_buf;

Error:
	printf("invalid file name: %s\n", uncompressed_fn);
	return NULL;
}

static char *huff_uncompress_file_name(char *compressed_fn)
{
	static char name_buf[MAX_FILE_NAME_SIZE];
	int length_fn = strlen(compressed_fn);

	if (!huff_validate_file_name(compressed_fn, HUFFMAN_OPT_DECODE))
		goto Error;

	strncpy(name_buf, compressed_fn, length_fn - HUFFMAN_SUFFIX_LENGTH);

	return name_buf;

Error:
	printf("unknown suffix: %s\n", compressed_fn);
	return NULL;
}

static int huff_set_names(char *uncompressed_fn, char *compressed_fn)
{
	if (!uncompressed_fn || ! compressed_fn)
		return -1;

	strncpy(uncompressed_file_name, uncompressed_fn, MAX_FILE_NAME_SIZE);
	strncpy(compressed_file_name, compressed_fn, MAX_FILE_NAME_SIZE);

	return 0;
}

static int huff_parse_command_line(int argc, char* argv[])
{
	char option;
	int ret = 0, expected_arg_num = 3;

	while (((option = getopt(argc, argv, HUFFMAN_OPTIONS)) != -1)) {
		switch (option) {
		case 'h':
			expected_arg_num = 2;
			ret |= HUFFMAN_OPT_HELP;
			break;
		case 'p':
			if (ret & HUFFMAN_OPT_PRINT_TREE)
				goto Error;
			expected_arg_num++;
			ret |= HUFFMAN_OPT_PRINT_TREE;
			break;
		case 'k':
			if (ret & HUFFMAN_OPT_KEEP_FILE)
				goto Error;
			expected_arg_num++;
			ret |= HUFFMAN_OPT_KEEP_FILE;
			break;
		case 'v':
			if (ret & HUFFMAN_OPT_VERBOSE)
				goto Error;
			ret |= HUFFMAN_OPT_VERBOSE;
			/* fall through */
		case 's':
			if (ret & HUFFMAN_OPT_STATISTICS)
				goto Error;
			expected_arg_num++;
			ret |= HUFFMAN_OPT_STATISTICS;
			break;
		case 'e':
			if ((ret & (HUFFMAN_OPT_ENCODE | HUFFMAN_OPT_DECODE)) ||
				huff_set_names(optarg,
					huff_compress_file_name(optarg))) {
				goto Error;
			}
			ret |= HUFFMAN_OPT_ENCODE;
			break;
		case 'd':
			if ((ret & (HUFFMAN_OPT_ENCODE | HUFFMAN_OPT_DECODE)) ||
				huff_set_names(
					huff_uncompress_file_name(optarg),
					optarg)) {
				goto Error;
			}
			ret |= HUFFMAN_OPT_DECODE;
			break;
		default:
			goto Error;
		}
	}

	if (((ret & HUFFMAN_OPT_HELP) && (ret ^ HUFFMAN_OPT_HELP)) ||
		(expected_arg_num != argc)) {
		goto Error;
	}

	return ret;

Error:
	printf("try `%s -h' for more information\n", argv[0]);
	return HUFFMAN_OPT_FAIL;
}

static char *huff_print_length(u32 file_length)
{
#define BUFFER_LENGTH 10
	static char length_buf[BUFFER_LENGTH];

	memset(length_buf, 0, BUFFER_LENGTH);

	if (file_length == 1) {
		snprintf(length_buf, BUFFER_LENGTH -1, "1byte");
	} else {
		if (file_length < KILO) {
			snprintf(length_buf, BUFFER_LENGTH - 1, "%lubytes",
				file_length);
		} else {
			if (file_length < MEGA) {
				snprintf(length_buf, BUFFER_LENGTH - 1,
					"%.2fKb",
					(double)file_length/KILO_BYTE);
			} else {
				if (file_length < GIGA) {
					snprintf(length_buf, BUFFER_LENGTH - 1,
						"%.2fMb",
						(double)file_length/MEGA_BYTE);
				} else {
					snprintf(length_buf, BUFFER_LENGTH - 1,
						"%.2fGb",
						(double)file_length/GIGA_BYTE);
				}
			}
		}
	}

	return length_buf;
}

static void huff_print_statistics(void)
{
	printf("%s: %s\n", compressed_file_name,
		huff_print_length(compressed_file_length));
	printf("%s:     %s\n", uncompressed_file_name,
		huff_print_length(uncompressed_file_length));
	if (uncompressed_file_length) {
		printf("length ratio: %.2f%%\n",
			(double)compressed_file_length /
			uncompressed_file_length * 100);
	}
}

static void huff_print_verbose(void)
{
	int header_byte_remainder = header_length % BYTE;
	u32 breakdown[ANSI_CHAR_SET_CARDINALITY]; /* length frequency */
	int max_rep_length = 0, min_rep_length = ANSI_CHAR_SET_CARDINALITY, i;
	double character_weight[ANSI_CHAR_SET_CARDINALITY];
	double mean = 0, std_dev = 0, var = 0;
	double uncompressed_file_length_in_bytes =
		(double)uncompressed_file_length;

	if (!uncompressed_file_length)
		return;

	/* printing header details */
	printf("huffman header size: %ibytes", (int)(header_length / BYTE));
	if (header_byte_remainder) {
		printf(" and %ibit", header_byte_remainder);
		if (header_byte_remainder != 1)
			printf("s");
	}
	if (compressed_file_length < (GIGA / 4))
		compressed_file_length *= BYTE;
	else
		header_length /= BYTE;

	/* printing persentage of header in compressed file */
	printf(" (%.2f%% of %s)\n",
		(double)header_length / (double)compressed_file_length * 100,
		compressed_file_name);

	/* printing huffman compression ratio */
	if (uncompressed_file_length < (GIGA / 4))
		uncompressed_file_length *= BYTE;
	else
		coded_length /= BYTE;
	printf("huffman compression ratio: %.2f%%\n",
		(double)coded_length / (double)uncompressed_file_length * 100);

	/* character representation statistics */
	printf("number of different characters used in %s: %icharacters\n",
		uncompressed_file_name,	character_set_cardinality);

	if (character_set_cardinality == 1) {
		printf("character representation length: 0bits\n");
		return;
	}

	/* computing character representation length mean, varience and
	 * stdandard deviation.
	 * the statistics are for the characters in the uncompressed file
	 *
	 * computing mean */
	memset(character_weight, 0, ANSI_CHAR_SET_CARDINALITY);
	for (i = 0; i < ANSI_CHAR_SET_CARDINALITY; i++) {
		mean += (character_weight[i] = representation_length[i] * 
			frequency[i]);
	}
	mean /= (double)uncompressed_file_length_in_bytes;

	/* computing max_rep_length, min_rep_length, varince and breakdown
	 * table */
	memset(breakdown, 0, ANSI_CHAR_SET_CARDINALITY);
	for (i = 0; i < ANSI_CHAR_SET_CARDINALITY; i++) {
		if (frequency[i]) {
			min_rep_length = MIN(min_rep_length,
				representation_length[i]);
			max_rep_length = MAX(max_rep_length,
				representation_length[i]);
			var += frequency[i] * pow(representation_length[i] -
				mean, 2);
			breakdown[representation_length[i]] += frequency[i];
		}
	}
	var /= (double)uncompressed_file_length_in_bytes;

	/* computing standard deviation */
	std_dev = sqrt(var);

	/* printing character representation length  table*/
	printf("\ncharacter representation length (bits)     " \
		"number of characters \n");
	printf("--------------------------------------     " \
		"--------------------\n");
	for (i = min_rep_length; i <= max_rep_length; i++) {
		if (breakdown[i])
			printf("                %-36i%-lu\n", i, breakdown[i]);
	}

	/* printing: mean, standard deviation and varince */
	printf("\nmean: %.2fbits/character\n", mean);
	printf("standard deviation: %.2f\n", std_dev);
	printf("varience: %.2f\n", var);
}

int main(int argc, char* argv[])
{
	int action;

	if ((action = huff_parse_command_line(argc, argv)) == HUFFMAN_OPT_FAIL)
		goto Error;

	if (action & HUFFMAN_OPT_HELP)
		huff_usage(argv);

	huffman_print_tree = (action & HUFFMAN_OPT_PRINT_TREE) ? 1 : 0;
	huffman_keep_file = (action & HUFFMAN_OPT_KEEP_FILE) ? 1 : 0;

	if ((action & HUFFMAN_OPT_ENCODE) && huffman_encode())
		goto Error;

	if ((action & HUFFMAN_OPT_DECODE) && huffman_decode())
		goto Error;

	if (action & HUFFMAN_OPT_STATISTICS)
		huff_print_statistics();

	if (action & HUFFMAN_OPT_VERBOSE)
		huff_print_verbose();

	return 0;

Error:
	printf("aborting!\n");
	return -1;
}

