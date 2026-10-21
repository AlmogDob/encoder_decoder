#include "encoder_decoder_shared.c"


size_t encode_byte_to_buffer(uint8_t *buffer, uint8_t the_byte, size_t output_alphabet_start, size_t count_of_output_alphabet, size_t count_of_reserved_prefixes, size_t count_of_payload_symbols)
{
    size_t direct_count = count_of_output_alphabet - count_of_reserved_prefixes;

    /* Direct bytes use exactly one output symbol. */
    if ((size_t)the_byte < direct_count) {
        buffer[0] = (uint8_t)(output_alphabet_start + the_byte);
        return 1;
    }

    /* Compute alphabet_size^payload_length, capped at 256. */
    size_t values_per_prefix = 1;
    for (size_t i = 0; i < count_of_payload_symbols; ++i) {
        if (values_per_prefix > (COUNT_CHARS_IN_BYTE - 1) / count_of_output_alphabet) {
            values_per_prefix = COUNT_CHARS_IN_BYTE;
            break;
        }
        values_per_prefix *= count_of_output_alphabet;
    }
    size_t offset = (size_t)the_byte - direct_count;
    size_t prefix_index = offset / values_per_prefix;
    size_t payload_value = offset % values_per_prefix;

    buffer[0] = (uint8_t)prefix_get_by_prefix_index(prefix_index, output_alphabet_start, count_of_output_alphabet, count_of_reserved_prefixes);
    /*
     * Fill backward to produce most-significant-first digits.
     * Continue through all positions to include leading zero digits.
     */
    for (size_t i = count_of_payload_symbols; i > 0; --i) {
        buffer[i] = (uint8_t)(output_alphabet_start + payload_value % count_of_output_alphabet);
        payload_value /= count_of_output_alphabet;
    }

    return count_of_payload_symbols + 1;
}

#define MAX_LEN 1500

int main(int argc, char const *argv[])
{
    const char *output_file_name = NULL;
    const char *input_file_name = NULL;
    size_t output_alphabet_start = 0;
    size_t output_alphabet_end = 0;
    size_t count_of_payload_symbols = 0;

    // for (size_t i = 0; i < argc; i++) {
    //     printf("%zu: %s\n", i, argv[i]);
    // } 
    if (argc != 6) {
        dprintERROR("Usage: %s input_file output_file start_character_index end_character_index payload_length", argv[0]);
        return -1;
    }
    input_file_name = argv[1];
    output_file_name = argv[2];
    sscanf(argv[3], "%zu", &output_alphabet_start);
    sscanf(argv[4], "%zu", &output_alphabet_end);
    sscanf(argv[5], "%zu", &count_of_payload_symbols);
    if (input_file_name == NULL || output_file_name == NULL || output_alphabet_start == 0 || output_alphabet_end == 0 || count_of_payload_symbols == 0) {
        dprintERROR("Usage: %s input_file output_file start_character_index end_character_index payload_length", argv[0]);
        return -1;
    }

    size_t count_of_output_alphabet  = output_alphabet_end - output_alphabet_start + 1;
    size_t count_of_reserved_prefixes = count_of_prefixes_needed(count_of_output_alphabet, count_of_payload_symbols);

    if (count_of_reserved_prefixes > count_of_output_alphabet) {
        printf("Not enough symbols to encode a byte stream. Increase payload length.\n");
        return -1;
    }

    // size_t count_of_input_values = count_of_possible_input_values(count_of_output_alphabet, count_of_reserved_prefixes, count_of_payload_symbols);
    // printf("Needs %zu prefixes and can represent %zu values.\n", count_of_reserved_prefixes, count_of_input_values);
    // print_mapping(output_alphabet_start, count_of_output_alphabet, count_of_reserved_prefixes, count_of_payload_symbols);

    struct Byte_String bs = bin_file_read((char *)input_file_name);
    if (bs.elements == NULL) {
        return -1;
    }
    FILE *output_fp = fopen(output_file_name, "wb");
    if (output_fp == NULL) {
        int err = errno;
        dprintERROR("Cannot open file %s: %s", output_file_name, strerror(err));
        return -1;
    }
    fputc((int)output_alphabet_start, output_fp);
    fputc((int)output_alphabet_end, output_fp);
    fputc((int)count_of_payload_symbols + '0', output_fp);

    uint8_t buffer[COUNT_CHARS_IN_BYTE];

    for (size_t i = 0; i < bs.length; i++) {
        uint8_t the_byte = byte_string_read_byte(&bs);
        size_t count = encode_byte_to_buffer(buffer, the_byte, output_alphabet_start, count_of_output_alphabet, count_of_reserved_prefixes, count_of_payload_symbols);
        fwrite(buffer, 1, count, output_fp);
    }

    printf("Encoded binary data from file '%s' to file '%s'.\n", input_file_name, output_file_name);

    return 0;
}
