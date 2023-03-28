/* number[size - 1] = msb, number[0] = lsb */
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

#ifndef DIV_ROUNDUP
#define DIV_ROUNDUP(x, len) (((x) + (len) -1) / (len))
#endif

/* count leading zeros of src*/
static int bn_clz(const bn *src)
{
    int cnt = 0;
    for (int i = src->size - 1; i >= 0; i--) {
        if (src->number[i]) {
            // prevent undefined behavior when src = 0
            cnt += __builtin_clz(src->number[i]);
            return cnt;
        } else {
            cnt += 32;
        }
    }
    return cnt;
}

/* count the digits of most significant bit */
static int bn_msb(const bn *src)
{
    return src->size * 32 - bn_clz(src);
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
    b->number = (unsigned int *) kmalloc(sizeof(int) * n, GFP_KERNEL);
    for (unsigned int i = 0; i < n; i++)
        b->number[i] = 0;
    return b;
}

int bn_resize(bn *src, unsigned int size)
{
    if (size == src->size)
        return 0;

    src->number = krealloc(src->number, sizeof(int) * size, GFP_KERNEL);
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
    memcpy(dest->number, src->number, src->size * sizeof(int));
    return 0;
}


/* left bit shift on bn (maximun shift 31) */
void bn_lshift(bn *src, size_t shift)
{
    size_t z = bn_clz(src);
    shift %= 32;  // only handle shift within 32 bits atm
    if (!shift)
        return;

    if (shift > z)
        bn_resize(src, src->size + 1);
    /* bit shift */
    for (int i = src->size - 1; i > 0; i--)
        src->number[i] =
            src->number[i] << shift | src->number[i - 1] >> (32 - shift);
    src->number[0] <<= shift;
}

/* right bit shift on bn (maximun shift 31) */
void bn_rshift(bn *src, size_t shift)
{
    size_t z = 32 - bn_clz(src);
    shift %= 32;  // only handle shift within 32 bits atm
    if (!shift)
        return;

    /* bit shift */
    for (int i = 0; i < (src->size - 1); i++)
        src->number[i] = src->number[i] >> shift | src->number[i + 1]
                                                       << (32 - shift);
    src->number[src->size - 1] >>= shift;

    if (shift >= z && src->size > 1)
        bn_resize(src, src->size - 1);
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
    // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
    size_t len = (8 * sizeof(int) * src->size) / 3 + 2 + src->sign;
    char *s = kmalloc(len, GFP_KERNEL);
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    for (int i = src->size - 1; i >= 0; i--) {
        for (unsigned int d = 1U << 31; d; d >>= 1) {
            /* binary -> decimal string */
            int carry = !!(d & src->number[i]);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;  // double it
                carry = (s[j] > '9');
                if (carry)
                    s[j] -= 10;
            }
        }
    }
    // skip leading zero
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
    // max digits = max(sizeof(a) + sizeof(b)) + 1
    int d = MAX(bn_msb(a), bn_msb(b)) + 1;
    d = DIV_ROUNDUP(d, 32) + !d;
    bn_resize(c, d);  // round up, min size = 1

    unsigned long long int carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->number[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->number[i] : 0;
        carry += (unsigned long long int) tmp1 + tmp2;
        c->number[i] = carry;
        carry >>= 32;
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
            c->number[i] = carry + (1LL << 32);
            carry = 1;
        } else {
            c->number[i] = carry;
            carry = 0;
        }
    }

    d = bn_clz(c) / 32;
    if (d == c->size)
        --d;
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
        carry += c->number[i] + (x & 0xFFFFFFFF);
        c->number[i] = carry;
        carry >>= 32;
        x >>= 32;
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
    // max digits = sizeof(a) + sizeof(b))
    int d = bn_msb(a) + bn_msb(b);
    d = DIV_ROUNDUP(d, 32) + !d;  // round up, min size = 1
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

    if (tmp) {
        bn_cpy(tmp, c);  // restore c
        bn_free(c);
    }
}
