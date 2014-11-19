/**********************************************************************
 * Copyright (c) 2013, 2014 Pieter Wuille                             *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _SECP256K1_NUM_REPR_IMPL_H_
#define _SECP256K1_NUM_REPR_IMPL_H_

#include <string.h>
#include <stdlib.h>
#include <gmp.h>

#include "util.h"
#include "num.h"

#ifdef VERIFY
static void secp256k1_num_sanity(const secp256k1_num_t *a) {
    VERIFY_CHECK(a->limbs == 1 || (a->limbs > 1 && a->data[a->limbs-1] != 0));
}
#else
#define secp256k1_num_sanity(a) do { } while(0)
#endif

static void secp256k1_num_init(secp256k1_num_t *r) {
    r->neg = 0;
    r->limbs = 1;
    r->data[0] = 0;
}

static void secp256k1_num_clear(secp256k1_num_t *r) {
    memset(r, 0, sizeof(*r));
}

static void secp256k1_num_free(secp256k1_num_t *r) {
    (void)r;
}

static void secp256k1_num_copy(secp256k1_num_t *r, const secp256k1_num_t *a) {
    *r = *a;
}

static int secp256k1_num_bits(const secp256k1_num_t *a) {
    int ret=(a->limbs-1)*GMP_NUMB_BITS;
    mp_limb_t x=a->data[a->limbs-1];
    while (x) {
        x >>= 1;
        ret++;
    }
    return ret;
}


static void secp256k1_num_get_bin(unsigned char *r, unsigned int rlen, const secp256k1_num_t *a) {
    unsigned char tmp[65];
    int len = 0;
    if (a->limbs>1 || a->data[0] != 0) {
        len = mpn_get_str(tmp, 256, (mp_limb_t*)a->data, a->limbs);
    }
    int shift = 0;
    while (shift < len && tmp[shift] == 0) shift++;
    VERIFY_CHECK(len-shift <= (int)rlen);
    memset(r, 0, rlen - len + shift);
    if (len > shift) {
        memcpy(r + rlen - len + shift, tmp + shift, len - shift);
    }
    memset(tmp, 0, sizeof(tmp));
}

static void secp256k1_num_set_bin(secp256k1_num_t *r, const unsigned char *a, unsigned int alen) {
    VERIFY_CHECK(alen > 0);
    VERIFY_CHECK(alen <= 64);
    int len = mpn_set_str(r->data, a, alen, 256);
    VERIFY_CHECK(len <= NUM_LIMBS*2);
    r->limbs = len;
    r->neg = 0;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) r->limbs--;
}

static void secp256k1_num_set_int(secp256k1_num_t *r, int a) {
    r->limbs = 1;
    r->neg = (a < 0);
    r->data[0] = (a < 0) ? -a : a;
}

static void secp256k1_num_add_abs(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    mp_limb_t c = mpn_add(r->data, a->data, a->limbs, b->data, b->limbs);
    r->limbs = a->limbs;
    if (c != 0) {
        VERIFY_CHECK(r->limbs < 2*NUM_LIMBS);
        r->data[r->limbs++] = c;
    }
}

static void secp256k1_num_sub_abs(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    mp_limb_t c = mpn_sub(r->data, a->data, a->limbs, b->data, b->limbs);
    VERIFY_CHECK(c == 0);
    r->limbs = a->limbs;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) r->limbs--;
}

static void secp256k1_num_mod(secp256k1_num_t *r, const secp256k1_num_t *m) {
    secp256k1_num_sanity(r);
    secp256k1_num_sanity(m);

    if (r->limbs >= m->limbs) {
        mp_limb_t t[2*NUM_LIMBS];
        mpn_tdiv_qr(t, r->data, 0, r->data, r->limbs, m->data, m->limbs);
        memset(t, 0, sizeof(t));
        r->limbs = m->limbs;
        while (r->limbs > 1 && r->data[r->limbs-1]==0) r->limbs--;
    }

    if (r->neg && (r->limbs > 1 || r->data[0] != 0)) {
        secp256k1_num_sub_abs(r, m, r);
        r->neg = 0;
    }
}

static void secp256k1_num_mod_inverse(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *m) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(m);

    /** mpn_gcdext computes: (G,S) = gcdext(U,V), where
     *  * G = gcd(U,V)
     *  * G = U*S + V*T
     *  * U has equal or more limbs than V, and V has no padding
     *  If we set U to be (a padded version of) a, and V = m:
     *    G = a*S + m*T
     *    G = a*S mod m
     *  Assuming G=1:
     *    S = 1/a mod m
     */
    VERIFY_CHECK(m->limbs <= NUM_LIMBS);
    VERIFY_CHECK(m->data[m->limbs-1] != 0);
    mp_limb_t g[NUM_LIMBS+1];
    mp_limb_t u[NUM_LIMBS+1];
    mp_limb_t v[NUM_LIMBS+1];
    for (int i=0; i < m->limbs; i++) {
        u[i] = (i < a->limbs) ? a->data[i] : 0;
        v[i] = m->data[i];
    }
    mp_size_t sn = NUM_LIMBS+1;
    mp_size_t gn = mpn_gcdext(g, r->data, &sn, u, m->limbs, v, m->limbs);
    VERIFY_CHECK(gn == 1);
    VERIFY_CHECK(g[0] == 1);
    r->neg = a->neg ^ m->neg;
    if (sn < 0) {
        mpn_sub(r->data, m->data, m->limbs, r->data, -sn);
        r->limbs = m->limbs;
        while (r->limbs > 1 && r->data[r->limbs-1]==0) r->limbs--;
    } else {
        r->limbs = sn;
    }
    memset(g, 0, sizeof(g));
    memset(u, 0, sizeof(u));
    memset(v, 0, sizeof(v));
}

static int secp256k1_num_is_zero(const secp256k1_num_t *a) {
    return (a->limbs == 1 && a->data[0] == 0);
}

static int secp256k1_num_is_odd(const secp256k1_num_t *a) {
    return a->data[0] & 1;
}

static int secp256k1_num_is_neg(const secp256k1_num_t *a) {
    return (a->limbs > 1 || a->data[0] != 0) && a->neg;
}

static int secp256k1_num_cmp(const secp256k1_num_t *a, const secp256k1_num_t *b) {
    if (a->limbs > b->limbs) return 1;
    if (a->limbs < b->limbs) return -1;
    return mpn_cmp(a->data, b->data, a->limbs);
}

static int secp256k1_num_eq(const secp256k1_num_t *a, const secp256k1_num_t *b) {
    if (a->limbs > b->limbs) return 0;
    if (a->limbs < b->limbs) return 0;
    if ((a->neg && !secp256k1_num_is_zero(a)) != (b->neg && !secp256k1_num_is_zero(b))) return 0;
    return mpn_cmp(a->data, b->data, a->limbs) == 0;
}

static void secp256k1_num_subadd(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b, int bneg) {
    if (!(b->neg ^ bneg ^ a->neg)) { /* a and b have the same sign */
        r->neg = a->neg;
        if (a->limbs >= b->limbs) {
            secp256k1_num_add_abs(r, a, b);
        } else {
            secp256k1_num_add_abs(r, b, a);
        }
    } else {
        if (secp256k1_num_cmp(a, b) > 0) {
            r->neg = a->neg;
            secp256k1_num_sub_abs(r, a, b);
        } else {
            r->neg = b->neg ^ bneg;
            secp256k1_num_sub_abs(r, b, a);
        }
    }
}

static void secp256k1_num_add(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);
    secp256k1_num_subadd(r, a, b, 0);
}

static void secp256k1_num_sub(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);
    secp256k1_num_subadd(r, a, b, 1);
}

static void secp256k1_num_mul(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);

    mp_limb_t tmp[2*NUM_LIMBS+1];
    VERIFY_CHECK(a->limbs + b->limbs <= 2*NUM_LIMBS+1);
    if ((a->limbs==1 && a->data[0]==0) || (b->limbs==1 && b->data[0]==0)) {
        r->limbs = 1;
        r->neg = 0;
        r->data[0] = 0;
        return;
    }
    if (a->limbs >= b->limbs)
        mpn_mul(tmp, a->data, a->limbs, b->data, b->limbs);
    else
        mpn_mul(tmp, b->data, b->limbs, a->data, a->limbs);
    r->limbs = a->limbs + b->limbs;
    if (r->limbs > 1 && tmp[r->limbs - 1]==0) r->limbs--;
    VERIFY_CHECK(r->limbs <= 2*NUM_LIMBS);
    mpn_copyi(r->data, tmp, r->limbs);
    r->neg = a->neg ^ b->neg;
    memset(tmp, 0, sizeof(tmp));
}

static void secp256k1_num_div(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);
    if (b->limbs > a->limbs) {
        r->limbs = 1;
        r->data[0] = 0;
        r->neg = 0;
        return;
    }

    mp_limb_t quo[2*NUM_LIMBS+1];
    mp_limb_t rem[2*NUM_LIMBS+1];
    mpn_tdiv_qr(quo, rem, 0, a->data, a->limbs, b->data, b->limbs);
    mpn_copyi(r->data, quo, a->limbs - b->limbs + 1);
    r->limbs = a->limbs - b->limbs + 1;
    while (r->limbs > 1 && r->data[r->limbs - 1]==0) r->limbs--;
    r->neg = a->neg ^ b->neg;
}

static void secp256k1_num_mod_mul(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b, const secp256k1_num_t *m) {
    secp256k1_num_mul(r, a, b);
    secp256k1_num_mod(r, m);
}


static int secp256k1_num_shift(secp256k1_num_t *r, int bits) {
    VERIFY_CHECK(bits <= GMP_NUMB_BITS);
    mp_limb_t ret = mpn_rshift(r->data, r->data, r->limbs, bits);
    if (r->limbs>1 && r->data[r->limbs-1]==0) r->limbs--;
    ret >>= (GMP_NUMB_BITS - bits);
    return ret;
}

static int secp256k1_num_get_bit(const secp256k1_num_t *a, int pos) {
    return (a->limbs*GMP_NUMB_BITS > pos) && ((a->data[pos/GMP_NUMB_BITS] >> (pos % GMP_NUMB_BITS)) & 1);
}

static void secp256k1_num_inc(secp256k1_num_t *r) {
    mp_limb_t ret = mpn_add_1(r->data, r->data, r->limbs, (mp_limb_t)1);
    if (ret) {
        VERIFY_CHECK(r->limbs < 2*NUM_LIMBS);
        r->data[r->limbs++] = ret;
    }
}

static void secp256k1_num_set_hex(secp256k1_num_t *r, const char *a, int alen) {
    static const unsigned char cvt[256] = {
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 1, 2, 3, 4, 5, 6,7,8,9,0,0,0,0,0,0,
        0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0
    };
    unsigned char num[257] = {};
    for (int i=0; i<alen; i++) {
        num[i] = cvt[(unsigned char)a[i]];
    }
    r->limbs = mpn_set_str(r->data, num, alen, 16);
    r->neg = 0;
    while (r->limbs > 1 && r->data[r->limbs-1] == 0) r->limbs--;
}

static void secp256k1_num_get_hex(char *r, int rlen, const secp256k1_num_t *a) {
    static const unsigned char cvt[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    unsigned char *tmp = malloc(257);
    mp_size_t len = mpn_get_str(tmp, 16, (mp_limb_t*)a->data, a->limbs);
    VERIFY_CHECK(len <= rlen);
    for (int i=0; i<len; i++) {
        VERIFY_CHECK(rlen-len+i >= 0);
        VERIFY_CHECK(rlen-len+i < rlen);
        VERIFY_CHECK(tmp[i] < 16);
        r[rlen-len+i] = cvt[tmp[i]];
    }
    for (int i=0; i<rlen-len; i++) {
        VERIFY_CHECK(i >= 0);
        VERIFY_CHECK(i < rlen);
        r[i] = cvt[0];
    }
    free(tmp);
}

static void secp256k1_num_split(secp256k1_num_t *rl, secp256k1_num_t *rh, const secp256k1_num_t *a, int bits) {
    VERIFY_CHECK(bits > 0);
    rh->neg = a->neg;
    if (bits >= a->limbs * GMP_NUMB_BITS) {
        *rl = *a;
        rh->limbs = 1;
        rh->data[0] = 0;
        return;
    }
    rl->limbs = 0;
    rl->neg = a->neg;
    int left = bits;
    while (left >= GMP_NUMB_BITS) {
        rl->data[rl->limbs] = a->data[rl->limbs];
        rl->limbs++;
        left -= GMP_NUMB_BITS;
    }
    if (left == 0) {
        mpn_copyi(rh->data, a->data + rl->limbs, a->limbs - rl->limbs);
        rh->limbs = a->limbs - rl->limbs;
    } else {
        mpn_rshift(rh->data, a->data + rl->limbs, a->limbs - rl->limbs, left);
        rh->limbs = a->limbs - rl->limbs;
        while (rh->limbs>1 && rh->data[rh->limbs-1]==0) rh->limbs--;
    }
    if (left > 0) {
        rl->data[rl->limbs] = a->data[rl->limbs] & ((((mp_limb_t)1) << left) - 1);
        rl->limbs++;
    }
    while (rl->limbs>1 && rl->data[rl->limbs-1]==0) rl->limbs--;
}

static void secp256k1_num_negate(secp256k1_num_t *r) {
    r->neg ^= 1;
}

static int secp256k1_num_get_bits(const secp256k1_num_t *a, int offset, int count) {
    int ret = 0;
    for (int i = 0; i < count; i++) {
        ret |= ((a->data[(offset + i) / GMP_NUMB_BITS] >> ((offset + i) % GMP_NUMB_BITS)) & 1) << i;
    }
    return ret;
}

#endif
