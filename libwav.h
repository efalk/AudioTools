#ifndef	LIBWAV_H
#define	LIBWAV_H

#include <stdio.h>
#include <stdint.h>

typedef struct chunk {
  char identifier[4];	/* e.g. "RIFF" for the top level */
  uint32_t length;	/* Length of the data that follows */
  uint32_t offset;		/* Offset into file of this chunk */
  struct chunk *next;
} Chunk;

typedef struct list_chunk {
  Chunk header;
  char type[4];		/* e.g. "WAVE" or "INFO" */
  Chunk *children;
} WaveChunk, ListChunk;

typedef struct fmt_chunk {
  Chunk header;
  uint16_t type;	/* 1 = pcm, see below */
  uint16_t channels;	/* # of channels */
  uint32_t sample_rate;	/* e.g. 44100 */
  uint32_t bytes_sec;	/* Average bytes/second */
  uint16_t block_align;	/* Channels * bits/sample/8 */
  uint16_t bits_samp;	/* Bits/sample, eg. 8 or 16 */
} FmtChunk;

#define	RIFF_PCM		1
#define	RIFF_MS_ADPCM		2
#define	RIFF_ALAW		6
#define	RIFF_MULAW		7
#define	RIFF_CL_ADPCM		512
#define	RIFF_IMA_ADPCM		17
#define	IBM_FORMAT_MULAW	0x0101
#define	IBM_FORMAT_ALAW	0x0102
#define	IBM_FORMAT_ADPCM	0x0103

typedef struct data_chunk {
  Chunk header;
  void *data;	/* Pointer to raw audio data. If NULL, get the
  		   data from the original file */
} DataChunk;

typedef struct cue {
  uint32_t name;	/* Integer identifier, not a string */
  uint32_t position;
  char fcc_chunk[4];	/* name of chunk containing cue */
  uint32_t chunk_start;
  uint32_t block_start;
  uint32_t sample_offset;
} Cue;

typedef struct cue_chunk {
  Chunk header;
  uint32_t n_cues;
  Cue cues[];
} CueChunk;

typedef struct int_chunk {
  Chunk header;
  uint32_t n;
} IntChunk;

typedef struct int_chunk FactChunk;
typedef struct int_chunk SilenceChunk;

/**
 * Extra data for a cue chunk
 */
typedef struct labl_chunk {
  Chunk header;
  uint32_t name;	/* Matches cue */
  char label[];		/* nul-terminated text */
} LablChunk, NoteChunk;

typedef struct ltxt_chunk {
  Chunk header;
  uint32_t name;	/* Matches cue */
  uint32_t sample_length;	/* # of samples in this segment */
  char purpose[4];	/* e.g. "rgn " to define a region */
  uint16_t country;	/* Country code */
  uint16_t language;	/* Language code */
  uint16_t dialext;	/* Dialect code */
  uint16_t codepage;	/* Text code page */
  uint8_t data[];
} LtxtChunk;

typedef struct file_chunk {
  Chunk header;
  uint32_t name;	/* Matches cue */
  uint32_t mediatype;
  uint8_t data[];
} FileChunk;

/**
 * Generic text chunks
 */
typedef struct text_chunk {
  Chunk header;
  char string[];	/* nul-terminated text */
} TextChunk;

typedef struct text_chunk IarlChunk;	/* Archival location */
typedef struct text_chunk IartChunk;	/* Artist */
typedef struct text_chunk IcmsChunk;	/* Commissioned */
typedef struct text_chunk IcmtChunk;	/* Comments */
typedef struct text_chunk IcopChunk;	/* Copyright */
typedef struct text_chunk IcrdChunk;	/* Creation date */
typedef struct text_chunk IcrpChunk;	/* Cropped */
typedef struct text_chunk IdimChunk;	/* Dimensions */
typedef struct text_chunk IdpiChunk;	/* Dots Per Inch */
typedef struct text_chunk IengChunk;	/* Engineer */
typedef struct text_chunk IgnrChunk;	/* Genre */
typedef struct text_chunk IkeyChunk;	/* Keywords */
typedef struct text_chunk IlgtChunk;	/* Lightness */
typedef struct text_chunk ImedChunk;	/* Medium */
typedef struct text_chunk InamChunk;	/* Name */
typedef struct text_chunk IpltChunk;	/* Palette Setting */
typedef struct text_chunk IprdChunk;	/* Product */
typedef struct text_chunk IsbjChunk;	/* Subject */
typedef struct text_chunk IsftChunk;	/* Software */
typedef struct text_chunk IshpChunk;	/* Sharpness */
typedef struct text_chunk IsrcChunk;	/* Source */
typedef struct text_chunk IsrfChunk;	/* Source Form */
typedef struct text_chunk ItchChunk;	/* Technician */

#ifdef	__cplusplus
extern	"C"
{
#endif

extern const char *WaveError;	/* Error text from last failure */

/**
 * Read the file metadata from the specified file. The
 * actual audio data is not read in, so you'll need to
 * extract it from the open file when the time comes.
 */
extern	WaveChunk *OpenWaveFile(FILE *ifile);

/**
 * Write a new .wav file to dst. The audio data is pulled from src
 * file if the "data" member of the data chunks is NULL. If none of
 * the data chunks are NULL, then src is not required and may be
 * NULL. The rest of the metadata is provided by the structures
 * pointed to by 'wave'.
 */
extern	void	WriteWaveFile(WaveChunk *wave, FILE *src, FILE *dst);

/**
 * Create a new empty chunk.
 */
extern Chunk *newChunk(const char *tag, uint32_t length, uint32_t offset, size_t size);

#ifdef	__cplusplus
}
#endif

#endif /* LIBWAV_H */
