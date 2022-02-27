static const char usage[] = "usage:\n"
"	wavtags -l file ...\n"
"	wavtags -i file ...\n"
"	wavtags [options] tag=value ... infile outfile\n"
"	wavtags -l\n"
"\n"
"	-h	--help		This list\n"
"	-v	--verbose	Verbose\n"
"	-c	--clear		Clear any existing tags\n"
"	-a	--append	Append tags to list instead of replacing\n"
"	-l	--list		print tags from files and exit\n"
"	-i	--info		Display format info and exit\n"
"	-L	--list-tags	List supported tags and exit\n"
"	-I	--list-id3	List supported id3 tags and exit\n"
"\n"
"Prints or edits the tags from a Microsoft multimedia file, such as .wav\n"
"\n"
"With no tags specified on the command line and no output file, dumps tags\n"
"and exits. If tags are specified, an output file must be specified.\n"
"\n"
"A leading '<' for a tag value takes the value from a named file.\n"
"\n"
"Set a tag to an empty string, e.g. \"isbj=''\" to delete it.\n"
;

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>
#include <err.h>

#include "libwav.h"
#include "utf16.h"

#define	MAX_FILE_TAG_SIZE	50000	/* arbitrary decision */

typedef struct chunk_type {
    const char *tag, *description;
    void (*dumper)(Chunk *, struct chunk_type *);
} ChunkType;

typedef struct frame_type {
    const char *tag, *description;
    void (*dumper)(Frame *, struct frame_type *);
} FrameType;


#define	NA(a)	(sizeof(a)/sizeof(a[0]))

static void dumpFormatFile(const char *filename);
static void dumpTagsFile(const char *filename);
static void dumpChunks(Chunk *list);
static void dumpText(Chunk *chunk, ChunkType *);
static void dumpId3(Chunk *chunk, ChunkType *);
static void listTags(void);
static void listId3Tags(void);
static void dumpFormat(WaveChunk *);
static int modifyTags(WaveChunk *, char **tag_replacements, int n_replacements);
static Chunk *searchFor(Chunk *top, const char *tag, const char *type);
static TextChunk *TextChunkFromString(const char *tag, const char *string);
static TextFrame * TextFrameFromString(const char *tag, const char *string);
static char *readValueFromFile(const char *filename);
static void dumpId3Frame(Frame *frame, FrameType *frameType);
static void dumpId3Text(Frame *frame, FrameType *frameType);
static ChunkType *findChunkType(const char *tag);
static FrameType *findFrameType(const char *tag);


struct option longopts[] = {
  {"help", no_argument, NULL, 'h'},
  {"verbose", no_argument, NULL, 'v'},
  {"clear", no_argument, NULL, 'c'},
  {"append", no_argument, NULL, 'a'},
  {"list", no_argument, NULL, 'l'},
  {"info", no_argument, NULL, 'i'},
  {"list-tags", no_argument, NULL, 'L'},
  {"list-id3", no_argument, NULL, 'I'},
  {0,0,0,0}
};

static int verbose = 0;
static bool clearTags = false;
static bool appendTags = false;
static bool showTags = false;
static bool showInfo = false;


int
main(int argc, char **argv)
{
    WaveChunk *waveFile;
    const char *ifilename, *ofilename;
    FILE *ifile, *ofile;
    int c;
    char **tag_replacements;
    int n_replacements = 0;

    while ((c = getopt_long(argc, argv, "hvcLaIil", longopts, NULL)) != -1)
    {
      switch (c) {
	case 'h': printf(usage); return 0;
	case 'v': verbose++; break;
	case 'L': listTags(); return 0;
	case 'I': listId3Tags(); return 0;
	case 'c': clearTags = true; break;
	case 'a': appendTags = true; break;
	case 'i': showInfo = true; break;
	case 'l': showTags = true; break;
	case '?': fprintf(stderr, usage); return 2;
      }
    }

    if (optind >= argc) {
	fprintf(stderr, "Specify at least one file name\n");
	return 2;
    }

    /* Next, look for any tag=value items in the arguments */
    tag_replacements = argv + optind;
    for (n_replacements=0;
	 optind < argc && strchr(argv[optind], '=') != NULL;
	 ++n_replacements, ++optind)
      ;

    if (optind >= argc) {
	fprintf(stderr, "Specify at least one file name\n");
	return 2;
    }

    if (showInfo) {
	for (; optind < argc; ++optind) {
	    dumpFormatFile(argv[optind]);
	}
	return 0;
    }

    if (showTags) {
	for (; optind < argc; ++optind) {
	    dumpTagsFile(argv[optind]);
	}
	return 0;
    }

    ifilename = argv[optind++];

    ifile = fopen(ifilename, "rb");
    if (ifile == NULL) {
	fprintf(stderr, "Cannot open %s: %s\n",
	    ifilename, strerror(errno));
	return 4;
    }
    waveFile = OpenWaveFile(ifile);
    if (waveFile == NULL) {
	fprintf(stderr, "%s: %s\n", ifilename, WaveError);
	return 4;
    }

    /* If any tags were specified on the command line,
     * and an output file was specified, change the
     * tags. Else, just dump them.
     */
    if (n_replacements == 0 || optind >= argc) {
	dumpChunks(waveFile->children);
    } else {
	ofilename = argv[optind++];
	if (strcasecmp(ifilename, ofilename) == 0) {
	    fprintf(stderr,
		"Input file and output file cannot have the same name\n");
	    return 3;
	}
	ofile = fopen(ofilename, "wb");
	if (ofile == NULL) {
	    fprintf(stderr, "Unable to open %s for write: %s\n",
		ofilename, strerror(errno));
	    return 3;
	}
	if (modifyTags(waveFile, tag_replacements, n_replacements) == 0) {
	    if (verbose) {
		dumpChunks(waveFile->children);
	    }
	    WriteWaveFile(waveFile, ifile, ofile);
	}
    }

    return 0;
}

/* List of chunk keys and the function to process them */
static ChunkType chunkTypes[] = {
    {"IARL", "Archival location", dumpText},
    {"IART", "Artist", dumpText},
    {"ICMS", "Commissioned", dumpText},
    {"ICMT", "Comments", dumpText},
    {"ICOP", "Copyright", dumpText},
    {"ICRD", "Creation date", dumpText},
    {"IENG", "Engineer", dumpText},
    {"IGNR", "Genre", dumpText},
    {"IKEY", "Keywords", dumpText},
    {"IMED", "Medium", dumpText},
    {"INAM", "Name", dumpText},
    {"IPRD", "Product", dumpText},
    {"ISBJ", "Subject", dumpText},
    {"ISFT", "Software", dumpText},
    {"ISRC", "Source", dumpText},
    {"ISRF", "Source Form", dumpText},
    {"ITCH", "Technician", dumpText},
    {"ITRK", "Track", dumpText},
    {"ICRP", "Cropped", dumpText},
    {"IDIM", "Dimensions", dumpText},
    {"IDPI", "Dots Per Inch", dumpText},
    {"ILGT", "Lightness", dumpText},
    {"IPLT", "Palette Setting", dumpText},
    {"ISHP", "Sharpness", dumpText},
    {"ID3 ", "ID3 Tags", dumpId3},
};

static FrameType id3Types[] = {
  {"AENC",   "Audio encryption", dumpId3Frame},
  {"APIC",   "Attached picture", dumpId3Frame},
  {"COMM",   "Comments", dumpId3Frame},
  {"COMR",   "Commercial frame", dumpId3Frame},
  {"ENCR",   "Encryption method registration", dumpId3Frame},
  {"EQUA",   "Equalization", dumpId3Frame},
  {"ETCO",   "Event timing codes", dumpId3Frame},
  {"GEOB",   "General encapsulated object", dumpId3Frame},
  {"GRID",   "Group identification registration", dumpId3Frame},
  {"IPLS",   "Involved people list", dumpId3Frame},
  {"LINK",   "Linked information", dumpId3Frame},
  {"MCDI",   "Music CD identifier", dumpId3Frame},
  {"MLLT",   "MPEG location lookup table", dumpId3Frame},
  {"OWNE",   "Ownership frame", dumpId3Frame},
  {"PRIV",   "Private frame", dumpId3Frame},
  {"PCNT",   "Play counter", dumpId3Frame},
  {"POPM",   "Popularimeter", dumpId3Frame},
  {"POSS",   "Position synchronisation frame", dumpId3Frame},
  {"RBUF",   "Recommended buffer size", dumpId3Frame},
  {"RVAD",   "Relative volume adjustment", dumpId3Frame},
  {"RVRB",   "Reverb", dumpId3Frame},
  {"SYLT",   "Synchronized lyric/text", dumpId3Frame},
  {"SYTC",   "Synchronized tempo codes", dumpId3Frame},
  {"TALB",   "Album/Show", dumpId3Text},
  {"TBPM",   "Beats per minute", dumpId3Text},
  {"TCOM",   "Composer", dumpId3Text},
  {"TCON",   "Content type", dumpId3Text},
  {"TCOP",   "Copyright message", dumpId3Text},
  {"TDAT",   "Date", dumpId3Text},
  {"TDLY",   "Playlist delay", dumpId3Text},
  {"TENC",   "Encoded by", dumpId3Text},
  {"TEXT",   "Lyricist/Text writer", dumpId3Text},
  {"TFLT",   "File type", dumpId3Text},
  {"TIME",   "Time", dumpId3Text},
  {"TIT1",   "Content group description", dumpId3Text},
  {"TIT2",   "Title", dumpId3Text},
  {"TIT3",   "Subtitle", dumpId3Text},
  {"TKEY",   "Initial key", dumpId3Text},
  {"TLAN",   "Language(s)", dumpId3Text},
  {"TLEN",   "Length", dumpId3Text},
  {"TMED",   "Media type", dumpId3Text},
  {"TOAL",   "Original album/movie", dumpId3Text},
  {"TOFN",   "Original filename", dumpId3Text},
  {"TOLY",   "Original lyricist(s)/text writer(s)", dumpId3Text},
  {"TOPE",   "Original artist(s)", dumpId3Text},
  {"TORY",   "Original release year", dumpId3Text},
  {"TOWN",   "File owner/licensee", dumpId3Text},
  {"TPE1",   "Lead performer(s)", dumpId3Text},
  {"TPE2",   "Band", dumpId3Text},
  {"TPE3",   "Conductor", dumpId3Text},
  {"TPE4",   "Interpreted, remixed, or otherwise modified by", dumpId3Text},
  {"TPOS",   "Part of a set", dumpId3Text},
  {"TPUB",   "Publisher", dumpId3Text},
  {"TRCK",   "Track number", dumpId3Text},
  {"TRDA",   "Recording dates", dumpId3Text},
  {"TRSN",   "Internet radio station name", dumpId3Text},
  {"TRSO",   "Internet radio station owner", dumpId3Text},
  {"TSIZ",   "Size", dumpId3Text},
  {"TSRC",   "ISRC (international standard recording code)", dumpId3Text},
  {"TSSE",   "Software/Hardware and settings used for encoding", dumpId3Text},
  {"TYER",   "Year", dumpId3Text},
  {"TXXX",   "User defined text information frame", dumpId3Text},
  {"UFID",   "Unique file identifier", dumpId3Frame},
  {"USER",   "Terms of use", dumpId3Frame},
  {"USLT",   "Unsychronized lyric/text transcription", dumpId3Frame},
  {"WCOM",   "Commercial information", dumpId3Frame},
  {"WCOP",   "Copyright/Legal information", dumpId3Frame},
  {"WOAF",   "Official audio file webpage", dumpId3Frame},
  {"WOAR",   "Official artist/performer webpage", dumpId3Frame},
  {"WOAS",   "Official audio source webpage", dumpId3Frame},
  {"WORS",   "Official internet radio station homepage", dumpId3Frame},
  {"WPAY",   "Payment", dumpId3Frame},
  {"WPUB",   "Publishers official webpage", dumpId3Frame},
  {"WXXX",   "User defined URL link frame", dumpId3Frame},

/* v2.4 frames: */
  {"TDRC",   "Recording time", dumpId3Text},
};


static void
listTags(void)
{
    int i;
    printf("WAV tags:\n");
    for (i=0; i<NA(chunkTypes); ++i) {
	printf(" %s %s\n", chunkTypes[i].tag, chunkTypes[i].description);
    }
    printf("(INAM and IART are displayed by Mac quicklook)\n");
}

static void
listId3Tags(void)
{
    int i;
    printf("ID3 tags:\n");
    for (i=0; i<NA(id3Types); ++i) {
	printf(" %s %s\n", id3Types[i].tag, id3Types[i].description);
    }
}

static void
dumpFormatFile(const char *filename)
{
    FILE *ifile = NULL;
    WaveChunk *waveFile;

    ifile = fopen(filename, "rb");
    if (ifile == NULL) {
	fprintf(stderr, "unable to open \"%s\", %s\n",
	    filename, strerror(errno));
	goto exit;
    }
    waveFile = OpenWaveFile(ifile);
    if (waveFile == NULL) {
	fprintf(stderr, "%s: %s\n", filename, WaveError);
	goto exit;
    }
    printf("%s:\n", filename);
    dumpFormat(waveFile);
    putchar('\n');

exit:
    /* TODO: free the waveFile structure and children */
    if (ifile != NULL) {
	fclose(ifile);
    }
}

static void
dumpTagsFile(const char *filename)
{
    FILE *ifile = NULL;
    WaveChunk *waveFile;

    ifile = fopen(filename, "rb");
    if (ifile == NULL) {
	fprintf(stderr, "unable to open \"%s\", %s\n",
	    filename, strerror(errno));
	goto exit;
    }
    waveFile = OpenWaveFile(ifile);
    if (waveFile == NULL) {
	fprintf(stderr, "%s: %s\n", filename, WaveError);
	goto exit;
    }
    printf("%s:\n", filename);
    dumpChunks(waveFile->children);
    putchar('\n');

exit:
    /* TODO: free the waveFile structure and children */
    if (ifile != NULL) {
	fclose(ifile);
    }
}

/**
 * Search for a format chunk, dump it, and return. Assume there is
 * only one format chunk.
 */
static void
dumpFormat(WaveChunk *waveFile)
{
    Chunk *chunk = searchFor(waveFile->children, "fmt ", NULL);

    if (chunk == NULL) {
	fprintf(stderr, "Format info not found in file\n");
    } else {
	FmtChunk *fc = (FmtChunk *)chunk;
	printf("  type=%u\n", fc->type);
	printf("  channels=%u\n", fc->channels);
	printf("  rate=%u\n", fc->sample_rate);
	printf("  bytes/second=%u\n", fc->bytes_sec);
	printf("  block align=%u\n", fc->block_align);
	printf("  bits/sample=%u\n", fc->bits_samp);
    }
}


/**
 * Recurse through the file structure looking for text tags
 * and printing them.
 */
static void
dumpChunks(Chunk *list)
{
    ChunkType *chunkType;
    for(; list != NULL; list = list->next)
    {
	if (strncasecmp(list->identifier, "list", 4) == 0) {
	    dumpChunks(((ListChunk *)list)->children);
	}
	else if ((chunkType = findChunkType(list->identifier)) != NULL) {
	    chunkType->dumper(list, chunkType);
	}
    }
}

static void
dumpText(Chunk *chunk, ChunkType *chunkType)
{
    TextChunk *tc = (TextChunk *)chunk;
    printf("  %4.4s %s: %s\n",
	chunk->identifier, chunkType->description, tc->string);
}


static ChunkType *
findChunkType(const char *tag)
{
    int i;
    for (i=0; i < NA(chunkTypes); ++i) {
	if (strncasecmp(tag, chunkTypes[i].tag, 4) == 0) {
	    return &chunkTypes[i];
	}
    }
    return NULL;
}

static FrameType *
findFrameType(const char *tag)
{
    int i;
    for (i=0; i < NA(id3Types); ++i) {
	if (strncasecmp(tag, id3Types[i].tag, 4) == 0) {
	    return &id3Types[i];
	}
    }
    return NULL;
}


	/* TAG EDITING */

static ListChunk * findInfoChunk(WaveChunk *waveFile);
static Id3v2Chunk * findId3Chunk(WaveChunk *waveFile);
static void clearInfoTags(ListChunk *lc);
static void clearId3Tags(Id3v2Chunk *ic);
static void recomputeId3Size(Id3v2Chunk *ic);
static int addInfoTag(ListChunk *lc, const ChunkType *, const char *value);
static int addId3Tag(Id3v2Chunk *ic, const FrameType *, const char *value);

/**
 * Search for a list of type "info" and modify the tags it contains.
 */
static int
modifyTags(WaveChunk *waveFile, char **tag_replacements, int n_replacements)
{
    ListChunk *lc = NULL;
    Id3v2Chunk *ic = NULL;
    ChunkType *ct = NULL;
    FrameType *ft = NULL;
    char **repl = tag_replacements;
    int nrep = n_replacements;
    char *eq;
    char tag[10];
    char *value;
    int l;

    for (; *repl != NULL && nrep > 0; ++repl, --nrep)
    {
	eq = strchr(*repl, '=');
	l = eq - *repl;
	if (l > 4) {
	    fprintf(stderr, "Unrecognized tag: \"%s\", ignored\n",
		*repl);
	    return -1;
	}

	memcpy(tag, *repl, l);
	if (l < 4) {
	    memset(tag, ' ', 4-l);
	}
	tag[4] = '\0';
	value = eq + 1;
	if (*value == '<')
	{
	    value = readValueFromFile(value + 1);
	}

	if ((ct = findChunkType(tag)) != NULL) {
	    if (lc == NULL) {
		lc = findInfoChunk(waveFile);
		if (clearTags) {
		    clearInfoTags(lc);
		}
	    }
	    if (addInfoTag(lc, ct, value) != 0) {
		return -1;
	    }
	} else if ((ft = findFrameType(tag)) != NULL) {
	    if (ic == NULL) {
		ic = findId3Chunk(waveFile);
		if (clearTags) {
		    clearId3Tags(ic);
		}
	    }
	    if (addId3Tag(ic, ft, value) != 0) {
		return -1;
	    }
	}
    }

    if (ic != NULL) {
	recomputeId3Size(ic);
    }

    return 0;
}

/**
 * Find and return the first LIST.INFO chunk in the file. Create
 * if necessary.
 */
static ListChunk *
findInfoChunk(WaveChunk *waveFile)
{
    Chunk *chunk, **ptr;
    ListChunk *lc;

    if ((chunk = searchFor(waveFile->children, "list", "info")) == NULL)
    {
	/* Create a new empty info chunk at the end of the file */
	chunk = newChunk("LIST", 4, 0, sizeof(*lc));
	lc = (ListChunk *) chunk;
	if (lc == NULL) {
	    fprintf(stderr, "Out of memory\n");
	    return NULL;
	}
	memcpy(lc->type, "INFO", 4);
	for (ptr = &waveFile->children; *ptr != NULL; ptr = &((*ptr)->next))
	  ;
	*ptr = chunk;
    }
    return (ListChunk *)chunk;
}

/**
 * Find and return the first ID3 chunk in the file. Create
 * if necessary.
 */
static Id3v2Chunk *
findId3Chunk(WaveChunk *waveFile)
{
    Chunk *chunk, **ptr;

    if ((chunk = searchFor(waveFile->children, "id3 ", NULL)) == NULL)
    {
	/* Create a new empty info chunk at the end of the file */
	chunk = newChunk("ID3 ", 0, 0, sizeof(Id3v2Chunk));
	if (chunk == NULL) {
	    fprintf(stderr, "Out of memory\n");
	    return NULL;
	}
	for (ptr = &waveFile->children; *ptr != NULL; ptr = &((*ptr)->next))
	  ;
	*ptr = chunk;
	((Id3v2Chunk *)chunk)->id3v2 = NewId3V2();
    }
    return (Id3v2Chunk *)chunk;
}

static void
clearInfoTags(ListChunk *lc)
{
    Chunk *child, *next;
    for (child = lc->children; child != NULL; child = next) {
	next = child->next;
	free(child);
    }
    lc->children = NULL;
}

static void
clearId3Tags(Id3v2Chunk *ic)
{
    Id3V2 *id3 = ic->id3v2;
    Frame *child, *next;
    for (child = id3->frames; child != NULL; child = next) {
	next = child->next;
	free(child);
    }
    id3->frames = NULL;
}

/**
 * Called after the ID3 tag has been modified. Recompute
 * its total size.
 */
static void
recomputeId3Size(Id3v2Chunk *ic)
{
    Id3V2 *id3 = ic->id3v2;
    Frame *child;
    uint32_t len = 0;
    for (child = id3->frames; child != NULL; child = child->next) {
	/* TODO: handle large-size headers */
	len += ID3_FRAME_SIZE + child->length;
    }
    id3->size = len;
    /* round tag size to a multiple of 2 bytes */
    ic->header.length = ID3_HEADER_SIZE + id3->size + (id3->size%2);
}

static int
addInfoTag(ListChunk *lc, const ChunkType *ct, const char *value)
{
    Chunk *child, *prev;
    TextChunk *textChunk;

    if (strlen(value) > 0) {
	textChunk = TextChunkFromString(ct->tag, value);
	if (textChunk == NULL) {
	    return -1;
	}
    } else {
	/* Empty string means delete the original but don't
	 * replace it with anything.
	 */
	textChunk = NULL;
    }

    /* Now search through the list chunk until we find a matching tag */
    if (lc->children == NULL) {
	/* That was easy */
	lc->children = (Chunk *)textChunk;
	return 0;
    }

    if (!appendTags) {
	/* Search list for matching tag */
	for (prev=NULL, child=lc->children;
	     child != NULL && strncasecmp(child->identifier, ct->tag, 4) != 0;
	     prev = child, child = child->next)
	  ;
    } else {
	/* Find end of list */
	for (prev = NULL, child=lc->children;
	     child != NULL;
	     prev = child, child = child->next)
	  ;
    }

    /* At this point, child is the node to be removed, if any,
     * and prev is the pointer to the child's predecessor, or
     * NULL if child was at start of list.
     */
    if (textChunk != NULL) {
	/* Delete and replace */
	if (child != NULL) {
	    textChunk->header.next = child->next;
	}
	if (prev == NULL) {
	    lc->children = (Chunk *) textChunk;
	} else {
	    prev->next = (Chunk *) textChunk;
	}
    } else if (child != NULL) {
	/* Delete but don't replace */
	if (prev == NULL) {
	    lc->children = child->next;
	} else {
	    prev->next = child->next;
	}
    }
    free(child);

    return 0;
}

static int
addId3Tag(Id3v2Chunk *ic, const FrameType *ft, const char *value)
{
    Id3V2 *id3 = ic->id3v2;
    Frame *child, *prev;
    TextFrame *textFrame;

    if (strlen(value) > 0) {
	textFrame = TextFrameFromString(ft->tag, value);
	if (textFrame == NULL) {
	    return -1;
	}
    } else {
	/* Empty string means delete the original but don't
	 * replace it with anything.
	 */
	textFrame = NULL;
    }

    /* Now search through the frames until we find a matching tag */
    if (id3->frames == NULL) {
	/* That was easy */
	id3->frames = (Frame *)textFrame;
	return 0;
    }

    if (!appendTags) {
	/* Search list for matching tag */
	for (prev=NULL, child=id3->frames;
	     child != NULL && strncasecmp(child->identifier, ft->tag, 4) != 0;
	     prev = child, child = child->next)
	  ;
    } else {
	/* Find end of list */
	for (prev = NULL, child=id3->frames;
	     child != NULL;
	     prev = child, child = child->next)
	  ;
    }

    /* At this point, child is the node to be removed, if any,
     * and prev is the pointer to the child's predecessor, or
     * NULL if child was at start of list.
     */
    if (textFrame != NULL) {
	/* Delete and replace */
	if (child != NULL) {
	    textFrame->header.next = child->next;
	}
	if (prev == NULL) {
	    id3->frames = (Frame *) textFrame;
	} else {
	    prev->next = (Frame *) textFrame;
	}
    } else if (child != NULL) {
	/* Delete but don't replace */
	if (prev == NULL) {
	    id3->frames = child->next;
	} else {
	    prev->next = child->next;
	}
    }
    free(child);

    return 0;
}


/**
* Recursively search for a chunk with this tag.
*/
static Chunk *
searchFor(Chunk *top, const char *tag, const char *type)
{
    Chunk *chunk;

    for (; top != NULL; top = top->next)
    {
	if (strncasecmp(top->identifier, tag, 4) == 0) {
	    if (strcasecmp(tag, "list") != 0 || type == NULL) {
		return top;
	    }
	    if (strncasecmp(((ListChunk *)top)->type, type, 4) == 0) {
		return top;
	    }
	}
	if (strncasecmp(top->identifier, "list", 4) == 0) {
	    ListChunk *lc = (ListChunk *)top;
	    chunk = searchFor(lc->children, tag, type);
	    if (chunk != NULL) {
		return chunk;
	    }
	}
    }
    return NULL;
}

static TextChunk *
TextChunkFromString(const char *tag, const char *string)
{
    TextChunk *textChunk;
    int l;
    l = strlen(string) + 1;
    l += l%2;
    textChunk = (TextChunk *)newChunk(tag, l, 0, sizeof(Chunk) + l);
    if (textChunk == NULL) {
	fprintf(stderr, "Out of memory\n");
	return NULL;
    }
    strcpy(textChunk->string, string);
    return textChunk;
}



	/* ID3 FUNCTIONS */

static void
dumpId3(Chunk *chunk, ChunkType *chunkType)
{
    Id3v2Chunk *ic = (Id3v2Chunk *)chunk;
    Frame *frame;
    FrameType *ft;

    printf("ID3 tags:\n");
    for (frame = ic->id3v2->frames; frame != NULL; frame = frame->next) {
	if ((ft = findFrameType(frame->identifier)) != NULL) {
	    ft->dumper(frame, ft);
	} else {
	    dumpId3Frame(frame, ft);
	}
    }
}

static void
dumpId3Frame(Frame *frame, FrameType *frameType)
{
    printf("  %4.4s %s: %ld bytes\n",
	frame->identifier, frameType->description, frame->length);
}

static void
dumpId3Text(Frame *frame, FrameType *frameType)
{
    TextFrame *tf = (TextFrame *)frame;
    wchar_t *buffer = NULL;
    int nchar, len;

    switch (tf->encoding) {
      case ID3_ENCODING_LATIN1:
      case ID3_ENCODING_UTF_8:
	nchar = frame->length - 1;
	printf("  %4.4s %s: %.*s\n",
	    frame->identifier, frameType->description,
	    nchar, tf->string);
	break;
      case ID3_ENCODING_UTF_16BOM:
	nchar = (frame->length - 1)/2;
	if ((buffer = malloc((nchar+1) * sizeof(*buffer))) == NULL) {
	    fprintf(stderr, "Out of memory in dumpId3Text\n");
	    goto exit;
	}
	len = utf16BOM_wchar((uint16_t *)tf->string, buffer, nchar);
	buffer[len] = 0;
	printf("  %4.4s %s: %S\n",
	    frame->identifier, frameType->description, (wchar_t *)buffer);
	break;

      case ID3_ENCODING_UTF_16BE:
	nchar = (frame->length - 1)/2;
	if ((buffer = malloc((nchar+1) * sizeof(*buffer))) == NULL) {
	    fprintf(stderr, "Out of memory in dumpId3Text\n");
	    goto exit;
	}
	len = utf16BE_wchar((uint16_t *)tf->string, buffer, nchar);
	buffer[len] = 0;
	printf("  %4.4s %s: %S\n",
	    frame->identifier, frameType->description, (wchar_t *)buffer);
	break;
    }

exit:
    free(buffer);
}

/**
 * Generate a text frame from a string. For now, only support
 * latin1.
 */
static TextFrame *
TextFrameFromString(const char *tag, const char *string)
{
    TextFrame *textFrame;
    int i, l;
    l = strlen(string);
    textFrame = malloc(sizeof(*textFrame) + l + 1);
    if (textFrame == NULL) {
	fprintf(stderr, "Out of memory\n");
	return NULL;
    }
    for (i=0; i<4; ++i) {
	textFrame->header.identifier[i] = toupper(tag[i]);
    }
    textFrame->header.length = l+2;
    textFrame->header.flags = 0;
    textFrame->header.offset = 0;
    textFrame->header.next = NULL;
    textFrame->encoding = ID3_ENCODING_LATIN1;
    strcpy((char *)textFrame->string, string);
    return textFrame;
}

	/* UTILITIES */

static char *
readValueFromFile(const char *filename)
{
    FILE *vfile = NULL;
    char *rval = NULL;
    off_t sz;

    if ((vfile = fopen(filename, "r")) == NULL) {
	fprintf(stderr, "Unable to read string file \"%s\", %s, tag ignored\n",
	    filename, strerror(errno));
	goto exit;
    }
    fseek(vfile, 0L, SEEK_END);
    sz = ftello(vfile);
    rewind(vfile);
    if (sz > MAX_FILE_TAG_SIZE) {
	sz = MAX_FILE_TAG_SIZE;
	fprintf(stderr, "Tag from \"%s\" truncated to %lu bytes\n",
	    filename, (unsigned long)sz);
    }
    if ((rval = malloc(sz+1)) == NULL) {
	fprintf(stderr, "Unable to read tag from \"%s\", out of memory\n",
	    filename);
	goto exit;
    }
    sz = fread(rval, 1, sz, vfile);
    rval[sz] = '\0';

exit:
    if (vfile != NULL) {
	fclose(vfile);
    }
    return rval;
}
