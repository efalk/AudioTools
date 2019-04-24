static const char usage[] = "usage:\n"
"	wavtags [options] [tag=value ...] infile [outfile]\n"
"	wavetags -i file ...\n"
"	wavetags -l\n"
"\n"
"	-h	--help		This list\n"
"	-v	--verbose	Verbose\n"
"	-c	--clear		Clear any existing tags\n"
"	-a	--append	Append tags to list instead of replacing\n"
"	-i	--info		Display format info and exit\n"
"	-L	--list-tags	List supported tags and exit\n"
"\n"
"Dumps the tags from a Microsoft multimedia file, such as .wav\n"
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
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>
#include <err.h>

#include "libwav.h"

#define	MAX_FILE_TAG_SIZE	50000	/* arbitrary decision */

typedef struct chunk_type {
    const char *tag, *description;
    void (*dumper)(Chunk *, struct chunk_type *);
} ChunkType;


#define	NA(a)	(sizeof(a)/sizeof(a[0]))

static void dumpChunks(Chunk *list);
static void dumpText(Chunk *chunk, ChunkType *);
static void listTags(void);
static void dumpFormat(WaveChunk *);
static int modifyTags(WaveChunk *, char **tag_replacements, int n_replacements);
static int addTag(ListChunk *lc, const char *replacement);
static Chunk *searchFor(Chunk *top, const char *tag, const char *type);
static TextChunk *TextChunkFromString(const char *tag, const char *string);
static char *readValueFromFile(const char *filename);


struct option longopts[] = {
  {"help", no_argument, NULL, 'h'},
  {"verbose", no_argument, NULL, 'v'},
  {"clear", no_argument, NULL, 'c'},
  {"append", no_argument, NULL, 'a'},
  {"info", no_argument, NULL, 'i'},
  {"list-tags", no_argument, NULL, 'L'},
  {0,0,0,0}
};

static int verbose = 0;
static bool clearTags = false;
static bool appendTags = false;
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

    while ((c = getopt_long(argc, argv, "hvcLai", longopts, NULL)) != -1)
    {
      switch (c) {
	case 'h': printf(usage); return 0;
	case 'v': verbose++; break;
	case 'L': listTags(); return 0;
	case 'c': clearTags = true; break;
	case 'a': appendTags = true; break;
	case 'i': showInfo = true; break;
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
	    ifile = fopen(argv[optind], "rb");
	    if (ifile == NULL) {
		fprintf(stderr, "unable to open \"%s\", %s\n",
		    argv[optind], strerror(errno));
		continue;
	    }
	    waveFile = OpenWaveFile(ifile);
	    printf("%s:\n", argv[optind]);
	    dumpFormat(waveFile);
	    putchar('\n');
	    fclose(ifile);
	}
	return 0;
    }

    ifilename = argv[optind++];

    ifile = fopen(ifilename, "rb");
    waveFile = OpenWaveFile(ifile);

    /* Recursively dig through the returned structure looking
     * for info tags.
     */
    if (n_replacements == 0 || optind >= argc) {
	dumpChunks(waveFile->children);
    } else {
	ofilename = argv[optind++];
	if (strcasecmp(ifilename, ofilename) == 0) {
	    fprintf(stderr, "Input file and output file cannot have the same name\n");
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
};

static void
listTags(void)
{
    int i;
    printf("Supported tags:\n");
    for (i=0; i<NA(chunkTypes); ++i) {
	printf(" %s %s\n", chunkTypes[i].tag, chunkTypes[i].description);
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
    int i;
    for(; list != NULL; list = list->next)
    {
	if (strncasecmp(list->identifier, "list", 4) == 0) {
	    dumpChunks(((ListChunk *)list)->children);
	}
	else {
	    for (i=0; i < NA(chunkTypes); ++i) {
		if (strncasecmp(list->identifier, chunkTypes[i].tag, 4) == 0) {
		    chunkTypes[i].dumper(list, &chunkTypes[i]);
		    break;
		}
	    }
	}
    }
}

static void
dumpText(Chunk *chunk, ChunkType *chunkType)
{
    TextChunk *tc = (TextChunk *)chunk;
    printf("  %4.4s %s: %s\n", chunk->identifier, chunkType->description, tc->string);
}


/**
 * Search for a list of type "info" and modify the tags it contains.
 */
static int
modifyTags(WaveChunk *waveFile, char **tag_replacements, int n_replacements)
{
    Chunk *chunk, *child, *next, **ptr;
    ListChunk *lc;
    char **repl = tag_replacements;
    int nrep = n_replacements;

    if ((chunk = searchFor(waveFile->children, "list", "info")) == NULL)
    {
	/* Create a new empty info chunk at the end of the file */
	chunk = newChunk("LIST", 4, 0, sizeof(*lc));
	lc = (ListChunk *) chunk;
	if (lc == NULL) {
	    fprintf(stderr, "Out of memory\n");
	    return -1;
	}
	memcpy(lc->type, "INFO", 4);
	for (ptr = &waveFile->children; *ptr != NULL; ptr = &((*ptr)->next))
	  ;
	*ptr = chunk;
    }
    lc = (ListChunk *) chunk;

    if (clearTags) {
	for (child = lc->children; child != NULL; child = next) {
	    next = child->next;
	    free(child);
	}
	lc->children = NULL;
    }

    for (; *repl != NULL && nrep > 0; ++repl, --nrep)
    {
	if (addTag(lc, *repl) != 0) {
	    return -1;
	}
    }

    return 0;
}

static int
addTag(ListChunk *lc, const char *replacement)
{
    Chunk *child, *prev;
    TextChunk *textChunk;
    char tag[10];
    char *value;
#if 0
    char *fvalue = NULL;
#endif
    char *eq;
    int i, l;

    eq = strchr(replacement, '=');
    l = eq - replacement;
    if (l > 4) {
	fprintf(stderr, "Unrecognized tag: \"%s\", ignored\n",
	    replacement);
	return -1;
    }

    memcpy(tag, replacement, l);
    if (l < 4) {
	memset(tag, ' ', 4-l);
    }
    tag[4] = '\0';

    /* Confirm this tag is in the approved list */
    for (i=0; i < NA(chunkTypes); ++i) {
	if (strncasecmp(chunkTypes[i].tag, tag, 4) == 0) {
	    break;
	}
    }
    if (i >= NA(chunkTypes)) {
	fprintf(stderr, "Unrecognized tag: \"%s\", ignored\n", tag);
	return -1;
    }

    value = eq + 1;
    if (*value == '<')
    {
	value = readValueFromFile(value + 1);
    }

    if (strlen(value) > 0) {
	textChunk = TextChunkFromString(tag, value);
	if (textChunk == NULL) {
	    return -1;
	}
    } else {
	/* Null textChunk means delete the original but don't
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
	     child != NULL && strncasecmp(child->identifier, tag, 4) != 0;
	     prev = child, child = child->next)
	  ;
    } else {
	/* Find end of list */
	for (prev = NULL, child=lc->children;
	     child != NULL;
	     prev = child, child = child->next)
	  ;
    }

    if (textChunk != NULL) {
	/* Delete and replace */
	if (child != NULL) {
	    /* Replace it */
	    textChunk->header.next = child->next;
	}
	if (prev == NULL) {
	    lc->children = (Chunk *) textChunk;
	} else {
	    prev->next = (Chunk *) textChunk;
	}
    } else {
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
