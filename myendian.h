#ifndef MY_ENDIAN_H
#define MY_ENDIAN_H

enum
{
  O32_LITTLE_ENDIAN = 0x03020100ul,
  O32_BIG_ENDIAN = 0x00010203ul,
  O32_PDP_ENDIAN = 0x01000302ul
};

static inline uint16_t swap16(uint16_t x) {
  return (x>>8 & 0xff) | (x<<8 & 0xff00);
}

static inline uint32_t swap32(uint32_t x) {
    x = ((x << 8) & 0xFF00FF00) | ((x >> 8) & 0xFF00FF);
    return (x << 16) | (x >> 16);
}

static inline uint64_t swap64(uint64_t x) {
    x = ((x << 8) & 0xFF00FF00FF00FF00ULL) |
	((x >> 8) & 0x00FF00FF00FF00FFULL);
    x = ((x << 16) & 0xFFFF0000FFFF0000ULL) |
	((x >> 16) & 0x0000FFFF0000FFFFULL);
    return (x << 32) | (x >> 32);
}

#define	ENDIAN	O32_LITTLE_ENDIAN

/* Convert numbers to or from little/big endian to host-endian */
#define	swaple16(n)	(n)
#define	swaple32(n)	(n)
#define	swaple64(n)	(n)
#define	swapbe16(n)	(swap16(n))
#define	swapbe32(n)	(swap32(n))
#define	swapbe64(n)	(swap64(n))

#endif	/* MY_ENDIAN_H */
