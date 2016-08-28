#ifndef HASHIDS_H
#define HASHIDS_H 1

/* version constants */
#define HASHIDS_VERSION "1.0.0"
#define HASHIDS_VERSION_MAJOR 1
#define HASHIDS_VERSION_MINOR 0
#define HASHIDS_VERSION_PATCH 0

/* minimal alphabet length */
#define HASHIDS_MIN_ALPHABET_LENGTH 16u

/* separator divisor */
#define HASHIDS_SEPARATOR_DIVISOR 3.5f

/* guard divisor */
#define HASHIDS_GUARD_DIVISOR 12u

/* default salt */
#define HASHIDS_DEFAULT_SALT L""

/* default minimal hash length */
#define HASHIDS_DEFAULT_MIN_HASH_LENGTH 0u

/* default alphabet */
#define HASHIDS_DEFAULT_ALPHABET L"abcdefghijklmnopqrstuvwxyz" \
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
                                 "1234567890"

/* default separators */
#define HASHIDS_DEFAULT_SEPARATORS L"cfhistuCFHISTU"

/* minimal buffer size */
#define HASHIDS_MIN_BUFFER_SIZE 64u

/* common error codes */
#define HASHIDS_ERROR_OK 0
#define HASHIDS_ERROR_ALLOC -1
#define HASHIDS_ERROR_ALPHABET_LENGTH -2
#define HASHIDS_ERROR_ALPHABET_SPACE -3
#define HASHIDS_ERROR_INVALID_HASH -4
#define HASHIDS_ERROR_INVALID_NUMBER -5

/* exported hashids_errno */
extern int hashids_errno;

/* alloc / free */
extern void *(*_hashids_alloc)(size_t size);
extern void (*_hashids_free)(void *ptr);

/* the hashids "object" */
struct hashids_t {
    wchar_t *alphabet;
    wchar_t *alphabet_copy_1;
    wchar_t *alphabet_copy_2;
    unsigned int alphabet_length;

    wchar_t *salt;
    unsigned int salt_length;

    wchar_t *separators;
    unsigned int separators_count;

    wchar_t *guards;
    unsigned int guards_count;

    unsigned int min_hash_length;
};

/* exported function definitions */
void
hashids_shuffle(wchar_t *str, size_t str_length, const wchar_t *salt, size_t salt_length);

void
hashids_free(struct hashids_t *hashids);

struct hashids_t *
hashids_init3(const wchar_t *salt, unsigned int min_hash_length,
    const wchar_t *alphabet);

struct hashids_t *
hashids_init2(const wchar_t *salt, unsigned int min_hash_length);

struct hashids_t *
hashids_init(const wchar_t *salt);

unsigned int
hashids_estimate_encoded_size(struct hashids_t *hashids,
    const unsigned int numbers_count, const unsigned long long *numbers);

unsigned int
hashids_estimate_encoded_size_v(struct hashids_t *hashids,
    const unsigned int numbers_count, ...);

unsigned int
hashids_encode(struct hashids_t *hashids, wchar_t *buffer,
    const unsigned int numbers_count, const unsigned long long *numbers);

unsigned int
hashids_encode_v(struct hashids_t *hashids, wchar_t *buffer,
    unsigned int numbers_count, ...);

unsigned int
hashids_encode_one(struct hashids_t *hashids, wchar_t *buffer,
    const unsigned long long number);

unsigned int
hashids_numbers_count(struct hashids_t *hashids, wchar_t *str);

unsigned int
hashids_decode(struct hashids_t *hashids, wchar_t *str,
    unsigned long long *numbers);

unsigned int
hashids_encode_hex(struct hashids_t *hashids, wchar_t *buffer,
    const wchar_t *hex_str);

unsigned int
hashids_decode_hex(struct hashids_t *hashids, wchar_t *str, wchar_t *output);

#endif
