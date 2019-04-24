
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>
#include <err.h>

#include "libwav.h"

/* Inline functions and macros */

static inline uint32_t
readUInt32(void *buffer)
{
    uint8_t *bytes = buffer;
    return bytes[0] | bytes[1]<<8 | bytes[2] << 16 | bytes[3] << 24;
}

static inline uint16_t
readUInt16(void *buffer)
{
    uint8_t *bytes = buffer;
    return bytes[0] | bytes[1]<<8;
}

static inline void
writeUInt32(void *buffer, uint32_t val)
{
    uint8_t *bytes = buffer;
    bytes[0] = val & 0xff;
    bytes[1] = (val>>8) & 0xff;
    bytes[2] = (val>>16) & 0xff;
    bytes[3] = (val>>24) & 0xff;
}

static inline void
writeUInt16(void *buffer, uint16_t val)
{
    uint8_t *bytes = buffer;
    bytes[0] = val & 0xff;
    bytes[1] = (val>>8) & 0xff;
}

#define	NA(a)	(sizeof(a)/sizeof(a[0]))

/* Internal type definitions */

struct chunk_type {
    const char *tag, *description;
    Chunk *(*reader)(FILE *, uint32_t, const char *tag, const struct chunk_type *, uint32_t);
    void (*writer)(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
};

typedef	const struct chunk_type ChunkType;

/* Forward references */

static Chunk *readChunk(FILE *, uint32_t offset);
static Chunk *readList(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
static Chunk *readFmt(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
static Chunk *readData(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
#if 0
static Chunk *readCues(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
static Chunk *readLabl(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
static Chunk *readLtxt(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
static Chunk *readId3(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
#endif
static Chunk *readText(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
//static Chunk *readInt16(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);
static Chunk *readInt32(FILE *, uint32_t offset, const char *, ChunkType *, uint32_t);

static void writeWave(WaveChunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeChunk(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeList(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeFmt(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeData(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
#if 0
static void writeCues(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeLabl(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeLtxt(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeId3(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
#endif
static void writeText(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
//static void writeInt16(Chunk *, FILE *src, FILE *dst, uint32_t *offset);
static void writeInt32(Chunk *, FILE *src, FILE *dst, uint32_t *offset);


const char *WaveError;


	/*** READ WAV FILE */

WaveChunk *
OpenWaveFile(FILE *ifile)
{
    WaveChunk *rval = NULL;
    char buffer[1024];
    uint32_t fileLen;
    uint32_t offset;
    Chunk *child;
    Chunk **children;

    if (fread(buffer, 1, 12, ifile) != 12) {
	WaveError = "Short file";
	goto exit;
    }

    if (memcmp(buffer, "RIFF", 4) != 0) {
	WaveError = "File does not seem to be a RIFF file";
	goto exit;
    }
    fileLen = readUInt32(buffer+4);

    rval = (WaveChunk *)newChunk(buffer, fileLen, 0, sizeof(*rval));
    if (rval == NULL) {
	WaveError = "Out of memory in OpenWaveFile";
	goto exit;
    }

    memcpy(rval->type, buffer+8, 4);
    rval->children = NULL;

    children = &rval->children;

    offset = 12;
    while (offset < fileLen) {
	child = readChunk(ifile, offset);
	if (child == NULL) {
	    /* Return with what we have so far */
	    goto exit;
	}
	*children = child;
	children = &child->next;
	offset += 8 + child->length;
    }

exit:
    return rval;
}

/* List of chunk keys and the function to read them */
static ChunkType chunkTypes[] = {
    {"LIST", "List", readList, writeList},
    {"data", "Audio data", readData, writeData},
    {"fmt ", "Format", readFmt, writeFmt},
    {"IARL", "Archival location", readText, writeText},
    {"IART", "Artist", readText, writeText},
    {"ICMS", "Commissioned", readText, writeText},
    {"ICMT", "Comments", readText, writeText},
    {"ICOP", "Copyright", readText, writeText},
    {"ICRD", "Creation date", readText, writeText},
    {"ICRP", "Cropped", readText, writeText},
    {"IDIM", "Dimensions", readText, writeText},
    {"IDPI", "Dots Per Inch", readText, writeText},
    {"IENG", "Engineer", readText, writeText},
    {"IGNR", "Genre", readText, writeText},
    {"IKEY", "Keywords", readText, writeText},
    {"ILGT", "Lightness", readText, writeText},
    {"IMED", "Medium", readText, writeText},
    {"INAM", "Name", readText, writeText},
    {"IPLT", "Palette Setting", readText, writeText},
    {"IPRD", "Product", readText, writeText},
    {"ISBJ", "Subject", readText, writeText},
    {"ISFT", "Software", readText, writeText},
    {"ISHP", "Sharpness", readText, writeText},
    {"ISRC", "Source", readText, writeText},
    {"ISRF", "Source Form", readText, writeText},
    {"ITCH", "Technician", readText, writeText},
    {"ITRK", "Track", readText, writeText},
    {"fact", "Samples", readInt32, writeInt32},
    {"slnt", "Silence", readInt32, writeInt32},
#if 0
    {"cue ", "Cues", readCues, writeCues},
    {"labl", "Label", readLabl, writeLabl},
    {"note", "Note", readLabl, writeLabl},
    {"ltxt", "?", readLtxt, writeLtxt},
    {"id3 ", "ID3 data", readId3, writeId3},
#endif
};

static Chunk *
readChunk(FILE *ifile, uint32_t offset)
{
    Chunk *chunk = NULL;
    char buffer[8];
    uint32_t chunkLen;
    int i;

    if (fseek(ifile, (long)offset, SEEK_SET) != 0) {
	WaveError = "OpenWaveFile: fseek failed";
	goto exit;
    }
    if (fread(buffer, 1, 8, ifile) != 8) {
	WaveError = "Premature end of file";
	goto exit;
    }

    /* The chunk type at the start of the buffer will determine the
     * function to read it in.
     */

    chunkLen = readUInt32(buffer+4);

    for (i=0; i < NA(chunkTypes); ++i) {
	if (strncasecmp(buffer, chunkTypes[i].tag, 4) == 0) {
	    chunk = chunkTypes[i].reader(ifile, offset, buffer, &chunkTypes[i], chunkLen);
	    break;
	}
    }
    if (chunk == NULL) {
	/* Unknown chunk type, return a generic chunk. We read
	 * the header, but leave the data in the file.
	 */
	chunk = readData(ifile, offset, buffer, NULL, chunkLen);
    }

exit:
    return chunk;
}

/**
 * Read a list chunk. Caller has already read the header, so the file
 * is positioned at the type field.
 */
static Chunk *
readList(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    Chunk *chunk = NULL;
    ListChunk *lc;
    char buffer[8];
    uint32_t off2 = 0;
    Chunk *child, **children;

    if (fread(buffer, 1, 4, ifile) != 4) {
	WaveError = "Premature end of file reading LIST";
	goto exit;
    }

    if ((chunk = newChunk(tag, chunkLen, offset, sizeof(*lc))) == NULL) {
	goto exit;
    }
    lc = (ListChunk *)chunk;
    lc->children = NULL;
    memcpy(lc->type, buffer, 4);
    children = &lc->children;

    offset += 8;
    off2 += 4;

    while (off2 < chunkLen) {
	child = readChunk(ifile, offset + off2);
	if (child == NULL) {
	    /* Return with what we have so far */
	    goto exit;
	}
	*children = child;
	children = &child->next;
	off2 += 8 + child->length;
    }

exit:
    return chunk;
}

/**
 * Read a chunk containing file format info
 */
static Chunk *
readFmt(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    Chunk *chunk = NULL;
    FmtChunk *fc;
    uint8_t buffer[16];

    if (fread(buffer, 1, 16, ifile) != 16) {
	WaveError = "Short file";
	goto exit;
    }

    if ((chunk = newChunk(tag, chunkLen, offset, sizeof(*fc))) == NULL) {
	goto exit;
    }
    fc = (FmtChunk *)chunk;

    fc->type = readUInt16(buffer);
    fc->channels = readUInt16(buffer+2);
    fc->sample_rate = readUInt32(buffer+4);
    fc->bytes_sec = readUInt32(buffer+8);
    fc->block_align = readUInt16(buffer+12);
    fc->bits_samp = readUInt16(buffer+14);

exit:
    return chunk;
}

/**
 * Read an audio data chunk. This is the big one. We don't
 * actually read the data, we just make a note of where it is
 * in the file.
 */
static Chunk *
readData(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    Chunk *chunk = NULL;
    DataChunk *dc;

    if ((chunk = newChunk(tag, chunkLen, offset, sizeof(*dc))) == NULL) {
	goto exit;
    }
    dc = (DataChunk *)chunk;

    dc->data = NULL;

exit:
    return chunk;
}

#if 0
static Chunk *
readCues(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    return NULL;
}

static Chunk *
readLabl(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    return NULL;
}

static Chunk *
readLtxt(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    return NULL;
}

static Chunk *
readId3(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    return NULL;
}
#endif

/**
 * Read a text that contains a single string
 */
static Chunk *
readText(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    Chunk *chunk = NULL;
    TextChunk *tc;

    if ((chunk = newChunk(tag, chunkLen, offset, sizeof(*chunk)+chunkLen)) == NULL) {
	goto exit;
    }
    tc = (TextChunk *)chunk;

    if (fread(tc->string, 1, chunkLen, ifile) != chunkLen) {
	WaveError = "Short file";
	goto exit;
    }

exit:
    return chunk;
}

#if 0
static Chunk *
readInt16(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    return NULL;
}
#endif

/**
 * Read a chunk that holds a single 32-bit integer value
 */
static Chunk *
readInt32(FILE *ifile, uint32_t offset, const char *tag, ChunkType *chunkType, uint32_t chunkLen)
{
    Chunk *chunk = NULL;
    IntChunk *ic;
    char buffer[4];

    if ((chunk = newChunk(tag, chunkLen, offset, sizeof(*ic))) == NULL) {
	goto exit;
    }
    ic = (IntChunk *)chunk;

    if (fread(buffer, 1, 4, ifile) != 4) {
	WaveError = "Short file";
	goto exit;
    }
    ic->n = readUInt32(buffer);

exit:
    return chunk;
}


	/*** WRITE WAV FILE */

static void computeSizes(Chunk *);

void
WriteWaveFile(WaveChunk *wave, FILE *src, FILE *dst)
{
    uint32_t offset = 0;

    /* Recurse through all of the data structures, writing
     * to the output file. We need to recompute the
     * offset as we go, replacing the offsets in the chunk
     * headers.
     */

    computeSizes((Chunk *)wave);
    writeWave(wave, src, dst, &offset);
}

/**
 * Compute the length value of this chunk.
 */
static void
computeSizes(Chunk *chunk)
{
    Chunk *child = NULL;
    uint32_t length = 0;
    /* The vast majority of the time, this value is already
     * in the header and doesn't need to be changed.
     * The exception is wave headers and list headers,
     * which need to sum up their children.
     */
    if (strncasecmp(chunk->identifier, "riff", 4) == 0 ||
        strncasecmp(chunk->identifier, "list", 4) == 0)
    {
	ListChunk *lc = (ListChunk *)chunk;
	child = lc->children;
	length = 4;
    }

    if (child != NULL)
    {
	for (; child != NULL; child = child->next) {
	    computeSizes(child);
	    length += child->length + 8;
	}
	chunk->length = length;
    }
}


static void
writeWave(WaveChunk *wave, FILE *src, FILE *dst, uint32_t *offset)
{
    Chunk *child;
    uint8_t buffer[12];

    memcpy(buffer, wave->header.identifier, 4);
    writeUInt32(buffer+4, wave->header.length);
    memcpy(buffer+8, wave->type, 4);
    fwrite(buffer, 1, sizeof(buffer), dst);
    *offset += sizeof(buffer);

    for (child = wave->children; child != NULL; child = child->next)
    {
	writeChunk(child, src, dst, offset);
    }

}

static void
writeChunk(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
    int i;

    for (i=0; i < NA(chunkTypes); ++i) {
	if (strncasecmp(chunk->identifier, chunkTypes[i].tag, 4) == 0) {
	    chunkTypes[i].writer(chunk, src, dst, offset);
	    return;
	}
    }

    /* Unknown chunk type, return a generic chunk. We read
     * the header, but leave the data in the file.
     */
    writeData(chunk, src, dst, offset);
}

static void
writeList(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
    ListChunk *lc = (ListChunk *)chunk;
    Chunk *child;
    char buffer[12];

    memcpy(buffer, chunk->identifier, 4);
    writeUInt32(buffer+4, chunk->length);
    memcpy(buffer+8, lc->type, 4);
    fwrite(buffer, 1, sizeof(buffer), dst);
    *offset += sizeof(buffer);
    for (child = lc->children; child != NULL; child = child->next)
    {
	writeChunk(child, src, dst, offset);
    }
}

static void
writeFmt(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
    FmtChunk *fc = (FmtChunk *)chunk;
    char buffer[24];

    memcpy(buffer, chunk->identifier, 4);
    writeUInt32(buffer+4, 16);

    writeUInt16(buffer+8, fc->type);
    writeUInt16(buffer+10, fc->channels);
    writeUInt32(buffer+12, fc->sample_rate);
    writeUInt32(buffer+16, fc->bytes_sec);
    writeUInt16(buffer+20, fc->block_align);
    writeUInt16(buffer+22, fc->bits_samp);

    fwrite(buffer, 1, sizeof(buffer), dst);
    *offset += sizeof(buffer);
}

/**
 * Write a data chunk. This is the only write function that
 * references the source file, and only if the "data" field
 * is NULL.
 */
static void
writeData(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
    DataChunk *dc = (DataChunk *)chunk;
    char buffer[8];

    memcpy(buffer, chunk->identifier, 4);
    writeUInt32(buffer+4, chunk->length);
    fwrite(buffer, 1, sizeof(buffer), dst);
    *offset += sizeof(buffer);

    if (dc->data != NULL)
    {
	fwrite(dc->data, 1, chunk->length, dst);
    }
    else
    {
	/* Copy from src => dst */
	char buf2[1024];
	size_t len = (size_t) chunk->length;
	size_t l;
	fseek(src, (long)(chunk->offset+8), SEEK_SET);
	while (len > 0) {
	    l = len > sizeof(buf2) ? sizeof(buf2) : len;
	    l = fread(buf2, 1, l, src);
	    if (l == 0) {
		fprintf(stderr, "Error reading data from source file, %s\n",
		    strerror(errno));
		break;
	    }
	    fwrite(buf2, 1, l, dst);
	    len -= l;
	}
    }
    *offset += chunk->length;
}

#if 0
static void
writeCues(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
}

static void
writeLabl(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
}

static void
writeLtxt(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
}

static void
writeId3(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
}
#endif

static void
writeText(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
    TextChunk *tc = (TextChunk *)chunk;
    char buffer[8];

    memcpy(buffer, chunk->identifier, 4);
    writeUInt32(buffer+4, chunk->length);

    fwrite(buffer, 1, sizeof(buffer), dst);
    fwrite(tc->string, 1, chunk->length, dst);
    *offset += sizeof(buffer) + chunk->length;
}

#if 0
static void
writeInt16(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
}
#endif

static void
writeInt32(Chunk *chunk, FILE *src, FILE *dst, uint32_t *offset)
{
    IntChunk *ic = (IntChunk *)chunk;
    char buffer[12];

    memcpy(buffer, chunk->identifier, 4);
    writeUInt32(buffer+4, 4);
    writeUInt32(buffer+8, ic->n);

    fwrite(buffer, 1, sizeof(buffer), dst);
    *offset += sizeof(buffer);
}


	/*** UTILITIES ***/

/**
 * Allocate the space for a chunk, and initialize the header
 */
Chunk *
newChunk(const char *tag, uint32_t length, uint32_t offset, size_t size)
{
    Chunk *chunk;

    if ((chunk = malloc(size)) == NULL) {
	WaveError = "Out of memory";
	goto exit;
    }
    memcpy(chunk->identifier, tag, 4);
    chunk->length = length;
    chunk->offset = offset;
    chunk->next = NULL;

exit:
    return chunk;
}
