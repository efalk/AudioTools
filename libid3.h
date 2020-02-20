#ifndef	LIBID3_H
#define	LIBID3_H

#include <stdio.h>
#include <stdint.h>

#define	ID3_HEADER_SIZE	10	/* id3 header without extra info */
#define	ID3_FRAME_SIZE	10	/* id3 frame header without extra info */

/**
 * Data from the Id3v2 tag header
 */
typedef struct id3v2 {
  char identifier[3];	/* "ID3" */
  uint8_t major, minor;	/* e.g. {3,0} for v2.3.0 */
  uint8_t flags;
  uint32_t size;	/* # of all frames that follow */
  struct frame *frames;
  FILE *file;
  off_t offset;
  uint8_t data[];
} Id3V2;

/**
 * Generic frame. The header is decoded and stored in this
 * structure. If the frame is an unsupported type, the
 * raw data follows immediately.
 */
typedef struct frame {
  char identifier[4];	/* e.g. "TALB" */
  long length;		/* Length of the data that follows (SS) */
  int flags;		/* See below */
  off_t offset;
  struct frame *next;
} Frame;

/* (SS) indicates a "syncsafe" integer. These are 28-bit values
 * encoded as 0xxxxxxx0xxxxxxx0xxxxxxx0xxxxxxx in the file. These
 * Are ordinary integers in these data structures.
 */

/* 2.3.0 flags, see http://id3.org/id3v2.3.0 */
#define	ID3v23_TAG_ALTER	0x8000
#define	ID3v23_FILE_ALTER	0x4000
#define	ID3v23_READONLY		0x2000
#define	ID3v23_COMPRESSION	0x80	/* Adds 4-byte uncompressed size */
#define	ID3v23_ENCRYPTION	0x40	/* Adds 1-byte encryption method */
#define	ID3v23_GROUPING		0x20	/* Adds 1-byte group identifier */

/* 2.4.0 flags */
#define	ID3v24_TAG_ALTER	0x4000
#define	ID3v24_FILE_ALTER	0x2000
#define	ID3v24_READONLY		0x1000
#define	ID3v24_GROUPING		0x40
#define	ID3v24_COMPRESSION	0x8
#define	ID3v24_ENCRYPTION	0x4
#define	ID3v24_UNSYNC		0x2
#define	ID3v24_DATA_LEN		0x1

/* Frames defined in v2.3:
 * 4.20    AENC    [[#sec4.20|Audio encryption]]
 * 4.15    APIC    [#sec4.15 Attached picture]
 * 4.11    COMM    [#sec4.11 Comments]
 * 4.25    COMR    [#sec4.25 Commercial frame]
 * 4.26    ENCR    [#sec4.26 Encryption method registration]
 * 4.13    EQUA    [#sec4.13 Equalization]
 * 4.6     ETCO    [#sec4.6 Event timing codes]
 * 4.16    GEOB    [#sec4.16 General encapsulated object]
 * 4.27    GRID    [#sec4.27 Group identification registration]
 * 4.4     IPLS    [#sec4.4 Involved people list]
 * 4.21    LINK    [#sec4.21 Linked information]
 * 4.5     MCDI    [#sec4.5 Music CD identifier]
 * 4.7     MLLT    [#sec4.7 MPEG location lookup table]
 * 4.24    OWNE    [#sec4.24 Ownership frame]
 * 4.28    PRIV    [#sec4.28 Private frame]
 * 4.17    PCNT    [#sec4.17 Play counter]
 * 4.18    POPM    [#sec4.18 Popularimeter]
 * 4.22    POSS    [#sec4.22 Position synchronisation frame]
 * 4.19    RBUF    [#sec4.19 Recommended buffer size]
 * 4.12    RVAD    [#sec4.12 Relative volume adjustment]
 * 4.14    RVRB    [#sec4.14 Reverb]
 * 4.10    SYLT    [#sec4.10 Synchronized lyric/text]
 * 4.8     SYTC    [#sec4.8 Synchronized tempo codes]
 * 4.2.1   TALB    [#TALB Album/Movie/Show title]
 * 4.2.1   TBPM    [#TBPM BPM (beats per minute)]
 * 4.2.1   TCOM    [#TCOM Composer]
 * 4.2.1   TCON    [#TCON Content type]
 * 4.2.1   TCOP    [#TCOP Copyright message]
 * 4.2.1   TDAT    [#TDAT Date]
 * 4.2.1   TDLY    [#TDLY Playlist delay]
 * 4.2.1   TENC    [#TENC Encoded by]
 * 4.2.1   TEXT    [#TEXT Lyricist/Text writer]
 * 4.2.1   TFLT    [#TFLT File type]
 * 4.2.1   TIME    [#TIME Time]
 * 4.2.1   TIT1    [#TIT1 Content group description]
 * 4.2.1   TIT2    [#TIT2 Title/songname/content description]
 * 4.2.1   TIT3    [#TIT3 Subtitle/Description refinement]
 * 4.2.1   TKEY    [#TKEY Initial key]
 * 4.2.1   TLAN    [#TLAN Language(s)]
 * 4.2.1   TLEN    [#TLEN Length]
 * 4.2.1   TMED    [#TMED Media type]
 * 4.2.1   TOAL    [#TOAL Original album/movie/show title]
 * 4.2.1   TOFN    [#TOFN Original filename]
 * 4.2.1   TOLY    [#TOLY Original lyricist(s)/text writer(s)]
 * 4.2.1   TOPE    [#TOPE Original artist(s)/performer(s)]
 * 4.2.1   TORY    [#TORY Original release year]
 * 4.2.1   TOWN    [#TOWN File owner/licensee]
 * 4.2.1   TPE1    [#TPE1 Lead performer(s)/Soloist(s)]
 * 4.2.1   TPE2    [#TPE2 Band/orchestra/accompaniment]
 * 4.2.1   TPE3    [#TPE3 Conductor/performer refinement]
 * 4.2.1   TPE4    [#TPE4 Interpreted, remixed, or otherwise modified by]
 * 4.2.1   TPOS    [#TPOS Part of a set]
 * 4.2.1   TPUB    [#TPUB Publisher]
 * 4.2.1   TRCK    [#TRCK Track number/Position in set]
 * 4.2.1   TRDA    [#TRDA Recording dates]
 * 4.2.1   TRSN    [#TRSN Internet radio station name]
 * 4.2.1   TRSO    [#TRSO Internet radio station owner]
 * 4.2.1   TSIZ    [#TSIZ Size]
 * 4.2.1   TSRC    [#TSRC ISRC (international standard recording code)]
 * 4.2.1   TSSE    [#TSEE Software/Hardware and settings used for encoding]
 * 4.2.1   TYER    [#TYER Year]
 * 4.2.2   TXXX    [#TXXX User defined text information frame]
 * 4.1     UFID    [#sec4.1 Unique file identifier]
 * 4.23    USER    [#sec4.23 Terms of use]
 * 4.9     USLT    [#sec4.9 Unsychronized lyric/text transcription]
 * 4.3.1   WCOM    [#WCOM Commercial information]
 * 4.3.1   WCOP    [#WCOP Copyright/Legal information]
 * 4.3.1   WOAF    [#WOAF Official audio file webpage]
 * 4.3.1   WOAR    [#WOAR Official artist/performer webpage]
 * 4.3.1   WOAS    [#WOAS Official audio source webpage]
 * 4.3.1   WORS    [#WORS Official internet radio station homepage]
 * 4.3.1   WPAY    [#WPAY Payment]
 * 4.3.1   WPUB    [#WPUB Publishers official webpage]
 * 4.3.2   WXXX    [#WXXX User defined URL link frame]
 */


/**
 * The tags that start with 'T' are text frames and they all have
 * this format.
 */
typedef struct text_frame {
    struct frame header;
    uint8_t encoding;
    uint8_t string[];		/* Encoded string */
} TextFrame;

#define	ID3_ENCODING_LATIN1	0
#define	ID3_ENCODING_UTF_16BOM	1
#define	ID3_ENCODING_UTF_16BE	2
#define	ID3_ENCODING_UTF_8	3

typedef struct url_frame {
    struct frame header;
    uint8_t data[];
} UrlFrame;

typedef struct picture_frame {
    struct frame header;
    uint8_t encoding;
    uint8_t data[];
} PictureFrame;


#ifdef	__cplusplus
extern	"C"
{
#endif

extern const char *Id3v2Error;

/**
 * Parse Id3V2 data.
 * @param ifile   file to read ID3 data from
 * @param offset  location in file where ID3 data begins
 */
extern	Id3V2 *ReadId3V2(FILE *ifile, off_t offset);

/**
 * Write Id3V2 data
 * @param src  source file, for those tags that need to be copied
 * @param dst  file to write
 * @param id3  pointer to Id3V2 data
 * @return total bytes written
 */
extern int WriteId3V2(FILE *src, FILE *ofile, Id3V2 *id3);

/**
 * Allocate and initialize an empty Id3V2 header structure.
 */
extern Id3V2 *NewId3V2(void);

#ifdef	__cplusplus
}
#endif

#endif /* LIBWAV_H */
