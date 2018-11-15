// intx: extended precision integer library.
// Copyright 2018 Pawel Bylica.
// Licensed under the Apache License, Version 2.0. See the LICENSE file.

#include <intx/int128.hpp>

namespace intx
{
union utwords
{
    unsigned __int128 all;
    struct
    {
        uint64_t low;
        uint64_t high;
    } s;
};

uint128 udivrem_128(uint128 a, uint128 b, uint128* rem)
{
    using du_int = uint64_t;
    using tu_int = unsigned __int128;
    const unsigned n_udword_bits = sizeof(du_int) * 8;
    const unsigned n_utword_bits = sizeof(tu_int) * 8;
    uint128 n = a;
    uint128 d = b;
    uint128 q;
    uint128 r;
    unsigned sr;
    /* special cases, X is unknown, K != 0 */
    if (n.hi == 0)
    {
        if (d.hi == 0)
        {
            /* 0 X
             * ---
             * 0 X
             */
            if (rem)
                *rem = n.lo % d.lo;
            return n.lo / d.lo;
        }
        /* 0 X
         * ---
         * K X
         */
        if (rem)
            *rem = n.lo;
        return 0;
    }
    /* n.hi != 0 */
    if (d.lo == 0)
    {
        if (d.hi == 0)
        {
            /* K X
             * ---
             * 0 0
             */
            if (rem)
                *rem = n.hi % d.lo;
            return n.hi / d.lo;
        }
        /* d.hi != 0 */
        if (n.lo == 0)
        {
            /* K 0
             * ---
             * K 0
             */
            if (rem)
            {
                r.hi = n.hi % d.hi;
                r.lo = 0;
                *rem = r;
            }
            return n.hi / d.hi;
        }
        /* K K
         * ---
         * K 0
         */
        if ((d.hi & (d.hi - 1)) == 0) /* if d is a power of 2 */
        {
            if (rem)
            {
                r.lo = n.lo;
                r.hi = n.hi & (d.hi - 1);
                *rem = r;
            }
            return n.hi >> __builtin_ctzll(d.hi);
        }
        /* K K
         * ---
         * K 0
         */
        sr = __builtin_clzll(d.hi) - __builtin_clzll(n.hi);
        /* 0 <= sr <= n_udword_bits - 2 or sr large */
        if (sr > n_udword_bits - 2)
        {
            if (rem)
                *rem = n;
            return 0;
        }
        ++sr;
        /* 1 <= sr <= n_udword_bits - 1 */
        /* q = n << (n_utword_bits - sr); */
        q.lo = 0;
        q.hi = n.lo << (n_udword_bits - sr);
        /* r = n >> sr; */
        r.hi = n.hi >> sr;
        r.lo = (n.hi << (n_udword_bits - sr)) | (n.lo >> sr);
    }
    else /* d.lo != 0 */
    {
        if (d.hi == 0)
        {
            /* K X
             * ---
             * 0 K
             */
            if ((d.lo & (d.lo - 1)) == 0) /* if d is a power of 2 */
            {
                if (rem)
                    *rem = n.lo & (d.lo - 1);
                if (d.lo == 1)
                    return n;
                sr = __builtin_ctzll(d.lo);
                q.hi = n.hi >> sr;
                q.lo = (n.hi << (n_udword_bits - sr)) | (n.lo >> sr);
                return q;
            }
            /* K X
             * ---
             * 0 K
             */
            sr = 1 + n_udword_bits + __builtin_clzll(d.lo) - __builtin_clzll(n.hi);
            /* 2 <= sr <= n_utword_bits - 1
             * q = n << (n_utword_bits - sr);
             * r = n >> sr;
             */
            if (sr == n_udword_bits)
            {
                q.lo = 0;
                q.hi = n.lo;
                r.hi = 0;
                r.lo = n.hi;
            }
            else if (sr < n_udword_bits)  // 2 <= sr <= n_udword_bits - 1
            {
                q.lo = 0;
                q.hi = n.lo << (n_udword_bits - sr);
                r.hi = n.hi >> sr;
                r.lo = (n.hi << (n_udword_bits - sr)) | (n.lo >> sr);
            }
            else  // n_udword_bits + 1 <= sr <= n_utword_bits - 1
            {
                q.lo = n.lo << (n_utword_bits - sr);
                q.hi = (n.hi << (n_utword_bits - sr)) | (n.lo >> (sr - n_udword_bits));
                r.hi = 0;
                r.lo = n.hi >> (sr - n_udword_bits);
            }
        }
        else
        {
            /* K X
             * ---
             * K K
             */
            sr = __builtin_clzll(d.hi) - __builtin_clzll(n.hi);
            /*0 <= sr <= n_udword_bits - 1 or sr large */
            if (sr > n_udword_bits - 1)
            {
                if (rem)
                    *rem = n;
                return 0;
            }
            ++sr;
            /* 1 <= sr <= n_udword_bits
             * q = n << (n_utword_bits - sr);
             * r = n >> sr;
             */
            q.lo = 0;
            if (sr == n_udword_bits)
            {
                q.hi = n.lo;
                r.hi = 0;
                r.lo = n.hi;
            }
            else
            {
                r.hi = n.hi >> sr;
                r.lo = (n.hi << (n_udword_bits - sr)) | (n.lo >> sr);
                q.hi = n.lo << (n_udword_bits - sr);
            }
        }
    }
    /* Not a special case
     * q and r are initialized with:
     * q = n << (n_utword_bits - sr);
     * r = n >> sr;
     * 1 <= sr <= n_utword_bits - 1
     */
    unsigned carry = 0;
    for (; sr > 0; --sr)
    {
        /* r:q = ((r:q)  << 1) | carry */
        r.hi = (r.hi << 1) | (r.lo >> (n_udword_bits - 1));
        r.lo = (r.lo << 1) | (q.hi >> (n_udword_bits - 1));
        q.hi = (q.hi << 1) | (q.lo >> (n_udword_bits - 1));
        q.lo = (q.lo << 1) | carry;
        /* carry = 0;
         * if (r >= d)
         * {
         *     r -= d;
         *      carry = 1;
         * }
         */
        auto da = d - r - 1;
        __int128 dx = (__int128(da.hi) << 64) | da.lo;
        const __int128 s = dx >> (n_utword_bits - 1);
        carry = s & 1;
        r -= d & s;
    }
    q = (q << 1) | carry;
    if (rem)
        *rem = r;
    return q;
}
}  // namespace intx
