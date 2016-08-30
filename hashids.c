#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "hashids.h"

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

/* exported hashids_errno */
int hashids_errno;

/* alloc/free */
static void *
hashids_alloc_f(size_t size)
{
    return calloc(size, sizeof(wchar_t));
}

static void
hashids_free_f(void *ptr)
{
    free(ptr);
}

void *(*_hashids_alloc)(size_t size) = hashids_alloc_f;
void (*_hashids_free)(void *ptr) = hashids_free_f;

/* shuffle */
void
hashids_shuffle(wchar_t *str, size_t str_length, const wchar_t *salt, size_t salt_length)
{
    size_t i, v, p;

    if (!salt_length) {
        return;
    }

    for (i = str_length - 1, v = 0, p = 0; i > 0; --i, ++v) {
        v %= salt_length;
        p += salt[v];
        size_t j = (salt[v] + v + p) % i;

        wchar_t temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
}

/* "destructor" */
void
hashids_free(struct hashids_t *hashids)
{
    if (likely(hashids)) {
        if (likely(hashids->alphabet)) {
            _hashids_free(hashids->alphabet);
        }
        if (likely(hashids->alphabet_copy_1)) {
            _hashids_free(hashids->alphabet_copy_1);
        }
        if (likely(hashids->alphabet_copy_2)) {
            _hashids_free(hashids->alphabet_copy_2);
        }
        if (likely(hashids->salt)) {
            _hashids_free(hashids->salt);
        }
        if (likely(hashids->separators)) {
            _hashids_free(hashids->separators);
        }
        if (likely(hashids->guards)) {
            _hashids_free(hashids->guards);
        }

        _hashids_free(hashids);
    }
}

/* common init */
struct hashids_t *
hashids_init3(const wchar_t *salt, unsigned int min_hash_length,
    const wchar_t *alphabet)
{
    struct hashids_t *result;
    unsigned int i, j;
    wchar_t ch;

    hashids_errno = HASHIDS_ERROR_OK;

    /* allocate the structure */
    result = malloc(sizeof(struct hashids_t));
    if (unlikely(!result)) {
        hashids_errno = HASHIDS_ERROR_ALLOC;
        return NULL;
    }

    /* allocate enough space for the alphabet and its copies */
    size_t estimated_alphabet_length = wcslen(alphabet) + 1;
    result->alphabet = _hashids_alloc(estimated_alphabet_length);
    result->alphabet_copy_1 = _hashids_alloc(estimated_alphabet_length);
    result->alphabet_copy_2 = _hashids_alloc(estimated_alphabet_length);
    if (unlikely(!result->alphabet || !result->alphabet_copy_1
        || !result->alphabet_copy_2)) {
        hashids_free(result);
        hashids_errno = HASHIDS_ERROR_ALLOC;
        return NULL;
    }

    /* extract only the unique characters */
    result->alphabet[0] = '\0';
    for (i = 0, j = 0; i < estimated_alphabet_length; ++i) {
        ch = alphabet[i];
        if (!wcschr(result->alphabet, ch)) {
            result->alphabet[j++] = ch;
        }
    }
    result->alphabet[j] = '\0';

    /* store alphabet length */
    result->alphabet_length = j;

    /* check length and whitespace */
    if (result->alphabet_length < HASHIDS_MIN_ALPHABET_LENGTH) {
        hashids_free(result);
        hashids_errno = HASHIDS_ERROR_ALPHABET_LENGTH;
        return NULL;
    }
    if (wcschr(result->alphabet, ' ')) {
        hashids_free(result);
        hashids_errno = HASHIDS_ERROR_ALPHABET_SPACE;
        return NULL;
    }

    /* copy salt */
    result->salt = wcsdup(salt ? salt : HASHIDS_DEFAULT_SALT);
    result->salt_length = (unsigned int) wcslen(result->salt);

    /* allocate enough space for separators */
    result->separators = _hashids_alloc((size_t) (ceil((float)result->alphabet_length / HASHIDS_SEPARATOR_DIVISOR) + 1) * sizeof(wchar_t));
    if (unlikely(!result->separators)) {
        hashids_free(result);
        hashids_errno = HASHIDS_ERROR_ALLOC;
        return NULL;
    }

    /* non-alphabet characters cannot be separators */
    for (i = 0, j = 0; i < wcslen(HASHIDS_DEFAULT_SEPARATORS); ++i) {
        wchar_t *p;
        ch = HASHIDS_DEFAULT_SEPARATORS[i];
        if ((p = wcschr(result->alphabet, ch))) {
            result->separators[j++] = ch;

            /* also remove separators from alphabet */
            wmemmove(p, p + 1,
                wcslen(result->alphabet) - (p - result->alphabet));
        }
    }

    /* store separators length */
    result->separators_count = j;

    /* subtract separators count from alphabet length */
    result->alphabet_length -= result->separators_count;

    /* shuffle the separators */
    hashids_shuffle(result->separators, result->separators_count,
        result->salt, result->salt_length);

    /* check if we have any/enough separators */
    if (!result->separators_count
        || (((float)result->alphabet_length / (float)result->separators_count)
                > HASHIDS_SEPARATOR_DIVISOR)) {
        unsigned int separators_count = (unsigned int)ceil(
            (float)result->alphabet_length / HASHIDS_SEPARATOR_DIVISOR);

        if (separators_count == 1) {
            separators_count = 2;
        }

        if (separators_count > result->separators_count) {
            /* we need more separators - get some from alphabet */
            int diff = separators_count - result->separators_count;
            wcsncat(result->separators, result->alphabet, diff);
            wmemmove(result->alphabet, result->alphabet + diff,
                result->alphabet_length - diff + 1);

            result->separators_count += diff;
            result->alphabet_length -= diff;
        } else {
            /* we have more than enough - truncate */
            result->separators[separators_count] = '\0';
            result->separators_count = separators_count;
        }
    }

    /* shuffle alphabet */
    hashids_shuffle(result->alphabet, result->alphabet_length,
        result->salt, result->salt_length);

    /* allocate guards */
    result->guards_count = (unsigned int) ceil((float)result->alphabet_length
                                               / HASHIDS_GUARD_DIVISOR);
    result->guards = _hashids_alloc(result->guards_count + 1);
    if (unlikely(!result->guards)) {
        hashids_free(result);
        hashids_errno = HASHIDS_ERROR_ALLOC;
        return NULL;
    }

    if (result->alphabet_length < 3) {
        /* take some from separators */
        wcsncpy(result->guards, result->separators, result->guards_count);
        wmemmove(result->separators, result->separators + result->guards_count,
            result->separators_count - result->guards_count + 1);

        result->separators_count -= result->guards_count;
    } else {
        /* take them from alphabet */
        wcsncpy(result->guards, result->alphabet, result->guards_count);
        wmemmove(result->alphabet, result->alphabet + result->guards_count,
            result->alphabet_length - result->guards_count + 1);

        result->alphabet_length -= result->guards_count;
    }

    /* set min hash length */
    result->min_hash_length = min_hash_length;

    /* return result happily */
    return result;
}

/* init with salt and minimum hash length */
struct hashids_t *
hashids_init2(const wchar_t *salt, unsigned int min_hash_length)
{
    return hashids_init3(salt, min_hash_length, HASHIDS_DEFAULT_ALPHABET);
}

/* init with hash only */
struct hashids_t *
hashids_init(const wchar_t *salt)
{
    return hashids_init2(salt, HASHIDS_DEFAULT_MIN_HASH_LENGTH);
}

/* estimate buffer size (generic) */
unsigned long
hashids_estimate_encoded_size(struct hashids_t *hashids,
    const unsigned long numbers_count, const unsigned long long *numbers)
{
    unsigned int result_len, i;

    /* start from 1 - the lottery character */
    result_len = 1;

    for (i = 0; i < numbers_count; ++i) {
        unsigned long long number = numbers[i];

        /* how long is the hash */
        do {
            ++result_len;
            number = (unsigned long long) floorl((long double)number / (long double)hashids->alphabet_length);
        } while (number);

        /* more than 1 number - separator */
        if (i + 1 < numbers_count) {
            ++result_len;
        }
    }

    /* minimum length checks */
    if (result_len++ < hashids->min_hash_length) {
        if (result_len++ < hashids->min_hash_length) {
            while (result_len < hashids->min_hash_length) {
                result_len += hashids->alphabet_length;
            }
        }
    }

    return result_len + 1;
}

/* estimate buffer size (variadic) */
unsigned long
hashids_estimate_encoded_size_v(struct hashids_t *hashids,
    const unsigned long numbers_count, ...)
{
    int i;
    unsigned int result;
    unsigned long long *numbers;
    va_list ap;

    numbers = calloc(numbers_count, sizeof(unsigned long long));

    if (unlikely(!numbers)) {
        hashids_errno = HASHIDS_ERROR_ALLOC;
        return 0;
    }

    va_start(ap, numbers_count);
    for (i = 0; i < numbers_count; ++i) {
        numbers[i] = va_arg(ap, unsigned long long);
    }
    va_end(ap);

    result = hashids_estimate_encoded_size(hashids, numbers_count, numbers);
    _hashids_free(numbers);

    return result;
}

/* encode many (generic) */
unsigned long
hashids_encode(struct hashids_t *hashids, wchar_t *buffer,
    const unsigned long numbers_count, const unsigned long long *numbers)
{
    /* bail out if no numbers */
    if (unlikely(!numbers_count)) {
        buffer[0] = '\0';

        return 0;
    }

    unsigned long i, j, result_len;
    unsigned long long number, numbers_hash;
    long p_max;
    wchar_t lottery, ch, temp_ch, *p, *buffer_end;

    /* return an estimation if no buffer */
    if (unlikely(!buffer)) {
        return hashids_estimate_encoded_size(hashids, numbers_count, numbers);
    }

    /* copy the alphabet into internal buffer 1 */
    wcsncpy(hashids->alphabet_copy_1, hashids->alphabet,
        hashids->alphabet_length);

    /* walk arguments once and generate a hash */
    for (i = 0, numbers_hash = 0; i < numbers_count; ++i) {
        number = numbers[i];
        numbers_hash += number % (i + 100);
    }

    /* lottery character */
    lottery = hashids->alphabet[numbers_hash % hashids->alphabet_length];

    /* start output buffer with it (or don't) */
    buffer[0] = lottery;
    buffer_end = buffer + 1;

    /* alphabet-like buffer used for salt at each iteration */
    hashids->alphabet_copy_2[0] = lottery;
    hashids->alphabet_copy_2[1] = '\0';
    wcsncat(hashids->alphabet_copy_2, hashids->salt,
        hashids->alphabet_length - 1);
    p = hashids->alphabet_copy_2 + hashids->salt_length + 1;
    p_max = hashids->alphabet_length - 1 - hashids->salt_length;
    if (p_max > 0) {
        wcsncat(hashids->alphabet_copy_2, hashids->alphabet,
                (size_t) p_max);
    } else {
        hashids->alphabet_copy_2[hashids->alphabet_length] = '\0';
    }

    for (i = 0; i < numbers_count; ++i) {
        /* take number */
        unsigned long long number_copy = number = numbers[i];

        /* create a salt for this iteration */
        if (p_max > 0) {
            wcsncpy(p, hashids->alphabet_copy_1, p_max);
        }

        /* shuffle the alphabet */
        hashids_shuffle(hashids->alphabet_copy_1, hashids->alphabet_length,
            hashids->alphabet_copy_2, hashids->alphabet_length);

        /* hash the number */
        wchar_t *buffer_temp = buffer_end;
        do {
            ch = hashids->alphabet_copy_1[number % hashids->alphabet_length];
            *buffer_end++ = ch;

            number = (unsigned long long) floorl((long double)number / (long double)hashids->alphabet_length);
        } while (number);

        /* reverse the hash we got */
        for (j = 0; j < (buffer_end - buffer_temp) / 2; ++j) {
            temp_ch = *(buffer_temp + j);
            *(buffer_temp + j) = *(buffer_end - 1 - j);
            *(buffer_end - 1 - j) = temp_ch;
        }

        if (i + 1 < numbers_count) {
            number_copy %= ch + i;
            *buffer_end = hashids->separators[number_copy %
                hashids->separators_count];
            ++buffer_end;
        }
    }

    /* intermediate string length */
    result_len = buffer_end - buffer;

    /* add guards before start padding with alphabet */
    if (result_len < hashids->min_hash_length) {
        unsigned long long guard_index = (numbers_hash + buffer[0]) % hashids->guards_count;
        wmemmove(buffer + 1, buffer, result_len);
        buffer[0] = hashids->guards[guard_index];
        ++result_len;

        if (result_len < hashids->min_hash_length) {
            guard_index = (numbers_hash + buffer[2]) % hashids->guards_count;
            buffer[result_len] = hashids->guards[guard_index];
            ++result_len;

            /* pad with half alphabet before and after */
            unsigned int half_length_floor, half_length_ceil;
            half_length_floor = (unsigned int) floor((float)hashids->alphabet_length / 2);
            half_length_ceil = (unsigned int) ceil((float)hashids->alphabet_length / 2);

            /* pad, pad, pad */
            while (result_len < hashids->min_hash_length) {
                wcsncpy(hashids->alphabet_copy_2, hashids->alphabet_copy_1,
                    hashids->alphabet_length);
                hashids_shuffle(hashids->alphabet_copy_1,
                    hashids->alphabet_length, hashids->alphabet_copy_2,
                    hashids->alphabet_length);

                wmemmove(buffer + half_length_ceil, buffer, result_len);
                wmemmove(buffer, hashids->alphabet_copy_1 + half_length_floor,
                    half_length_ceil);
                wmemmove(buffer + result_len + half_length_ceil,
                    hashids->alphabet_copy_1, half_length_floor);

                result_len += hashids->alphabet_length;
                unsigned long excess = result_len - hashids->min_hash_length;

                if (excess > 0) {
                    wmemmove(buffer, buffer + excess / 2,
                        hashids->min_hash_length);
                    result_len = hashids->min_hash_length;
                }
            }
        }
    }

    buffer[result_len] = '\0';
    return result_len;
}

/* encode many (variadic) */
unsigned long
hashids_encode_v(struct hashids_t *hashids, wchar_t *buffer,
    unsigned long numbers_count, ...)
{
    int i;
    unsigned long result;
    unsigned long long *numbers;
    va_list ap;

    numbers = _hashids_alloc(numbers_count * sizeof(unsigned long long));

    if (unlikely(!numbers)) {
        hashids_errno = HASHIDS_ERROR_ALLOC;
        return 0;
    }

    va_start(ap, numbers_count);
    for (i = 0; i < numbers_count; ++i) {
        numbers[i] = va_arg(ap, unsigned long long);
    }
    va_end(ap);

    result = hashids_encode(hashids, buffer, numbers_count, numbers);
    _hashids_free(numbers);

    return result;
}

/* encode one */
unsigned long
hashids_encode_one(struct hashids_t *hashids, wchar_t *buffer,
    const unsigned long long number)
{
    return hashids_encode(hashids, buffer, 1, &number);
}

/* numbers count */
unsigned long
hashids_numbers_count(struct hashids_t *hashids, wchar_t *str)
{
    unsigned int numbers_count;
    wchar_t ch, *p;

    /* skip characters until we find a guard */
    if (hashids->min_hash_length) {
        p = str;
        while ((ch = *p)) {
            if (wcschr(hashids->guards, ch)) {
                str = p + 1;
                break;
            }

            p++;
        }
    }

    /* parse */
    numbers_count = 0;
    while ((ch = *str)) {
        if (wcschr(hashids->guards, ch)) {
            break;
        }
        if (wcschr(hashids->separators, ch)) {
            numbers_count++;
            str++;
            continue;
        }
        if (!wcschr(hashids->alphabet, ch)) {
            hashids_errno = HASHIDS_ERROR_INVALID_HASH;
            return 0;
        }

        str++;
    }

    /* account for the last number */
    return numbers_count + 1;
}

/* decode */
unsigned long
hashids_decode(struct hashids_t *hashids, wchar_t *str,
    unsigned long long *numbers)
{
    //struct hashids_t *hashids_temp = hashids_init3(hashids->salt, hashids->min_hash_length, hashids->alphabet);

    unsigned long numbers_count;
    unsigned long long number;
    wchar_t lottery, ch, *p, *c, *str_copy = str;
    long p_max;

    numbers_count = hashids_numbers_count(hashids, str);

    if (!numbers) {
        return numbers_count;
    }

    /* skip characters until we find a guard */
    if (hashids->min_hash_length) {
        p = str;
        while ((ch = *p)) {
            if (wcschr(hashids->guards, ch)) {
                str = p + 1;
                break;
            }

            p++;
        }
    }

    /* get the lottery character */
    lottery = *str++;

    /* copy the alphabet into internal buffer 1 */
    wcsncpy(hashids->alphabet_copy_1, hashids->alphabet,
        hashids->alphabet_length);

    /* alphabet-like buffer used for salt at each iteration */
    hashids->alphabet_copy_2[0] = lottery;
    hashids->alphabet_copy_2[1] = '\0';
    wcsncat(hashids->alphabet_copy_2, hashids->salt,
        hashids->alphabet_length - 1);
    p = hashids->alphabet_copy_2 + hashids->salt_length + 1;
    p_max = hashids->alphabet_length - 1 - hashids->salt_length;
    if (p_max > 0) {
        wcsncat(hashids->alphabet_copy_2, hashids->alphabet,
            p_max);
    } else {
        hashids->alphabet_copy_2[hashids->alphabet_length] = '\0';
    }

    /* first shuffle */
    hashids_shuffle(hashids->alphabet_copy_1, hashids->alphabet_length,
        hashids->alphabet_copy_2, hashids->alphabet_length);

    /* parse */
    number = 0;
    while ((ch = *str)) {
        if (wcschr(hashids->guards, ch)) {
            break;
        }
        if (wcschr(hashids->separators, ch)) {
            *numbers++ = number;
            number = 0;

            /* resalt the alphabet */
            if (p_max > 0) {
                wcsncpy(p, hashids->alphabet_copy_1, (size_t) p_max);
            }
            hashids_shuffle(hashids->alphabet_copy_1, hashids->alphabet_length,
                hashids->alphabet_copy_2, hashids->alphabet_length);

            str++;
            continue;
        }
        if (!(c = wcschr(hashids->alphabet_copy_1, ch))) {
            hashids_errno = HASHIDS_ERROR_INVALID_HASH;
            return 0;
        }

        number *= hashids->alphabet_length;
        number += c - hashids->alphabet_copy_1;

        str++;
    }

    /* store last number */
    *numbers = number;

    // wchar_t *buffer = _hashids_alloc(hashids_estimate_encoded_size(hashids_temp, numbers_count, numbers));
    // hashids_encode(hashids_temp, buffer, numbers_count, numbers);
    // if (wcscmp(buffer, str_copy) != 0) {
    //     free(buffer);
    //     return 0;
    // }
    //
    // free(buffer);
    // free(hashids_temp);

    return numbers_count;
}

/* encode hex */
unsigned long
hashids_encode_hex(struct hashids_t *hashids, wchar_t *buffer,
    const wchar_t *hex_str)
{
    size_t len;
    wchar_t *temp, *p;
    unsigned long long number;
    unsigned long result;

    len = wcslen(hex_str);
    temp = _hashids_alloc(sizeof(wchar_t) * (len + 2));

    if (!temp) {
        hashids_errno = HASHIDS_ERROR_ALLOC;
        return 0;
    }

    temp[0] = '1';
    wcsncpy(temp + 1, hex_str, len);

    number = wcstoull(temp, &p, 16);

    if (p == temp) {
        _hashids_free(temp);
        hashids_errno = HASHIDS_ERROR_INVALID_NUMBER;
        return 0;
    }

    result = hashids_encode(hashids, buffer, 1, &number);
    _hashids_free(temp);

    return result;
}

/* decode hex */
unsigned long
hashids_decode_hex(struct hashids_t *hashids, wchar_t *str, wchar_t *output)
{
    unsigned long result, i;
    unsigned long long number;
    wchar_t ch, *temp;

    result = hashids_numbers_count(hashids, str);

    if (result != 1) {
        return 0;
    }

    result = hashids_decode(hashids, str, &number);

    if (result != 1) {
        return 0;
    }

    temp = output;

    do {
        ch = number % 16;
        if (ch > 9) {
            ch += 'A' - 10;
        } else {
            ch += '0';
        }

        *temp++ = (wchar_t)ch;

        number /= 16;
    } while (number);

    temp--;
    *temp = 0;

    for (i = 0; i < (temp - output) / 2; ++i) {
        ch = *(output + i);
        *(output + i) = *(temp - 1 - i);
        *(temp - 1 - i) = ch;
    }

    return 1;
}
