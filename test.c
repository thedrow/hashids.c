#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "hashids.h"

#ifndef lengthof
#define lengthof(x) ((unsigned int)(sizeof(x) / sizeof(x[0])))
#endif

struct testcase_t {
    const wchar_t *salt;
    unsigned int min_hash_length;
    const wchar_t *alphabet;
    unsigned int numbers_count;
    unsigned long long numbers[16];
    const wchar_t *expected_hash;
};

struct testcase_t testcases[] = {
    {L"", 0, HASHIDS_DEFAULT_ALPHABET, 1, {1ull}, L"jR"},
    {L"", 0, HASHIDS_DEFAULT_ALPHABET, 1, {12345ull}, L"j0gW"},
    {L"", 0, HASHIDS_DEFAULT_ALPHABET, 1, {18446744073709551615ull}, L"AOo9Ql5nQR1VO"},

    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {1ull}, L"NV"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {22ull}, L"K4"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {333ull}, L"OqM"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {9999ull}, L"kQVg"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {9999ull}, L"kQVg"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {12345ull}, L"NkK9"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {123000ull}, L"58LzD"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {456000000ull}, L"5gn6mQP"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {987654321ull}, L"oyjYvry"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {666555444333222ull}, L"KVO9yy1oO5j"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 1, {18446744073709551615ull}, L"zXVjmzBamYlqX"},

    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 5, {1ull, 2ull, 3ull, 4ull, 5ull}, L"zoHWuNhktp"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 3, {1ull,2ull,3ull}, L"laHquq"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 3, {2ull,4ull,6ull}, L"44uotN"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 2, {99ull,25ull}, L"97Jun"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 3, {1337ull,42ull,314ull}, L"7xKhrUxm"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 4, {683ull,94108ull,123ull,5ull}, L"aBMswoO2UB3Sj"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 7, {547ull,31ull,241271ull,311ull,31397ull,1129ull,71129ull}, L"3RoSDhelEyhxRsyWpCx5t1ZK"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 5, {21979508ull,35563591ull,57543099ull,93106690ull,150649789ull}, L"p2xkL3CK33JjcrrZ8vsw4YRZueZX9k"},

    {L"this is my salt", 18, HASHIDS_DEFAULT_ALPHABET, 1, {1}, L"aJEDngB0NV05ev1WwP"},
    {L"this is my salt", 18, HASHIDS_DEFAULT_ALPHABET, 6, {4140ull,21147ull,115975ull,678570ull,4213597ull,27644437ull}, L"pLMlCWnJSXr1BSpKgqUwbJ7oimr7l6"},

    {L"this is my salt", 0, L"ABCDEFGhijklmn34567890-", 5, {1ull,2ull,3ull,4ull,5ull}, L"D4h3F7i5Al"},

    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 4, {5ull,5ull,5ull,5ull}, L"1Wc8cwcE"},
    {L"this is my salt", 0, HASHIDS_DEFAULT_ALPHABET, 10, {1ull,2ull,3ull,4ull,5ull,6ull,7ull,8ull,9ull,10ull}, L"kRHnurhptKcjIDTWC3sx"},

    {L"this is my salt", 0, L"cfhistuCFHISTU+-", 1, {1337ull}, L"+-+-++---++-"},

    {NULL, 0, NULL, 0, {0ull}, NULL}
};

char * failures[lengthof(testcases)];
wchar_t *w_failures[lengthof(testcases)];

char *
f(const char *fmt, ...)
{
    char *result = calloc(512, sizeof(char));
    va_list ap;

    if (!result) {
        printf("Fatal error: Cannot allocate memory for error description\n");
        exit(EXIT_FAILURE);
    }

    va_start(ap, fmt);
    vsprintf(result, fmt, ap);
    va_end(ap);

    return result;
}

wchar_t *
fw(const wchar_t *fmt, ...)
{
    wchar_t *result = calloc(512, sizeof(wchar_t));
    va_list ap;

    if (!result) {
        printf("Fatal error: Cannot allocate memory for error description\n");
        exit(EXIT_FAILURE);
    }

    va_start(ap, fmt);
    vswprintf(result, 512, fmt, ap);
    va_end(ap);

    return result;
}

int
main(int argc, char **argv)
{
    struct hashids_t *hashids;
    wchar_t buffer[1024];
    char *error = 0;
    unsigned int i, j, k, result;
    unsigned long long numbers[16];
    struct testcase_t testcase;

    /* walk test cases */
    for (i = 0, j = 0, k = 0;; ++i) {
        bool success = true;

        if (i && i % 72 == 0) {
            printf("\n");
        }

        testcase = testcases[i];

        /* bail out */
        if (!testcase.salt || !testcase.alphabet || !testcase.expected_hash) {
            break;
        }

        /* initialize hashids */
        hashids = hashids_init3(testcase.salt, testcase.min_hash_length,
            testcase.alphabet);

        /* error upon initialization */
        if (!hashids) {
            printf("F");

            switch (hashids_errno) {
                case HASHIDS_ERROR_ALLOC:
                    error = "Hashids: Allocation failed";
                    break;
                case HASHIDS_ERROR_ALPHABET_LENGTH:
                    error = "Hashids: Alphabet is too short";
                    break;
                case HASHIDS_ERROR_ALPHABET_SPACE:
                    error = "Hashids: Alphabet contains whitespace characters";
                    break;
                default:
                    error = "Hashids: Unknown error";
                    break;
            }

            failures[j++] = f("#%d: %s", i + 1, error);
            continue;
        }

        /* encode */
        result = hashids_encode(hashids, buffer, testcase.numbers_count,
            testcase.numbers);

        if (wcscmp(buffer, testcase.expected_hash) != 0) {
            printf("F");
            w_failures[k++] = fw(L"hashids_encode() buffer %s does not match expected hash %s", buffer,
                              testcase.expected_hash);
            success = false;
        }

        /* encoding error */
        if (!result) {
            printf("F");
            failures[j++] = f("#%d: hashids_encode() returned 0", i + 1);
            success = false;
        }

        /* decode */
        result = hashids_decode(hashids, buffer, numbers);

        /* decoding error */
        if (result != testcase.numbers_count) {
            printf("F");
            failures[j++] = f("hashids_decode() returned %u", i + 1, result);
            success = false;
        }

        /* compare */
        if (memcmp(numbers, testcase.numbers,
                result * sizeof(unsigned long long))) {
            printf("F");
            failures[j++] = f("#%d: hashids_decode() decoding error", i + 1);
            success = false;
        }

        if (success) {
            printf(".");
        }
        hashids_free(hashids);
    }

    printf("\n\n");

    for (i = 0; i < j; ++i) {
        printf("%s\n", failures[i]);
        free(failures[i]);
    }

    for (i = 0; i < k; ++i) {
        wprintf(L"%s\n", w_failures[k]);
        free(w_failures[k]);
    }

    if (j || k) {
        printf("\n");
    }

    printf("%u samples, %u failures\n", lengthof(testcases) - 1, j + k);

    return j ? EXIT_FAILURE : EXIT_SUCCESS;
}
