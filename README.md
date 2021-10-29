# A collection of utilities for manipulating audio files

Frankly, sox(1) does most of everything you ever need from
the command line, and Audacity gives you everything you need
from a GUI. There are just a few tools here that I find
useful.

Name | What it is
---- | ----
[wavtags](#wavtags) | Edit the "INFO" tags in a .wav file
[id3](#id3) | Edit the id3 tags in a .mp3 file
[cdinfo.py](#cdinfo) | Match a CD againt the gnudb CD database



## wavtags

Edit the "INFO" tags in a .wav file. Run with "--help" for documentation.

Includes libwav.[ch], a simple utility library for manipulating the
chunks in a .wav (or any Microsoft RIFF file).

Also includes libid3.[ch], a simple utility library for manipulating
ID3 tags.

## id3

Edit the id3 tags in a .mp3 file. Run with --help or --long-help for
documentation. Can add images to the id3 tag or extract them. Includes the
"-f" option to let you format the output the way you want.

Written in python.

    Display or edit ID3 data in an mp3 file.

    Usage:	id3 [options] file.mp3 ...

    Options:

    With no editing options (below), dumps the id3 tag(s)
    and exits.

    Help:

	-h --help      short help (this list)
	-H --long-help long help

	--genres	list genres and exit
	--frame-types	list Id3v2 frame types and exit
	--image-types	list picture types and exit

    Output controls:

	-v	verbose
	-s	short form
	-l	long form output
	-j	json output
	-1	only deal with Id3v1
	-2	only deal with Id3v2

	-f fmt	Control output format; see long help

	--write-images dir	Extract all images to dir

    Edit Id3 tags:

	These options modify the mp3 file.

	-a str		Set artist
	-A str		Set album
	-t str		Set title
	-c str		Set comment
	-c [[lang:]desc:]str Set comment with metadata. Lang defaults to "eng"
	-n N		Set track # (may be a string for Id3v2)
	-g N		Set genre by number
	-g str		Set genre by string (Id3v2 only)
	-y nnnn		Set year

	-T fid:str	Set arbitrary text frame, see --frame-types for list
	-T fid:		Delete text frame

	-T TXXX:descr:str	Set TXXX frame.
	-T TXXX:desc:		Delete TXXX frame.

	--from-json file	Read tags from json file.
	--delete		delete Id3 tag(s)

	--add-image file:TYPE[:description]	Add a picture.
	--remove-image TYPE			Remove a picture

	--encoding latin1|utf16|utf16-be|utf8	Set encoding (default utf8)

	--2.2			Write Id3v2.2 tag
	--2.3			Write Id3v2.3 tag
	--2.4			Write Id3v2.4 tag (default)
	--padding n		Padding for new id3v2 tags (1024)


    Details:

	Some Id3v2 text frames map to Id3v1 fields, e.g. setting the
	artist with -a also sets the TPE1 frame.

	Id3v1 text fields are always encoded with latin1. Avoid
	strings that can't be encoded with latin1 if you're writing
	an id3v1 tag, or if you've selected "--encoding latin1" for
	Id3v2.  Unencodable characters will be replaced with '?'.

	The --2.x and --padding flags are ignored if there is already
	an existing Id3v2 tag.

	Editing options are processed in order. For example,
	    --remove -a "Annie Lennox" -t "Sisters"
	would erase the existing ID3 tag and start a new one.

	Where possible, this tool over-writes the existing tags in place.
	Otherwise, it will re-write the entire .mp3 file, which may take
	longer and requires the use of a temporary file.

	By default, new Id3v2 tags are written with a certain amount
	of padding, allowing them to be modified later without needing
	to rewrite the entire mp3 file. The default padding is 1024
	bytes, but you can change this with the --padding option. This
	has no effect when modifying existing Id3v2 tags.

    Output format:

	The '-f' option lets you specify the format of the output.

	Format string is literal text plus directives, e.g.
	    '%f: "%t", artist=%a, album=%A, length=%TLEN'

	  Directives are:
	      %f	    filename
	      %a	    artist
	      %A	    album
	      %t	    title
	      %c	    comment
	      %n	    track #
	      %g	    genre
	      %y	    year
	      %XXXX	    Id3V2 frame

    Json:

	The -j option writes a json file to stdout. Normally, it
	generates an array of output as shown in the first example
	below. If the -s (short) option is also given, then it
	generates the third form, and only for the first file on
	the command line.

	On input, the --from-json option can accept several formats:

	1: An array of id3 info blocks and filenames:

	  [
	    {"filename": "_path_",
	     "id3v1": {_id3_block_},
	     "id3v2": {_id3_block_}
	    },
	    ...
	  ]

	  This form does not require any filenames on the command line.
	  This form lets you modify multiple files. This is the format
	  emitted with the "-j" option.


	2: A single id3 info block with a filename:

	  {"filename": "_path_",
	   "id3v1": {_id3_block_},
	   "id3v2": {_id3_block_}
	  }

	  This form does not require any filenames on the command line.


	3: A single id3 info block with no filename. This information will
	   be applied to every file listed on the command line:

	  {"id3v1": {_id3_block_},
	   "id3v2": {_id3_block_}
	  }


	4: A block of key/value pairs which will be applied to both id3 tags
	   on every file listed on the command line:

	  {_id3_block_}

	The second two forms require file name(s) on the command line and
	are applied equally to all files.


	An _id3_block_ may consist of the following keys:

	  "title"	string (TIT2)
	  "artist"	string (TPE1)
	  "album"	string (TALB)
	  "comment"	string (COMM)
	  "year"	string, 4 characters (TYER)
	  "track"	number (TRCK)
	  "genre"	number, use --genres for a list (TCON)

	  "Tttt"	string, use --frame-types for a list
	  "TXXX"	{"description":DESC, "value":STRING}
	  "Wwww"	url string, use --frame-types for a list
	  "WXXX"	{"description":DESC, "value":STRING}
	  "COMM"	{"lang":LANG¹, "description":DESC, "value":STRING}
	  "APIC"	{"type":TYPE², "description":DESC, "file":PATH}

	  If Id3V2 version is 2.2, the keys are the 3-character equivalents.

	  All strings are encoded utf-8 in json files.

	  ¹ Language is ISO-639-2, e.g. "eng"
	  ² Use --image-types for a list of image types.


    Exit codes:

	0 - command accepted, successful return
	1 - ID3 data not found
	2 - user error
	3 - system error

    Bugs and issues:

	Id3v2.2, Id3v2.3, and Id3v2.4 define different sets of
	frames. This app doesn't care, and supports any kind of frame
	on any version of Id3v2.  For example, any frame whose ident
	starts with 'T' is accepted as a text frame.

	Does not support Id3v2 earlier than 2.2

	Id3v2.4 allows text frames to have multiple strings. This app
	does not support this.

	The standard allows for multiple frames with the same tag,
	e.g. two "TALB" tags. This app will display all frames, but
	when modifying a frame, only modifies the first instance of
	a frame. Likewise, only the first instance of a frame will
	be displayed when using a custom format.

	Many frame types, such as "Synchronized tempo codes",
	"Popularimeter", and so forth are not supported. If their size
	is reasonably small (or -l is specified), they can be dumped
	to json in base64 format. If you need support for any of
	these, contact the author; they shouldn't be too hard to
	add.

	Extended headers, compression, and encryption are not
	supported.  I've never encountered an mp3 file in the wild
	that uses these.

    Python API Notes:

	Read the source code for notes on the API which allows you
	to use this module from your own python script.



## cdinfo

Yet another program that grabs track info from a CD and looks up the disc on the
cddb database. The differences are:

Every other program I've downloaded to do this eventually broke, typically because
of missing dependencies. Many of them depended on CD utility libraries that no longer
existed and so forth. Others depended on versions of Python that I don't
have on my server. This version has zero dependencies outside of the standard Python
libraries and has been tested on Python 2.6 and 3.7.

This version goes to the latest incarnation of cddb, which is currently hosted
at gnudb.gnudb.org.

This is a rough draft. It works on Linux. I don't have a Mac or Windows machine
with a CD drive, so I won't be porting it any time soon. Currently, it looks up
the CD in question in the database and displays all matches. In the future, I'll
add the ability to generate output in json, and to query the user to see which
match they want.
