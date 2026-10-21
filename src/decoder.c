#include "encoder_decoder_shared.c"

int main(int argc, char const *argv[])
{
    const char *output_file_name = NULL;
    const char *input_file_name = NULL;

    if (argc != 3) {
        dprintERROR("Usage: %s input_file output_file", argv[0]);
        return -1;
    }
    input_file_name = argv[1];
    output_file_name = argv[2];
    if (input_file_name == NULL || output_file_name == NULL) {
        dprintERROR("Usage: %s input_file output_file", argv[0]);
        return -1;
    }

    FILE *input_fp = NULL;
    FILE *output_fp = NULL;
    int result = EXIT_FAILURE;

    input_fp = fopen(input_file_name, "rb");
    if (input_fp == NULL) {
        int err = errno;
        dprintERROR("Cannot open file %s: %s", input_file_name, strerror(err));
        goto cleanup;
    }

    size_t output_alphabet_start = fgetc(input_fp);
    size_t output_alphabet_end = fgetc(input_fp);
    size_t count_of_payload_symbols = fgetc(input_fp) - '0';

    if (output_alphabet_start > output_alphabet_end || output_alphabet_end > UINT8_MAX || count_of_payload_symbols == 0) {
        dprintERROR("%s", "Invalid alphabet or payload length.\n");
        return EXIT_FAILURE;
    }

    size_t count_of_output_alphabet = output_alphabet_end - output_alphabet_start + 1;

    if (count_of_output_alphabet < 2) {
        dprintERROR("%s", "The alphabet must contain at least two symbols.\n");
        return EXIT_FAILURE;
    }

    size_t count_of_reserved_prefixes = count_of_prefixes_needed( count_of_output_alphabet, count_of_payload_symbols);

    if (count_of_reserved_prefixes > count_of_output_alphabet) {
        dprintERROR("%s", "Not enough symbols to decode a byte stream. Increase payload length.\n");
        return EXIT_FAILURE;
    }

    size_t direct_count = count_of_output_alphabet - count_of_reserved_prefixes;

    /* Match the encoder's capped alphabet_size^payload_length. */
    size_t values_per_prefix = 1;
    for (size_t i = 0; i < count_of_payload_symbols; ++i) {
        if (values_per_prefix > (COUNT_CHARS_IN_BYTE - 1) / count_of_output_alphabet) {
            values_per_prefix = COUNT_CHARS_IN_BYTE;
            break;
        }
        values_per_prefix *= count_of_output_alphabet;
    }

    output_fp = fopen(output_file_name, "wb");
    if (output_fp == NULL) {
        int err = errno;
        dprintERROR("Cannot open file %s: %s", output_file_name, strerror(err));
        goto cleanup;
    }

    int ch;
    while ((ch = fgetc(input_fp)) != EOF) {
        if ((size_t)ch < output_alphabet_start || (size_t)ch > output_alphabet_end) {
            dprintERROR("%s", "Encoded symbol is outside the alphabet.\n");
            goto cleanup;
        }

        size_t symbol = (size_t)ch - output_alphabet_start;
        size_t decoded_value;

        if (symbol < direct_count) {
            decoded_value = symbol;
        } else {
            size_t prefix_index = symbol - direct_count;
            size_t payload_value = 0;

            for (size_t i = 0; i < count_of_payload_symbols; ++i) {
                ch = fgetc(input_fp);
                if (ch == EOF) {
                    if (ferror(input_fp)) {
                        dprintERROR("%s", "Failed to read encoded input.\n");
                    } else {
                        dprintERROR("%s", "Incomplete payload after prefix.\n");
                    }
                    goto cleanup;
                }

                if ((size_t)ch < output_alphabet_start || (size_t)ch > output_alphabet_end) {
                    dprintERROR("%s", "Payload symbol is outside the alphabet.\n");
                    goto cleanup;
                }

                size_t digit = (size_t)ch - output_alphabet_start;

                /*
                 * Reject payloads above 255 before multiplication,
                 * avoiding overflow even for long payloads.
                 */
                if (payload_value > (UINT8_MAX - digit) / count_of_output_alphabet) {
                    dprintERROR("%s", "Payload exceeds the byte range.\n");
                    goto cleanup;
                }

                payload_value = payload_value * count_of_output_alphabet + digit;
            }

            decoded_value = direct_count + prefix_index * values_per_prefix + payload_value;

            /* Some prefix/payload combinations are unused. */
            if (decoded_value > UINT8_MAX) {
                dprintERROR("%s", "Encoded sequence exceeds the byte range.\n");
                goto cleanup;
            }
        }

        if (fputc((int)decoded_value, output_fp) == EOF) {
            dprintERROR("%s", "Failed to write decoded output.\n");
            goto cleanup;
        }
    }

    if (ferror(input_fp)) {
        dprintERROR("%s", "Failed to read encoded input.\n");
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
    if (input_fp != NULL && fclose(input_fp) != 0) {
        dprintERROR("%s", "Failed to close encoded input.\n");
        result = EXIT_FAILURE;
    }

    if (output_fp != NULL && fclose(output_fp) != 0) {
        dprintERROR("%s", "Failed to close decoded output.\n");
        result = EXIT_FAILURE;
    }

    printf("%s decode binary data from file '%s' to file '%s'.\n", result == EXIT_FAILURE ? "Failed to" : "Succeeded in", input_file_name, output_file_name);

    return result;
}