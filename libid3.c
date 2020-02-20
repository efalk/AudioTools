
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

#include "libid3.h"

/* Internal type definitions */

struct frame_type {
    const char *tag, *description;
    int (*reader)(FILE *ifile, off_t offset, uint8_t *buffer, Frame **rframe);
    int (*writer)(FILE *src, FILE *dst, Frame *frame);
};

typedef	const struct frame_type FrameType;

/* Inline functions and macros */

/**
 * Read a sync-safe value. Assume the value is well-formed.
 */
static inline uint32_t
readSyncSafe(void *buffer)
{
    uint8_t *bytes = buffer;
    return bytes[0]<<21 | bytes[1]<<15 | bytes[2] << 7 | bytes[3];
}

static inline uint16_t
readUInt16(void *buffer)
{
    uint8_t *bytes = buffer;
    return bytes[0]<<8 | bytes[1];
}

static inline void
writeSyncSafe(void *buffer, uint32_t val)
{
    uint8_t *bytes = buffer;
    bytes[0] = (val>>21) & 0x7f;
    bytes[1] = (val>>15) & 0x7f;
    bytes[2] = (val>>7) & 0x7f;
    bytes[3] = val & 0x7f;
}

static inline void
writeUInt16(void *buffer, uint16_t val)
{
    uint8_t *bytes = buffer;
    bytes[0] = (val>>8) & 0xff;
    bytes[1] = val & 0xff;
}

#define	NA(a)		(sizeof(a)/sizeof(a[0]))
#define	MIN(a,b)	((a)<(b)?(a):(b))

/* Forward references */

static int parseFrame(FILE *ifile, off_t offset, Frame **frame);
static int readFrame(FILE *ifile, off_t offset, uint8_t *buffer, Frame **rframe);
static int readText(FILE *ifile, off_t offset, uint8_t *buffer, Frame **rframe);
static int newFrame(off_t offset, uint8_t *buffer, int size, Frame **rframe);

static int writeFrame(FILE *src, FILE *dst, Frame *frame);
static int writeText(FILE *src, FILE *dst, Frame *frame);

static FrameType *findFrameType(const char *tag);
static int copyFile(FILE *src, off_t offset, FILE *dst, size_t len);
#if 0
static void writeWave(WaveFrame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeFrame(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeList(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeFmt(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeData(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeCues(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeLabl(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeLtxt(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeId3(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeText(Frame *, FILE *src, FILE *dst, uint32_t *offset);
//static void writeInt16(Frame *, FILE *src, FILE *dst, uint32_t *offset);
static void writeInt32(Frame *, FILE *src, FILE *dst, uint32_t *offset);
#endif


/* List of frame keys and the function to read them */
static FrameType frameTypes[] = {
/* v2.3 frames: */
/* Programming note: as readFrame() is the default fallback,
 * we can save time searching this list by not including
 * those frame types.
 */
#if 0
  {"AENC",   "Audio encryption]", readFrame, writeFrame},
  {"APIC",   "Attached picture", readFrame, writeFrame},
  {"COMM",   "Comments", readFrame, writeFrame},
  {"COMR",   "Commercial frame", readFrame, writeFrame},
  {"ENCR",   "Encryption method registration", readFrame, writeFrame},
  {"EQUA",   "Equalization", readFrame, writeFrame},
  {"ETCO",   "Event timing codes", readFrame, writeFrame},
  {"GEOB",   "General encapsulated object", readFrame, writeFrame},
  {"GRID",   "Group identification registration", readFrame, writeFrame},
  {"IPLS",   "Involved people list", readFrame, writeFrame},
  {"LINK",   "Linked information", readFrame, writeFrame},
  {"MCDI",   "Music CD identifier", readFrame, writeFrame},
  {"MLLT",   "MPEG location lookup table", readFrame, writeFrame},
  {"OWNE",   "Ownership frame", readFrame, writeFrame},
  {"PRIV",   "Private frame", readFrame, writeFrame},
  {"PCNT",   "Play counter", readFrame, writeFrame},
  {"POPM",   "Popularimeter", readFrame, writeFrame},
  {"POSS",   "Position synchronisation frame", readFrame, writeFrame},
  {"RBUF",   "Recommended buffer size", readFrame, writeFrame},
  {"RVAD",   "Relative volume adjustment", readFrame, writeFrame},
  {"RVRB",   "Reverb", readFrame, writeFrame},
  {"SYLT",   "Synchronized lyric/text", readFrame, writeFrame},
  {"SYTC",   "Synchronized tempo codes", readFrame, writeFrame},
#endif
  {"TALB",   "Album/Movie/Show title", readText, writeText},
  {"TBPM",   "BPM (beats per minute)", readText, writeText},
  {"TCOM",   "Composer", readText, writeText},
  {"TCON",   "Content type", readText, writeText},
  {"TCOP",   "Copyright message", readText, writeText},
  {"TDAT",   "Date", readText, writeText},
  {"TDLY",   "Playlist delay", readText, writeText},
  {"TENC",   "Encoded by", readText, writeText},
  {"TEXT",   "Lyricist/Text writer", readText, writeText},
  {"TFLT",   "File type", readText, writeText},
  {"TIME",   "Time", readText, writeText},
  {"TIT1",   "Content group description", readText, writeText},
  {"TIT2",   "Title/songname/content description", readText, writeText},
  {"TIT3",   "Subtitle/Description refinement", readText, writeText},
  {"TKEY",   "Initial key", readText, writeText},
  {"TLAN",   "Language(s)", readText, writeText},
  {"TLEN",   "Length", readText, writeText},
  {"TMED",   "Media type", readText, writeText},
  {"TOAL",   "Original album/movie/show title", readText, writeText},
  {"TOFN",   "Original filename", readText, writeText},
  {"TOLY",   "Original lyricist(s)/text writer(s)", readText, writeText},
  {"TOPE",   "Original artist(s)/performer(s)", readText, writeText},
  {"TORY",   "Original release year", readText, writeText},
  {"TOWN",   "File owner/licensee", readText, writeText},
  {"TPE1",   "Lead performer(s)/Soloist(s)", readText, writeText},
  {"TPE2",   "Band/orchestra/accompaniment", readText, writeText},
  {"TPE3",   "Conductor/performer refinement", readText, writeText},
  {"TPE4",   "Interpreted, remixed, or otherwise modified by", readText, writeText},
  {"TPOS",   "Part of a set", readText, writeText},
  {"TPUB",   "Publisher", readText, writeText},
  {"TRCK",   "Track number/Position in set", readText, writeText},
  {"TRDA",   "Recording dates", readText, writeText},
  {"TRSN",   "Internet radio station name", readText, writeText},
  {"TRSO",   "Internet radio station owner", readText, writeText},
  {"TSIZ",   "Size", readText, writeText},
  {"TSRC",   "ISRC (international standard recording code)", readText, writeText},
  {"TSSE",   "Software/Hardware and settings used for encoding", readText, writeText},
  {"TYER",   "Year", readText, writeText},
  {"TXXX",   "User defined text information frame", readText, writeText},
#if 0
  {"UFID",   "Unique file identifier", readFrame, writeFrame},
  {"USER",   "Terms of use", readFrame, writeFrame},
  {"USLT",   "Unsychronized lyric/text transcription", readFrame, writeFrame},
  {"WCOM",   "Commercial information", readFrame, writeFrame},
  {"WCOP",   "Copyright/Legal information", readFrame, writeFrame},
  {"WOAF",   "Official audio file webpage", readFrame, writeFrame},
  {"WOAR",   "Official artist/performer webpage", readFrame, writeFrame},
  {"WOAS",   "Official audio source webpage", readFrame, writeFrame},
  {"WORS",   "Official internet radio station homepage", readFrame, writeFrame},
  {"WPAY",   "Payment", readFrame, writeFrame},
  {"WPUB",   "Publishers official webpage", readFrame, writeFrame},
  {"WXXX",   "User defined URL link frame", readFrame, writeFrame},
#endif

/* v2.4 frames: */
  {"TDRC",   "Recording time", readText, writeText},
};

const char *Id3v2Error;


	/*** READ ID3v2 DATA */

Id3V2 *
ReadId3V2(FILE *ifile, off_t offset)
{
    Id3V2 *header = NULL;
    Frame *frame, **ptr;
    uint8_t buffer[ID3_HEADER_SIZE];
    uint8_t major, minor;
    uint8_t flags;
    uint32_t size;
    ssize_t rem;
    int l;

    if (fseeko(ifile, offset, SEEK_SET) != 0) {
	Id3v2Error = "ParseId3V2: fseek failed";
	goto exit;
    }
    if (fread(buffer, 1, sizeof(buffer), ifile) != sizeof(buffer)) {
	Id3v2Error = "ParseId3V2: read failed";
	goto exit;
    }

    if (memcmp(buffer, "ID3", 3) != 0) {
	Id3v2Error = "Data does not contain ID3 header";
	goto exit;
    }
    major = buffer[3];
    minor = buffer[4];
    if (major != 3) {
	Id3v2Error = "Data is not ID3v2.3";
	goto exit;
    }
    flags = buffer[5];
    size = readSyncSafe(buffer+6);
    header = malloc(sizeof(*header));
    if (header == NULL) {
	Id3v2Error = "Out of memory";
	goto exit;
    }

    memcpy(header->identifier, buffer, 3);
    header->major = major;
    header->minor = minor;
    header->flags = flags;
    header->size = size;
    header->frames = NULL;
    header->file = ifile;
    header->offset = offset;

    offset += ID3_HEADER_SIZE;
    rem = (ssize_t) size;

    ptr = &header->frames;
    while (rem > 0) {
	l = parseFrame(ifile, offset, &frame);
	if (l < 0) {
	    goto exit;
	}
	*ptr = frame;
	ptr = &frame->next;
	offset += l;
	rem -= l;
    }

exit:
    return header;
}

/**
 * Read the header of one frame, determine what to do with
 * the rest of it.
 */
static int
parseFrame(FILE *ifile, off_t offset, Frame **frame)
{
    FrameType *frameType;
    uint8_t buffer[ID3_FRAME_SIZE];
    int l = -1;
    uint32_t size;

    if (fseeko(ifile, offset, SEEK_SET) != 0) {
	Id3v2Error = "ParseId3V2: fseek failed";
	goto exit;
    }
    if (fread(buffer, 1, sizeof(buffer), ifile) != sizeof(buffer)) {
	Id3v2Error = "ParseId3V2: read failed";
	goto exit;
    }
    offset += ID3_FRAME_SIZE;

    /* TODO: depending on flags, there could be additional header
     * data to read. For now, compression, encryption, and grouping
     * are not supported.
     */
    l = ID3_FRAME_SIZE;

    size = readSyncSafe(buffer+4);
    l += size;

    if ((frameType = findFrameType((const char *)buffer)) != NULL) {
	if (frameType->reader(ifile, offset, buffer, frame) < 0) {
	    l = -1;
	}
    } else if (readFrame(ifile, offset, buffer, frame) < 0) {
	l = -1;
    }

exit:
    return l;
}

/**
 * Read a generic frame.
 * The header data has already been read, so we just fill in a header
 * and return. The frame data is not read from the file, we just
 * remember where it can be found.
 */
static int
readFrame(FILE *ifile, off_t offset, uint8_t *buffer, Frame **rframe)
{
    Frame *frame;
    int l;

    l = newFrame(offset, buffer, sizeof(*frame), &frame);
    if (frame == NULL) {
	return -1;
    }

    *rframe = frame;
    return l;
}

/**
 * Read a text frame
 */
static int
readText(FILE *ifile, off_t offset, uint8_t *buffer, Frame **rframe)
{
    Frame *frame;
    TextFrame *tf;
    uint32_t size;
    uint8_t encoding;
    int l;

    size = readSyncSafe(buffer+4);

    if (fread(&encoding, 1, 1, ifile) != 1) {
	Id3v2Error = "ParseId3V2: read failed";
	l = -1;
	goto exit;
    }

    /* Allocate 2 extra bytes to nul-terminate the string */
    l = newFrame(offset, buffer, sizeof(*frame) + size + 2, &frame);
    if (frame == NULL) {
	l = -1;
	goto exit;
    }
    tf = (TextFrame *)frame;
    tf->encoding = encoding;

    if (fread(tf->string, 1, size-1, ifile) != size-1) {
	Id3v2Error = "ParseId3V2: read failed";
	free(frame);
	l = -1;
	goto exit;
    }
    memset(tf->string + size - 1, 0, 2);

    *rframe = frame;
exit:
    return l;
}


	/*** WRITE ID3 ***/

int
WriteId3V2(FILE *src, FILE *dst, Id3V2 *id3)
{
    Frame *frame;
    FrameType *frameType;
    uint8_t buffer[ID3_HEADER_SIZE];

    memcpy(buffer, id3->identifier, 3);
    buffer[3] = id3->major;
    buffer[4] = id3->minor;
    buffer[5] = id3->flags;
    writeSyncSafe(buffer+6, id3->size);
    fwrite(buffer, 1, ID3_HEADER_SIZE, dst);

    for (frame = id3->frames; frame != NULL; frame = frame->next)
    {
	if ((frameType = findFrameType(frame->identifier)) != NULL) {
	    frameType->writer(src, dst, frame);
	} else {
	    writeFrame(src, dst, frame);
	}
    }

    return ID3_HEADER_SIZE + id3->size;
}

/**
 * This is the generic writer of Id3 frames. We only have the header,
 * the rest of the data was left in the source file, so we'll do a
 * file->file copy.
 */
static int
writeFrame(FILE *src, FILE *dst, Frame *frame)
{
    uint8_t buffer[ID3_FRAME_SIZE];
    memcpy(buffer, frame->identifier, 4);
    writeSyncSafe(buffer+4, frame->length);
    writeUInt16(buffer+8, frame->flags);
    if (fwrite(buffer, 1, sizeof(buffer), dst) != sizeof(buffer)) {
	Id3v2Error = "Write failure";
	return -1;
    }
    return copyFile(src, frame->offset, dst, frame->length);
}

/**
 * Write a text frame. The text is in memory following the frame.
 * @return <0 on error
 */
static int
writeText(FILE *src, FILE *dst, Frame *frame)
{
    TextFrame *tf = (TextFrame *)frame;
    uint8_t buffer[ID3_FRAME_SIZE];

    memcpy(buffer, frame->identifier, 4);
    writeSyncSafe(buffer+4, frame->length);
    writeUInt16(buffer+8, frame->flags);
    if (fwrite(buffer, 1, sizeof(buffer), dst) != sizeof(buffer)) {
	Id3v2Error = "Write failure";
	return -1;
    }
    if (fwrite(&tf->encoding, 1, 1, dst) != 1) {
	Id3v2Error = "Write failure";
	return -1;
    }
    return fwrite(tf->string, 1, frame->length-1, dst);
}

	/*** EDITING ***/

Id3V2 *
NewId3V2(void)
{
    Id3V2 *id3;

    if ((id3 = malloc(sizeof *id3)) == NULL) {
	Id3v2Error = "Out of memory";
	return NULL;
    }

    memcpy(id3->identifier, "ID3", 3);
    id3->major = 3;
    id3->minor = 0;
    id3->flags = 0;
    id3->size = 0;
    id3->frames = NULL;
    id3->file = NULL;
    id3->offset = 0;
    return id3;
}

	/*** UTILITIES ***/

static FrameType *
findFrameType(const char *tag)
{
    int i;
    for (i=0; i < NA(frameTypes); ++i) {
	if (strncmp((char *)tag, frameTypes[i].tag, 4) == 0) {
	    return &frameTypes[i];
	}
    }
    return NULL;
}

/**
 * Allocate the space for a frame, and initialize the header
 * Return the amount of data that was consumed to populate
 * the header.
 * @return the number of bytes used in the file for the frame
 */
int
newFrame(off_t offset, uint8_t *buffer, int size, Frame **rframe)
{
    Frame *frame;
    int rval = -1;

    if ((frame = malloc(size)) == NULL) {
	Id3v2Error = "Out of memory";
	goto exit;
    }
    memcpy(frame->identifier, buffer, 4);
    frame->length = readSyncSafe(buffer+4);
    frame->flags = readUInt16(buffer+8);
    frame->offset = offset;
    frame->next = NULL;

    /* TODO: handle the flags that make the header bigger */
    rval = ID3_FRAME_SIZE + frame->length;

exit:
    *rframe = frame;
    return rval;
}


/**
 * Copy data from file to file
 */
static int
copyFile(FILE *src, off_t offset, FILE *dst, size_t len)
{
    char buffer[1024];
    size_t l;

    fseeko(src, offset, SEEK_SET);
    while (len > 0) {
	l = MIN(len, sizeof(buffer));
	l = fread(buffer, 1, l, src);
	if (l == 0) {
	    fprintf(stderr, "Error reading data from source file, %s\n",
		strerror(errno));
	    return -1;
	}
	if (fwrite(buffer, 1, l, dst) < 0) {
	    fprintf(stderr, "Write failed\n");
	    return -1;
	}
	len -= l;
    }
    return 0;
}
