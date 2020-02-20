#ifndef UTF_16_H
#define UTF_16_H


/**
 * UTF16 to wchar_t conversion.
 * @param in   utf-16 string to be converted
 * @param out  wchar_t buffer to receive results
 * @param len  number of words in input
 *
 * Caller is responsible for allocating enough space in the
 * output buffer. Making it the same number of words as the
 * input should suffice.
 *
 * Functions don't add nul termination to the output unless
 * it was present in the input.
 */
extern void utf16LE_wchar(uint16_t *in, wchar_t *out, int len);
extern void utf16BE_wchar(uint16_t *in, wchar_t *out, int len);
extern void utf16BOM_wchar(uint16_t *in, wchar_t *out, int len);

#endif	/* UTF_16_H */
