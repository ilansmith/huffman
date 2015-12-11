#ifndef _HUFFMAN_H_
#define _HUFFMAN_H_

#include <limits.h>

#define ANSI_CHAR_SET_CARDINALITY 128
#define MAX_FILE_NAME_SIZE 256

#define BYTE 8

#define MAX_FILE_LENGTH_REPRESENTATION_U8 UCHAR_MAX /* 255 */
#define FILE_LENGTH_REPRESENTATION_U8 'c'
#define MAX_FILE_LENGTH_REPRESENTATION_U16 USHRT_MAX /* 65535 */
#define FILE_LENGTH_REPRESENTATION_U16 's'
#define MAX_FILE_LENGTH_REPRESENTATION_U32 ULONG_MAX /* 4294967295 */
#define FILE_LENGTH_REPRESENTATION_U32 'l'

#define HUFFMAN_EOF ((unsigned char)EOF)
#define ASSERT(x) if (x) return -1

#define HUFF_NODE_LSON(x) ((x)->left_son)
#define HUFF_NODE_RSON(x) ((x)->right_son)
#define HUFF_NODE_ISLEAF(x) ((x) && !HUFF_NODE_LSON(x) && !HUFF_NODE_RSON(x))
#define HUFF_NODE_NEXT(x) ((x)->next)
#define HUFF_NODE_CHAR(x) ((x)->character)
#define HUFF_NODE_FREQ(x) ((x)->frequency)

/* printing the huffman tree */
#define HUFF_PRINT_ROOT '*'
#define HUFF_PRINT_RIGHT 'R'
#define HUFF_PRINT_LEFT 'L'

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef struct node {
	struct node *left_son;
	struct node *right_son;
	struct node *next;
	u8 character;
	int frequency;
} huff_tree_node_t;

typedef enum bit_t {
	ZERO = 0,
	ONE = 1,
	NO_BIT = 2,
} bit_t;

extern char compressed_file_name[MAX_FILE_NAME_SIZE];
extern char uncompressed_file_name[MAX_FILE_NAME_SIZE];
extern u32 frequency[ANSI_CHAR_SET_CARDINALITY];
extern u8 character_set_cardinality;
extern u8 representation_length[ANSI_CHAR_SET_CARDINALITY];
extern u32 uncompressed_file_length;
extern int huffman_print_tree;
extern huff_tree_node_t *tree_root;
extern bit_t *dictionary[ANSI_CHAR_SET_CARDINALITY];

/* for statistics option */
extern int huffman_keep_file;
extern u32 compressed_file_length;
extern u32 header_length;
extern u32 coded_length;

/* tree_node opperations */
huff_tree_node_t *huff_tree_node_alloc(u8 character, int freq);
void huff_tree_node_free(huff_tree_node_t *node);
void huff_delete_tree(huff_tree_node_t *node);
void huff_print_tree();

/* bit stack opperations */
bit_t *bit_stack_alloc(int num);
bit_t *bit_stack_clone(bit_t *stack, int offset);
void bit_stack_free(bit_t *ptr);
#endif

