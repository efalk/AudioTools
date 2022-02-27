/**
 * @file
 * Tiny build-time tool to determine the endian nature
 * of the host system. Probably useless for cross-compiling.
 *
 * If you need to know system endian order in a cross-compiled
 * environment, you're going to have to bite the bullet and figure
 * out which header file(s) will give you what you need.
 */

#include <stdio.h>
#include <stdint.h>

enum
{
  O32_LITTLE_ENDIAN = 0x03020100ul,
  O32_BIG_ENDIAN = 0x00010203ul,
  O32_PDP_ENDIAN = 0x01000302ul
};

static const char boilerPlate1[] =
  "#ifndef MY_ENDIAN_H\n"
  "#define MY_ENDIAN_H\n\n"
  "enum\n"
  "{\n"
  "  O32_LITTLE_ENDIAN = 0x03020100ul,\n"
  "  O32_BIG_ENDIAN = 0x00010203ul,\n"
  "  O32_PDP_ENDIAN = 0x01000302ul\n"
  "};\n\n"
  "static inline uint16_t swap16(uint16_t x) {\n"
  "  return (x>>8 & 0xff) | (x<<8 & 0xff00);\n"
  "}\n"
  "\n"
  "static inline uint32_t swap32(uint32_t x) {\n"
  "    x = ((x << 8) & 0xFF00FF00) | ((x >> 8) & 0xFF00FF);\n"
  "    return (x << 16) | (x >> 16);\n"
  "}\n"
  "\n"
  "static inline uint64_t swap64(uint64_t x) {\n"
  "    x = ((x << 8) & 0xFF00FF00FF00FF00ULL) |\n"
  "	((x >> 8) & 0x00FF00FF00FF00FFULL);\n"
  "    x = ((x << 16) & 0xFFFF0000FFFF0000ULL) |\n"
  "	((x >> 16) & 0x0000FFFF0000FFFFULL);\n"
  "    return (x << 32) | (x >> 32);\n"
  "}\n\n"
  ;

static const char hostle[] =
  "/* Convert numbers to or from little/big endian to host-endian */\n"
  "#define	swaple16(n)	(n)\n"
  "#define	swaple32(n)	(n)\n"
  "#define	swaple64(n)	(n)\n"
  "#define	swapbe16(n)	(swap16(n))\n"
  "#define	swapbe32(n)	(swap32(n))\n"
  "#define	swapbe64(n)	(swap64(n))\n"
  ;

static const char hostbe[] =
  "/* Convert numbers to or from little/big endian to host-endian */\n"
  "#define	swaple16(n)	(swap16(n))\n"
  "#define	swaple32(n)	(swap32(n))\n"
  "#define	swaple64(n)	(swap64(n))\n"
  "#define	swapbe16(n)	(n)\n"
  "#define	swapbe32(n)	(n)\n"
  "#define	swapbe64(n)	(n)\n"
  ;

static const char boilerPlate3[] =
  "\n#endif	/* MY_ENDIAN_H */\n";

int
main()
{
    static const union { unsigned char bytes[4]; uint32_t value; }
	o32_host_order = { { 0, 1, 2, 3 } };
    uint32_t endian = o32_host_order.value;

    printf(boilerPlate1);
    switch (endian) {
      case O32_LITTLE_ENDIAN:
	printf("#define	ENDIAN	O32_LITTLE_ENDIAN\n\n");
	break;
      case O32_BIG_ENDIAN:
	printf("#define	ENDIAN	O32_BIG_ENDIAN\n\n");
	break;
      case O32_PDP_ENDIAN:
	printf("#define	ENDIAN	O32_PDP_ENDIAN\n\n");
	break;
      default:
	printf("#define	ENDIAN	%#x	/* Unknown byte order */\n\n",
	    endian);
	break;
    }
    switch (endian) {
      case O32_LITTLE_ENDIAN:
	printf(hostle);
	break;
      case O32_BIG_ENDIAN:
	printf(hostbe);
	break;
      default:
	printf("/* Unsupported byte order for */\n");
	break;
    }
    printf(boilerPlate3);
    return 0;
}
