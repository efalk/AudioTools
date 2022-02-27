/**
 * @file
 * UTF16 => wchar_t conversion
 */

#include <wchar.h>
#include <stdint.h>

#include "myendian.h"


int
utf16LE_wchar(uint16_t *in, wchar_t *out, int len)
{
    uint16_t c, c2;
    int olen = 0;

    while (--len >= 0) {
	c = swaple16(*in++);
	if (c < 0xD800 || c > 0xDFFF) {
	    *out++ = c;
            ++olen;
	} else {
	    c2 = swaple16(*in++); --len;
	    *out++ = ((c & 0x3ff)<<10) | (c2 & 0x3ff);
            ++olen;
	}
    }
    return olen;
}

int
utf16BE_wchar(uint16_t *in, wchar_t *out, int len)
{
    uint16_t c, c2;
    int olen = 0;

    while (--len >= 0) {
	c = swapbe16(*in++);
	if (c < 0xD800 || c > 0xDFFF) {
	    *out++ = c;
            ++olen;
	} else {
	    c2 = swapbe16(*in++); --len;
	    *out++ = ((c & 0x3ff)<<10) | (c2 & 0x3ff);
            ++olen;
	}
    }
    return olen;
}

int
utf16BOM_wchar(uint16_t *in, wchar_t *out, int len)
{
    uint16_t bom;

    bom = *in++; --len;
    if (bom == 0xFEFF)
	return utf16BE_wchar(in, out, len);
    else
	return utf16LE_wchar(in, out, len);
}
