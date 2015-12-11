#include "huffman.h"
#include "huffman_io.h"

static int huff_decoder_prologue(huff_reader_t **r_ptr, huff_writer_t **w_ptr)
{
    if (!(*r_ptr = huff_reader_open(compressed_file_name)) ||
	!(*w_ptr = huff_writer_open(uncompressed_file_name)))
    {
	return -1;
    }

    return 0;
}

static int huff_decoder_epilogue(huff_reader_t *reader, huff_writer_t *writer)
{
    int i;

    if (huff_reader_close(reader) || huff_writer_close(writer))
	return -1;

    if (!huffman_keep_file)
	remove(compressed_file_name);

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

static int huff_decoder_read_file_length(huff_reader_t *reader)
{
    u8 length_type;
	
    huff_read_u8(reader, &length_type);

    switch (length_type)
    {
	case(FILE_LENGTH_REPRESENTATION_U8):
	    huff_read_u8(reader, (u8 *)&uncompressed_file_length);

	    /* statistics */
	    compressed_file_length += 2 * BYTE;
	    break;
	case(FILE_LENGTH_REPRESENTATION_U16):
	    huff_read_u16(reader, (u16 *)&uncompressed_file_length);

	    /* statistics */
	    compressed_file_length += 3 * BYTE;
	    break;
	case(FILE_LENGTH_REPRESENTATION_U32):
	    huff_read_u32(reader, &uncompressed_file_length);

	    /* statistics */
	    compressed_file_length += 5 * BYTE;
	    break;
	default:
	    return -1;
    }
    
    return 0;
}

static int huff_decoder_read_char_set_cardinality(huff_reader_t *reader)
{
    /* statistics */
    compressed_file_length += BYTE;

    return huff_read_u8(reader, &character_set_cardinality);
}

static int huff_decoder_create_dictionary_entry(huff_reader_t *reader)
{
    u8 character, rep_length;
    bit_t *stack = NULL;
    int i;

    if (huff_read_u8(reader, &character) || huff_read_u8(reader, &rep_length) ||
	!(stack = bit_stack_alloc((int)rep_length + 1)))
    {
	return -1;
    }

    for (i = 0; i < rep_length; i++)
    {
	if (huff_read_bit(reader, stack + i))
	    return -1;
    }
    stack[i] = NO_BIT;
    dictionary[character] = stack;

    /* statisics */
    compressed_file_length += 2 * BYTE + rep_length;
    representation_length[character] = rep_length;

    return 0;
}

static int huff_decoder_parse_header(huff_reader_t *reader,
    huff_writer_t *writer)
{
    int i;

    /* read the uncompressed file length */
    if (huff_decoder_read_file_length(reader))
    {
	printf("it is not possible for a huffman file to be of zero length\n");
	return -1;
    }
    /* read the uncompressed file character set cardinality */
    if (huff_decoder_read_char_set_cardinality(reader))
	return -1;

    /* no dictionary is created for a huffman file with character set 
     * cardinality == 1 */
    if (character_set_cardinality == 1)
    {
	/* statistics */
	header_length = compressed_file_length + BYTE;

	return 0;
    }

    /* creating a huffman code dictionary */
    for (i = 0; i < character_set_cardinality; i++)
    {
	if (huff_decoder_create_dictionary_entry(reader))
	    return -1;
    }

    /* statisics */
    header_length = compressed_file_length;

    return 0;
}

/* inserting a character representation path into the huffman tree */
static int huff_decoder_insert_node(u8 character, bit_t *stack)
{
    huff_tree_node_t **node_ptr = &tree_root;
    bit_t *stack_ptr = stack;

    while (*stack_ptr != NO_BIT)
    {
	/* ZERO is associated with HUFF_NODE_LSON and
	 * ONE is associated with HUFF_NODE_RSON
	 */
	node_ptr = (*stack_ptr == ZERO) ? &HUFF_NODE_LSON(*node_ptr) :
	    &HUFF_NODE_RSON(*node_ptr);

	if (!*node_ptr && !(*node_ptr = huff_tree_node_alloc(HUFFMAN_EOF, 0)))
	    return -1;

	stack_ptr++;
    }

    (*node_ptr)->character = character;
    return 0;
}

/* creating a huffman tree based on the dictionary in the header */
static int huff_decoder_creat_tree(void)
{
    u8 ch;
    
    if (character_set_cardinality == 1)
	return 0;

    if (!(tree_root = huff_tree_node_alloc(HUFFMAN_EOF, 0)))
	return -1;

    for (ch = 0; ch < ANSI_CHAR_SET_CARDINALITY; ch++)
    {
	if (dictionary[ch] && huff_decoder_insert_node(ch, dictionary[ch]))
	{
	    return -1;
	}
    }

    return 0;
}

/* decoding the huffman file */
static int huff_decoder_decompress(huff_reader_t *reader, huff_writer_t *writer)
{
    u32 file_size;
    huff_tree_node_t *node = NULL;
    bit_t bit;
    u8 character;

    switch (character_set_cardinality)
    {
    case 1:
	if (huff_read_u8(reader, &character))
	    return -1;

	for (file_size = 0; file_size < uncompressed_file_length; file_size++)
	{
	    if (huff_write_u8(writer, character))
		return -1;
	}

	/* statisics */
	compressed_file_length += BYTE;
	frequency[character] = uncompressed_file_length;
	break;
    default:
	for (file_size = 0; file_size < uncompressed_file_length; file_size++)
	{
	    node = tree_root;
	    do
	    {
		if (huff_read_bit(reader, &bit))
		    return -1;

		node = (bit == ZERO) ? HUFF_NODE_LSON(node) :
		    HUFF_NODE_RSON(node);

		/* statistics */
		compressed_file_length++;
	    }
	    while (!HUFF_NODE_ISLEAF(node));

	    if (huff_write_u8(writer, node->character))
		return -1;

	    /* statistics */
	    frequency[node->character]++;
	}
	break;
    }

    /* statistics */
    coded_length = compressed_file_length - header_length;

    return 0;
}

/* Encode the file text_file_name. */
/* Decode the file text_file_name.huf */
int huffman_decode(void)
{
    huff_reader_t *reader = NULL;
    huff_writer_t *writer = NULL;

    ASSERT(huff_decoder_prologue(&reader, &writer));
    ASSERT(huff_decoder_parse_header(reader, writer));
    ASSERT(huff_decoder_creat_tree());
    ASSERT(huff_decoder_decompress(reader, writer));
    ASSERT(huff_decoder_epilogue(reader, writer));

    /* statistics */
    compressed_file_length =
	((compressed_file_length % BYTE) ? 1 : 0) +
	(compressed_file_length / BYTE);

    return 0;
}

