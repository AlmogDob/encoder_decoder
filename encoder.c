#include <stdio.h>
#include <limits.h>
#include <math.h>

#define COUNT_CHARS_IN_BYTE 256

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

    const size_t missing = COUNT_CHARS_IN_BYTE - count_of_output_alphabet;
    const size_t gain_per_prefix = payload_combinations - 1;

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
    const size_t direct_count = count_of_output_alphabet - count_of_reserved_prefixes;
    const size_t first_prefix = output_alphabet_start + direct_count;

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
        const size_t prefix = first_prefix + i;
        const size_t remaining = COUNT_CHARS_IN_BYTE - range_start;
        const size_t range_size = values_per_prefix < remaining ? values_per_prefix : remaining;
        const size_t range_end = range_start + range_size - 1;

        printf("%3zu-%3zu         | %zu, followed by %zu base-%zu digits " "of (original - %zu), each + %zu\n", range_start, range_end, prefix, count_of_payload_symbols, count_of_output_alphabet, range_start, output_alphabet_start);

        range_start += range_size;
    }

    printf("Payload digits are most-significant first, padded with zero " "digits to exactly %zu symbols before adding %zu.\n", count_of_payload_symbols, output_alphabet_start);
}

int main(int argc, char const *argv[])
{
    // size_t n = 242;
    // size_t base = 12;

    // /* Enough space even for base 2. Assumes base >= 2. */
    // size_t digits[sizeof(size_t) * CHAR_BIT];
    // size_t count = 0;
    // do {
    //     digits[count++] = n % base;
    //     n /= base;
    // } while (n != 0);
    // printf("Most to least significant: ");
    // while (count > 0) {
    //     printf("%zu ", digits[--count]);
    // }
    // printf("\n");
    // return 0;

    size_t output_alphabet_start = 33;
    size_t output_alphabet_end = 126;
    size_t count_of_output_alphabet  = output_alphabet_end - output_alphabet_start + 1;
    size_t count_of_payload_symbols  = 1;
    size_t count_of_reserved_prefixes = count_of_prefixes_needed(count_of_output_alphabet, count_of_payload_symbols);

    if (count_of_reserved_prefixes > count_of_output_alphabet) {
        printf("Not enough symbols to encode a byte stream. Increase payload length.\n");
        return -1;
    }

    size_t count_of_input_values = count_of_possible_input_values(count_of_output_alphabet, count_of_reserved_prefixes, count_of_payload_symbols);

    printf("Needs %zu prefixes and can represent %zu values.\n", count_of_reserved_prefixes, count_of_input_values);
    
    print_mapping(output_alphabet_start, count_of_output_alphabet, count_of_reserved_prefixes, count_of_payload_symbols);


    return 0;
}
