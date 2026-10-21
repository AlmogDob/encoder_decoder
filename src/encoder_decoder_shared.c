#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>


struct Byte_String {
    char *name;
    size_t capacity;
    size_t length;
    uint8_t *elements;
    size_t cursor;
    FILE *fp;
};

#define dprintERROR(fmt, ...) fprintf(stderr, "[Error] %s:%d:\n%*sIn function '%s':\n%*s" fmt "\n", __FILE__, __LINE__, 8, "", __func__, 8, "", __VA_ARGS__)
#define MALLOC malloc
#define ASSERT assert
#define FREE free
#define COUNT_CHARS_IN_BYTE 256

size_t              count_of_prefixes_needed(size_t count_of_output_alphabet, size_t count_of_payload_symbols);
size_t              count_of_possible_input_values(size_t count_of_output_alphabet, size_t count_of_reserved_prefixes, size_t count_of_payload_symbols);
void                print_mapping(size_t output_alphabet_start, size_t count_of_output_alphabet, size_t count_of_reserved_prefixes, size_t count_of_payload_symbols);
struct Byte_String  bin_file_read(char *file_name);
uint8_t             byte_string_read_byte(struct Byte_String *bs);
size_t              prefix_get_by_prefix_index(size_t index, size_t output_alphabet_start, size_t count_of_output_alphabet, size_t count_of_reserved_prefixes);

size_t count_of_prefixes_needed(size_t count_of_output_alphabet, size_t count_of_payload_symbols) 
{
    /* Compute alphabet_size^payload_length, capped at input_values. */
    size_t payload_combinations = 1;
    for (size_t i = 0; i < count_of_payload_symbols; ++i) {
        if (payload_combinations > (COUNT_CHARS_IN_BYTE - 1) / count_of_output_alphabet) {
            payload_combinations = COUNT_CHARS_IN_BYTE;
            break;
        }

        payload_combinations *= count_of_output_alphabet;
    }

    size_t missing = COUNT_CHARS_IN_BYTE - count_of_output_alphabet;
    size_t gain_per_prefix = payload_combinations - 1;

    size_t prefixes = 0;

    if (count_of_output_alphabet < COUNT_CHARS_IN_BYTE) {

        prefixes = missing / gain_per_prefix;
        prefixes += (missing % gain_per_prefix != 0);
    }

    return prefixes;
}

size_t count_of_possible_input_values(size_t count_of_output_alphabet, size_t count_of_reserved_prefixes, size_t count_of_payload_symbols)
{
    return (size_t)((count_of_output_alphabet - count_of_reserved_prefixes) + count_of_reserved_prefixes * pow((double)count_of_output_alphabet, (double)count_of_payload_symbols));
}

void print_mapping(size_t output_alphabet_start, size_t count_of_output_alphabet, size_t count_of_reserved_prefixes, size_t count_of_payload_symbols)
{
    size_t direct_count = count_of_output_alphabet - count_of_reserved_prefixes;
    size_t first_prefix = output_alphabet_start + direct_count;

    /*
    * Each prefix covers alphabet_size^payload_length values.
    * Cap at 256: larger ranges are unnecessary for printing byte mappings.
    * This also prevents overflow.
    *
    * Assumes alphabet_size >= 2 and payload_length >= 1.
    */
    size_t values_per_prefix = 1;

    for (size_t i = 0; i < count_of_payload_symbols; ++i) {
        if (values_per_prefix >
            (COUNT_CHARS_IN_BYTE - 1) / count_of_output_alphabet) {
            values_per_prefix = COUNT_CHARS_IN_BYTE;
            break;
        }

        values_per_prefix *= count_of_output_alphabet;
    }

    printf("Original byte   | Encoded bytes\n");

    if (direct_count > 0) {
        printf("%3zu-%3zu         | original + %zu\n", (size_t)0, direct_count - 1, output_alphabet_start);
    }

    size_t range_start = direct_count;

    for (size_t i = 0; i < count_of_reserved_prefixes && range_start < COUNT_CHARS_IN_BYTE; ++i) {
        size_t prefix = first_prefix + i;
        size_t remaining = COUNT_CHARS_IN_BYTE - range_start;
        size_t range_size = values_per_prefix < remaining ? values_per_prefix : remaining;
        size_t range_end = range_start + range_size - 1;

        printf("%3zu-%3zu         | %zu, followed by %zu base-%zu digits " "of (original - %zu), each + %zu\n", range_start, range_end, prefix, count_of_payload_symbols, count_of_output_alphabet, range_start, output_alphabet_start);

        range_start += range_size;
    }

    printf("Payload digits are most-significant first, padded with zero " "digits to exactly %zu symbols before adding %zu.\n", count_of_payload_symbols, output_alphabet_start);
}

struct Byte_String bin_file_read(char *file_name)
{
    struct Byte_String res = {0};
    
    FILE *fp = fopen(file_name, "rb");
    if (fp == NULL) {
        int err = errno;
        dprintERROR( "Cannot open file %s: %s", file_name, strerror(err));
        return res;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        int err = errno;
        dprintERROR( "Failed to seek to end of file %s: %s", file_name, strerror(err));
        fclose(fp);
        return res;
    }

    long size = ftell(fp);
    if (size < 0) {
        int err = errno;
        dprintERROR( "Failed to tell size of file %s: %s", file_name, strerror(err));
        fclose(fp);
        return res;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        int err = errno;
        dprintERROR( "Failed to seek to start of file %s: %s", file_name, strerror(err));
        fclose(fp);
        return res;
    }

    res.name = MALLOC(sizeof(char) * (1 + strlen(file_name)));
    if (res.name == NULL) {
        dprintERROR( "Memory allocation failed for file '%s' (%zu bytes).", file_name, strlen(file_name));
        fclose(fp);
        res.length = 0;
        return res;
    }
    strncpy(res.name, file_name, strlen(file_name)+1);

    res.length = (size_t)size;
    res.capacity = (size_t)size;
    res.elements = MALLOC(res.length);
    if (res.elements == NULL) {
        dprintERROR( "Memory allocation failed for file '%s' (%zu bytes).", file_name, res.length);
        fclose(fp);
        res.length = 0;
        FREE(res.name);
        return res;
    }

    size_t nread = fread(res.elements, 1, res.length, fp);
    if (nread != res.length) {
        if (ferror(fp)) {
            int err = errno;
            dprintERROR( "Failed to read file '%s': %s", file_name, strerror(err));
        } else {
            dprintERROR(
                "Unexpected end of file while reading '%s' "
                "(expected %zu bytes, got %zu).", file_name, res.length, nread);
        }
        FREE(res.elements);
        FREE(res.name);
        res.elements = NULL;
        res.length = 0;
        fclose(fp);
        return res;
    }

    if (fclose(fp) != 0) {
        int err = errno;
        dprintERROR("Failed to close file '%s': %s", file_name, strerror(err));
        FREE(res.elements);
        FREE(res.name);
        res.elements = NULL;
        res.length = 0;
        return res;
    }

    return res;
}

uint8_t byte_string_read_byte(struct Byte_String *bs)
{
    ASSERT(bs->cursor < bs->length);
    return bs->elements[bs->cursor++];
}

size_t prefix_get_by_prefix_index(size_t index, size_t output_alphabet_start, size_t count_of_output_alphabet, size_t count_of_reserved_prefixes)
{
    size_t direct_count = count_of_output_alphabet - count_of_reserved_prefixes;
    return output_alphabet_start + direct_count + index;
}
