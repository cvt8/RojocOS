#ifndef _STDINT_H_
#define _STDINT_H_

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long int64_t;
typedef unsigned long uint64_t;

typedef long intptr_t;                // ints big enough to hold pointers
typedef unsigned long uintptr_t;

typedef __int128_t int128_t;
typedef __uint128_t uint128_t;

#endif /* _STDINT_H_ */