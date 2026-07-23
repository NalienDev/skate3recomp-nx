// endian.h - Nintendo Switch / devkitA64 (newlib) shim
//
// newlib ships <machine/endian.h> (BYTE_ORDER/LITTLE_ENDIAN/BIG_ENDIAN) but not
// the glibc-style <endian.h>. This provides the glibc names and byte-order
// helpers on top of the machine header so code written for Linux compiles.
#pragma once

#include <machine/endian.h>
#include <stdint.h>

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#ifndef __PDP_ENDIAN
#ifdef PDP_ENDIAN
#define __PDP_ENDIAN PDP_ENDIAN
#endif
#endif
#ifndef __BYTE_ORDER
#define __BYTE_ORDER BYTE_ORDER
#endif

// AArch64 on Switch is little-endian; convert accordingly using compiler
// builtins. Provided as macros to match glibc's function-like interface.
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htobe16(x) __builtin_bswap16(x)
#define htole16(x) ((uint16_t)(x))
#define be16toh(x) __builtin_bswap16(x)
#define le16toh(x) ((uint16_t)(x))
#define htobe32(x) __builtin_bswap32(x)
#define htole32(x) ((uint32_t)(x))
#define be32toh(x) __builtin_bswap32(x)
#define le32toh(x) ((uint32_t)(x))
#define htobe64(x) __builtin_bswap64(x)
#define htole64(x) ((uint64_t)(x))
#define be64toh(x) __builtin_bswap64(x)
#define le64toh(x) ((uint64_t)(x))
#else
#define htobe16(x) ((uint16_t)(x))
#define htole16(x) __builtin_bswap16(x)
#define be16toh(x) ((uint16_t)(x))
#define le16toh(x) __builtin_bswap16(x)
#define htobe32(x) ((uint32_t)(x))
#define htole32(x) __builtin_bswap32(x)
#define be32toh(x) ((uint32_t)(x))
#define le32toh(x) __builtin_bswap32(x)
#define htobe64(x) ((uint64_t)(x))
#define htole64(x) __builtin_bswap64(x)
#define be64toh(x) ((uint64_t)(x))
#define le64toh(x) __builtin_bswap64(x)
#endif
