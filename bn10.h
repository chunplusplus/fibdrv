#include <linux/kernel.h>

#define MAX_DIGITS 8
#define BOUND32 100000000U

typedef struct _bn {
    unsigned int *number;
    unsigned int size;
    int sign;
} bn;

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#ifndef SWAP
#define SWAP(x, y)           \
    do {                     \
        typeof(x) __tmp = x; \
        x = y;               \
        y = __tmp;           \
    } while (0)
#endif

/* count the size of number if carry */
static int size_if_carry(const bn *src)
{
    return src->size + !!(src->number[src->size - 1] / (BOUND32 / 10));
}

int bn_free(bn *src)
{
    if (src == NULL)
        return -1;
    kfree(src->number);
    kfree(src);
    return 0;
}

bn *bn_alloc(unsigned int n)
{
    bn *b = kmalloc(sizeof(bn), GFP_KERNEL);
    b->size = n;
    b->sign = 0;
    b->number = kmalloc(sizeof(unsigned int) * n, GFP_KERNEL);
    for (unsigned int i = 0; i < n; i++)
        b->number[i] = 0;
    return b;
}

int bn_resize(bn *src, unsigned int size)
{
    if (size == src->size)
        return 0;

    src->number =
        krealloc(src->number, sizeof(unsigned int) * size, GFP_KERNEL);
    for (unsigned int i = src->size; i < size; i++)
        src->number[i] = 0;
    src->size = size;

    return 0;
}

/*
 * copy the value from src to dest
 * return 0 on success, -1 on error
 */
int bn_cpy(bn *dest, bn *src)
{
    if (bn_resize(dest, src->size) < 0)
        return -1;
    dest->sign = src->sign;
    memcpy(dest->number, src->number, src->size * sizeof(unsigned int));
    return 0;
}

/*
 * compare length
 * return 1 if |a| > |b|
 * return -1 if |a| < |b|
 * return 0 if |a| = |b|
 */
int bn_cmp(const bn *a, const bn *b)
{
    if (a->size > b->size) {
        return 1;
    } else if (a->size < b->size) {
        return -1;
    } else {
        for (int i = a->size - 1; i >= 0; i--) {
            if (a->number[i] > b->number[i])
                return 1;
            if (a->number[i] < b->number[i])
                return -1;
        }
        return 0;
    }
}

/*
 * output bn to decimal string
 * Note: the returned string should be freed with kfree()
 */
char *bn_to_string(bn *src)
{
    size_t len = src->size * MAX_DIGITS + 2;
    char *s = kmalloc(len, GFP_KERNEL);
    char *p = s + 1;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    for (int i = src->size - 1; i >= 0; i--) {
        snprintf(p, MAX_DIGITS + 1, "%08u", src->number[i]);
        p += MAX_DIGITS;
    }
    // skip leading zero
    p = s;
    while (p[0] == '0' && p[1] != '\0') {
        p++;
    }
    if (src->sign)
        *(--p) = '-';
    memmove(s, p, strlen(p) + 1);
    return s;
}

/* |c| = |a| + |b| */
static void bn_do_add(const bn *a, const bn *b, bn *c)
{
    int d = MAX(size_if_carry(a), size_if_carry(b));
    bn_resize(c, d);

    unsigned int carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->number[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->number[i] : 0;
        carry += tmp1 + tmp2;
        c->number[i] = carry % BOUND32;
        carry = !!(carry >= BOUND32);
    }

    if (!c->number[c->size - 1] && c->size > 1)
        bn_resize(c, c->size - 1);
}

/*
 * |c| = |a| - |b|
 * Note: |a| > |b| must be true
 */
static void bn_do_sub(const bn *a, const bn *b, bn *c)
{
    // max digits = max(sizeof(a) + sizeof(b))
    int d = MAX(a->size, b->size);
    bn_resize(c, d);

    long long int carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->number[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->number[i] : 0;

        carry = (long long int) tmp1 - tmp2 - carry;
        if (carry < 0) {
            c->number[i] = carry + BOUND32;
            carry = 1;
        } else {
            c->number[i] = carry;
            carry = 0;
        }
    }

    d = 0;
    for (int i = c->size - 1; i > 0; i--) {
        if (c->number[i])
            break;
        else
            d++;
    }

    bn_resize(c, c->size - d);
}

/* c = a + b
 * Note: work for c == a or c == b
 */
void bn_add(const bn *a, const bn *b, bn *c)
{
    if (a->sign == b->sign) {  // both positive or negative
        bn_do_add(a, b, c);
        c->sign = a->sign;
    } else {          // different sign
        if (a->sign)  // let a > 0, b < 0
            SWAP(a, b);
        int cmp = bn_cmp(a, b);
        if (cmp > 0) {
            /* |a| > |b| and b < 0, hence c = a - |b| */
            bn_do_sub(a, b, c);
            c->sign = 0;
        } else if (cmp < 0) {
            /* |a| < |b| and b < 0, hence c = -(|b| - |a|) */
            bn_do_sub(b, a, c);
            c->sign = 1;
        } else {
            /* |a| == |b| */
            bn_resize(c, 1);
            c->number[0] = 0;
            c->sign = 0;
        }
    }
}

/* c = a - b
 * Note: work for c == a or c == b
 */
void bn_sub(const bn *a, const bn *b, bn *c)
{
    /* xor the sign bit of b and let bn_add handle it */
    bn tmp = *b;
    tmp.sign ^= 1;  // a - b = a + (-b)
    bn_add(a, &tmp, c);
}

void bn_swap(bn *a, bn *b)
{
    bn tmp = *a;
    *a = *b;
    *b = tmp;
}

/* c += x, starting from offset */

static void bn_mult_add(bn *c, int offset, unsigned long long int x)
{
    unsigned long long int carry = 0;
    for (int i = offset; i < c->size; i++) {
        carry += c->number[i] + (x % BOUND32);
        c->number[i] = carry % BOUND32;
        carry = carry / BOUND32;
        x = x / BOUND32;
        if (!x && !carry)  // done
            return;
    }
}

/*
 * c = a x b
 * Note: work for c == a or c == b
 * using the simple quadratic-time algorithm (long multiplication)
 */
void bn_mult(const bn *a, const bn *b, bn *c)
{
    int d = a->size + b->size;
    bn *tmp;
    /* make it work properly when c == a or c == b */
    if (c == a || c == b) {
        tmp = c;  // save c
        c = bn_alloc(d);
    } else {
        tmp = NULL;
        bn_resize(c, d);
    }

    for (int i = 0; i < a->size; i++) {
        for (int j = 0; j < b->size; j++) {
            unsigned long long int carry = 0;
            carry = (unsigned long long int) a->number[i] * b->number[j];
            bn_mult_add(c, i + j, carry);
        }
    }
    c->sign = a->sign ^ b->sign;

    d = 0;
    for (int i = c->size - 1; i > 0; i--) {
        if (c->number[i])
            break;
        else
            d++;
    }

    bn_resize(c, c->size - d);

    if (tmp) {
        bn_cpy(tmp, c);  // restore c
        bn_free(c);
    }
}
