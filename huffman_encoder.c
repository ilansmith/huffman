#include <stdlib.h>
#include "huffman.h"
#include "huffman_io.h"

static int tree_height;

static int huff_encoder_prologue(huff_reader_t **r_ptr, huff_writer_t **w_ptr)
{
    if (!(*r_ptr = huff_reader_open(uncompressed_file_name)) ||
	!(*w_ptr = huff_writer_open(compressed_file_name)))
    {
	return -1;
    }

    return 0;
}

static int huff_encoder_epilogue(huff_reader_t *reader, huff_writer_t *writer)
{
    int i;

    if (huff_reader_close(reader) || huff_writer_close(writer))
	return -1;

    /* it is not possible to compress a file of length 0 */
    if (!uncompressed_file_length)
    {
	remove(compressed_file_name);
	printf ("it is not possible to compress a file of zero length\n");
	return -1;
    }

    if (!huffman_keep_file)
	remove(uncompressed_file_name);

    if (huffman_print_tree)
	huff_print_tree();

    if (tree_root)
	huff_delete_tree(tree_root);

    if (!uncompressed_file_length || (character_set_cardinality == 1))
	return 0;

    for (i = 0; i < ANSI_CHAR_SET_CARDINALITY; i++)
    {
	if (dictionary[i])
	    bit_stack_free(dictionary[i]);
    }

    return 0;
}

/* Reads file_name and increases the character count in frequency for each
 * occurence of a character from the ANSI character set.
 * Return -1 if failed to open file_name for reading, if a character read does
 * not belong to the ANSI character set or if failed to close file_name.
 * Otherwise, return 0.
 */
static int huff_encoder_parse(huff_reader_t *reader)
{
    u8 ch;

    if (huff_reader_reset(reader) == HUFFMAN_EOF)
	return -1;

    while (!huff_read_u8(reader, &ch))
    {
	if (ch >= ANSI_CHAR_SET_CARDINALITY)
	{
	    printf("non ANSI character in %s\n", uncompressed_file_name);
	    return -1;
	}

	if (!frequency[ch])
	    character_set_cardinality++;
	frequency[ch]++;
	uncompressed_file_length++;
    }

    return 0;
}

static int huff_encoder_max(int x, int y)
{
    return x < y ? y : x;
}

static int huff_encoder_get_tree_height(huff_tree_node_t *node)
{
    if (!node)
	return 0;

    return huff_encoder_max(huff_encoder_get_tree_height(HUFF_NODE_LSON(node)),
	huff_encoder_get_tree_height(HUFF_NODE_RSON(node))) + 1;
}

/* Insert node into the minimum priority queue rooted at tree_root. */
static void huff_tree_insert_node(huff_tree_node_t *node)
{
    huff_tree_node_t **tree_ptr = &tree_root;
    while (*tree_ptr)
    {
	if (HUFF_NODE_FREQ(node) < HUFF_NODE_FREQ(*tree_ptr))
	    break;
	tree_ptr=&HUFF_NODE_NEXT(*tree_ptr);
    }

    HUFF_NODE_NEXT(node) = *tree_ptr;
    *tree_ptr = node;
}

/* Create a huffman tree for the ANSI character set. The frequency of character
 * ch is indicated in frequency[ch]. This is done in two stages:
 * 1 Create a minimum priority queue rooted at tree_root.
 * 2 While there is more than one node in the minimum priority queue do:
 *   - Create a new (internal)node.
 *   - Set the tree_root to its left son.
 *   - Set the HUFF_NODE_NEXT(tree_root) to its right son.
 *   - Insert the new node into the remaining minimum priority queue.
 * Return 0 if successful, otherwise -1.
 */
static int huff_encoder_create_tree()
{
    huff_tree_node_t *node = NULL;
    int ch;

    if (!uncompressed_file_length || (character_set_cardinality == 1))
	return 0;

    for (ch = 0; ch < ANSI_CHAR_SET_CARDINALITY; ch++)
    {
	if (frequency[ch])
	{
	    if(!(node = huff_tree_node_alloc((u8)ch, frequency[ch])))
		goto Error;
	    huff_tree_insert_node(node);
	}
    }

    while (HUFF_NODE_NEXT(tree_root))
    {
	huff_tree_node_t *l_node = tree_root;
	huff_tree_node_t *r_node = HUFF_NODE_NEXT(tree_root);

	node = huff_tree_node_alloc(HUFFMAN_EOF, HUFF_NODE_FREQ(l_node) +
	    HUFF_NODE_FREQ(r_node));
	if (!node)
	    goto Error;

	tree_root = HUFF_NODE_NEXT(r_node);
	HUFF_NODE_NEXT(l_node) = NULL;
	HUFF_NODE_NEXT(r_node) = NULL;

	HUFF_NODE_LSON(node) = l_node;
	HUFF_NODE_RSON(node) = r_node;
	huff_tree_insert_node(node);
    }

    tree_height = huff_encoder_get_tree_height(tree_root);
    return 0;

Error:
    return -1;
}

/* A recursive function for creating the huffman dictionary.
 * If a leaf is reached in the huffman tree, the current direction stack
 * is cloned at dictionary[HUFF_NODE_CHAR(node)].
 * Otherwise the tree rooted at the current node is traversed while updating
 * the direction stack.
 * Any failure generates a return value of -1. Otherwise 0 is retuned.
 */
static int huff_encoder_dictionary_gen(huff_tree_node_t *node,
    bit_t *stack, int offset)
{
    if (HUFF_NODE_ISLEAF(node))
    {
	return (dictionary[HUFF_NODE_CHAR(node)] =
	    bit_stack_clone(stack, offset)) ? 0 : 1;
    }

    /* HUFF_NODE_LSON is associated with ZERO */
    if (HUFF_NODE_LSON(node))
    {		
	stack[offset] = ZERO;
	if (huff_encoder_dictionary_gen(HUFF_NODE_LSON(node), stack, ++offset))
	    return -1;

	offset--;
    }

    /* HUFF_NODE_RSON is associated with ONE */
    if (HUFF_NODE_RSON(node))
    {
	stack[offset] = ONE;
	if (huff_encoder_dictionary_gen(HUFF_NODE_RSON(node), stack, ++offset))
	    return -1;

	offset--;
    }

    stack[offset] = NO_BIT;
    return 0;
}

/* Creating the huffman dictionary corresponding to the generated huffman tree.
 * A direction stack of size tree_height is created and the recursive function
 * huff_encoder_dictionary_gen() is called to create the dictionary.
 * Return 0 if successful, otherwise -1.
 */
static int huff_encoder_create_dictionary()
{
    bit_t *stack = NULL;
    int ret;

    if (!uncompressed_file_length || (character_set_cardinality == 1))
	return 0;

    if (!(stack = bit_stack_alloc(tree_height)))
	return -1;

    ret = huff_encoder_dictionary_gen(tree_root, stack, 0);
    bit_stack_free(stack);
    return ret;
}

/* Write the file length type and the file length into the file header.
 * Return 0 if successful, otherwise -1;
 */
static int huff_encoder_write_file_length(huff_writer_t *writer)
{
    if (uncompressed_file_length < MAX_FILE_LENGTH_REPRESENTATION_U8)
    {
	/* statistics */
	compressed_file_length += 2 * BYTE;

	return (huff_write_u8(writer, FILE_LENGTH_REPRESENTATION_U8) ||
	   huff_write_u8(writer, (u8)uncompressed_file_length));
    }
    
    if (uncompressed_file_length < MAX_FILE_LENGTH_REPRESENTATION_U16)
    {
	/* statistics */
	compressed_file_length += 3 * BYTE;

	return (huff_write_u8(writer, FILE_LENGTH_REPRESENTATION_U16) ||
	   huff_write_u16(writer, (u16)uncompressed_file_length));
    }

    if (uncompressed_file_length < MAX_FILE_LENGTH_REPRESENTATION_U32)
    {
	/* statistics */
	compressed_file_length += 5 * BYTE;

	return (huff_write_u8(writer, FILE_LENGTH_REPRESENTATION_U32) ||
	   huff_write_u32(writer, uncompressed_file_length));
    }

    return -1;
}

/* Writer the character set cardinality into the file header.
 * return 0 if successful, otherwise -1.
 */
static int huff_encoder_write_character_set_cardinality(huff_writer_t *writer)
{
    /* statistics */
    compressed_file_length += BYTE;

    return huff_write_u8(writer, character_set_cardinality);
}

/* Write character representation
 * Return 0 if successful, otherwise -1;
 */
static int huff_encoder_write_dictionary_entry(huff_writer_t *writer,
    bit_t *stack)
{
    bit_t *stack_ptr = stack;

    if (!stack)
	return -1;

    while (*stack_ptr != NO_BIT)
    {
	if (huff_write_bit(writer, *stack_ptr))
	    return -1;

	stack_ptr++;

	/* statistics */
	compressed_file_length++;
    }

    return 0;
}

/* Write a character representation into the file header.
 * Return 0 if successful, otherwise -1.
 */
static int huff_encoder_write_character_representation(huff_writer_t *writer,
    u8 character, bit_t *stack)
{
    u8 stack_len = 0;
    bit_t *stack_ptr = stack;

    while (*stack_ptr++ != NO_BIT)
	stack_len++;

    /* writing character, representation length and representation
     * representation length and representation are written only if the length
     * is greater than 0
     */
    if (huff_write_u8(writer, character) || (stack_len && 
	(huff_write_u8(writer, stack_len) ||
	huff_encoder_write_dictionary_entry(writer, stack))))
    {
	return -1;
    }

    /* statstics */
    compressed_file_length += 2 * BYTE;
    representation_length[character] = stack_len;

    return 0;
}

/* Writer the header into the compressed file.
 * Return 0 if successful, otherwise -1.
 */
static int huff_encoder_write_header(huff_writer_t *writer)
{
    int i;

    /* write uncompressed file length type and file length */
    if (huff_encoder_write_file_length(writer))
	return -1;

    /* writer character set cardinality */
    if (huff_encoder_write_character_set_cardinality(writer))
    {
	return -1;
    }

    /* if character_set_cardinality == 1 no dictionary is needed */
    if (character_set_cardinality == 1)
    {
	/* statstics */
	compressed_file_length += BYTE;

	goto Exit;
    }

    /* write character dictionary */
    for (i = 0; i < ANSI_CHAR_SET_CARDINALITY; i++)
    {
	if (dictionary[i] &&
	    huff_encoder_write_character_representation(writer, (u8)i,
	    dictionary[i]))
	{
	    return -1;
	}
    }

Exit:
    /* statistics */
    header_length = compressed_file_length;

    return 0;
}

/* Write the data into the compressed file.
 * Return 0 if successful, otherwise -1.
 */
static int huff_encoder_write_data(huff_reader_t *reader, huff_writer_t *writer)
{
    u8 ch;
    int ret;

    if (huff_reader_reset(reader))
	return -1;

    if (character_set_cardinality == 1)
    {
	/* statistics */
	coded_length = 0;

	return (huff_read_u8(reader, &ch) || huff_write_u8(writer, ch));
    }

    while (!(ret = huff_read_u8(reader, &ch)))
    {
	if (huff_encoder_write_dictionary_entry(writer, dictionary[ch]))
	    return -1;
    }

    /* statistics */
    coded_length = compressed_file_length - header_length;

    return 0;
}

/* Write the compressed file.
 * Return 0 if successful, otherwise -1.
 */
static int huff_encoder_compress(huff_reader_t *reader, huff_writer_t *writer)
{
    if (!uncompressed_file_length)
	return 0;

    return (huff_encoder_write_header(writer) ||
	huff_encoder_write_data(reader, writer));
}

/* Encode the file text_file_name. */
int huffman_encode()
{
    huff_reader_t *reader = NULL;
    huff_writer_t *writer = NULL;

    ASSERT(huff_encoder_prologue(&reader, &writer));
    ASSERT(huff_encoder_parse(reader));
    ASSERT(huff_encoder_create_tree());
    ASSERT(huff_encoder_create_dictionary());
    ASSERT(huff_encoder_compress(reader, writer));
    ASSERT(huff_encoder_epilogue(reader, writer));

    /* statistics */
    compressed_file_length =
	((compressed_file_length % BYTE) ? 1 : 0) +
	(compressed_file_length / BYTE);

    return 0;
}

