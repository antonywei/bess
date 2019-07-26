//
// Created by whr on 7/21/19.
//

#include <stdio.h>
#include "hash.h"
#include <string.h>
#define BIG_CONSTANT(x) (x##LLU)


uint64_t AwareHash(unsigned char* data, uint64_t n,
                   uint64_t hash, uint64_t scale, uint64_t hardener) {

    n = 2*n;
    char tmp = 0;
    while (n) {
        hash *= scale;
        if(n%2 == 0)
            tmp = *data++;
        hash += (tmp >> (n%2)*4) % (1 << (n%2+1)*4);
        n--;
    }
    return hash ^ hardener;
}

uint64_t AwareHash_debug(unsigned char* data, uint64_t n,
                         uint64_t hash, uint64_t scale, uint64_t hardener) {

    while (n) {
        fprintf(stderr, "    %lu %lu %lu %u\n", n, hash, scale, *data);
        hash *= scale;
        hash += *data++;
        n--;
        fprintf(stderr, "        internal %lu\n", hash);
    }
    return hash ^ hardener;
}



uint64_t seed = 0;
uint64_t GenHashSeed(uint64_t index) {
    /*
    if (index == 0) {
        srand(0);
    }
    */
    if (seed == 0) {
        seed = rand();
    }
    uint64_t x, y = seed + index;
    mangle((const unsigned char*)&y, (unsigned char*)&x, 8);
    return AwareHash((uint8_t*)&y, 8, 388650253, 388650319, 1176845762);
}

void mangle(const unsigned char* key, unsigned char* ret_key,
            int nbytes) {
    int i, j;
    unsigned long long new_key;
    for (j = 0; j < nbytes/4; ++j) {
        new_key = 0;
        for (i=0; i< 4; ++i) {
            uint64_t tmp = 0;
            tmp = key[4-i-1+j*4];
            new_key |= tmp << (i * 8);
        }
        new_key = (new_key * 2083697005) & (0xffffffffffffffff);
        for (i=0; i<4; ++i) {
            ret_key[i+4*j] = (new_key >> (i * 8)) & 0xff;
        }
    }

    int remain = nbytes%4;
    new_key = 0;
    for (i = 0; i < remain; ++i) {
        uint64_t tmp = 0;
        tmp = key[remain-i-1+j*4];
        new_key |= tmp << (i * 8);
    }
    new_key = (new_key * 2083697005) & (0xffffffffffffffff);
    for (i = 0; i < remain; ++i) {
        ret_key[i+4*j] = (new_key >> (i * 8)) & 0xff;
    }
}



int is_prime(int num) {
    int i;
    for (i=2; i<num; i++) {
        if ((num % i) == 0) {
            break;
        }
    }
    if (i == num) {
        return 1;
    }
    return 0;
}

int calc_next_prime(int num) {
    while (!is_prime(num)) {
        num++;
    }
    return num;
}


//-----------------------------------------------------------------------------
// MurmurHash2, 64-bit versions, by Austin Appleby
// The same caveats as 32-bit MurmurHash2 apply here - beware of alignment
// and endian-ness issues if used across multiple platforms.
// 64-bit hash for 64-bit platforms
uint64_t MurmurHash64A ( const void * key, int len, uint64_t seed )
{
    const uint64_t m = BIG_CONSTANT(0xc6a4a7935bd1e995);
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t * data = (const uint64_t *)key;
    const uint64_t * end = data + (len/8);

    while(data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char * data2 = (const unsigned char*)data;

    switch(len & 7)
    {
        case 7: h ^= uint64_t(data2[6]) << 48;
            break;
        case 6: h ^= uint64_t(data2[5]) << 40;break;
        case 5: h ^= uint64_t(data2[4]) << 32;break;
        case 4: h ^= uint64_t(data2[3]) << 24;break;
        case 3: h ^= uint64_t(data2[2]) << 16;break;
        case 2: h ^= uint64_t(data2[1]) << 8;break;
        case 1: h ^= uint64_t(data2[0]);
            h *= m;break;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}



#if defined(_MSC_VER)

#define FORCE_INLINE__forceinline

#include <stdlib.h>

#define ROTL32(x,y)_rotl(x,y)
#define ROTL64(x,y)_rotl64(x,y)

#define BIG_CONSTANT(x) (x)

#else// defined(_MSC_VER)
#define FORCE_INLINE inline __attribute__((always_inline))

inline uint32_t rotl32 ( uint32_t x, int8_t r  )
{
    return (x << r) | (x >> (32 - r));
}

inline uint64_t rotl64 ( uint64_t x, int8_t r  )
{
    return (x << r) | (x >> (64 - r));
}

#define ROTL32(x,y)rotl32(x,y)
#define ROTL64(x,y)rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

#endif // !defined(_MSC_VER)

FORCE_INLINE uint32_t getblock32 ( const uint32_t * p, int i  )
{
    return p[i];

}

FORCE_INLINE uint64_t getblock64 ( const uint64_t * p, int i  )
{
    return p[i];

}

FORCE_INLINE uint32_t fmix32 ( uint32_t h  )
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;

}

FORCE_INLINE uint64_t fmix64 ( uint64_t k  )
{
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    return k;

}
