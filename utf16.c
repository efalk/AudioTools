/**
 * @file
 * UTF16 => wchar_t conversion
 */

#include <wchar.h>
#include <stdint.h>

#include "myendian.h"


void
utf16LE_wchar(uint16_t *in, wchar_t *out, int len)
{
    uint16_t c, c2;

    while (--len >= 0) {
	c = swaple16(*in++);
	if (c < 0xD800 || c >= 0xE00) {
	    *out++ = c;
	} else {
	    c2 = swaple16(*in++); --len;
	    *out++ = (c & 0x3ff) | (c2 & 0x3ff);
	}
    }
}

void
utf16BE_wchar(uint16_t *in, wchar_t *out, int len)
{
    uint16_t c, c2;

    while (--len >= 0) {
	c = swapbe16(*in++);
	if (c < 0xD800 || c >= 0xE00) {
	    *out++ = c;
	} else {
	    c2 = swapbe16(*in++); --len;
	    *out++ = (c & 0x3ff) | (c2 & 0x3ff);
	}
    }
}

void
utf16BOM_wchar(uint16_t *in, wchar_t *out, int len)
{
    uint16_t bom;

    bom = *in++; --len;
    if (bom == 0xFEFF)
	return utf16BE_wchar(in, out, len);
    else
	return utf16LE_wchar(in, out, len);
}
