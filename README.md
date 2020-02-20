# A collection of utilities for manipulating audio tools

Frankly, sox(1) does most of everything you ever need from
the command line, and Audacity gives you everything you need
from a GUI. There are just a few tools here that I find
useful.

Name | What it is
---- | ----
[wavtags](#wavtags) | Edit the "INFO" tags in a .wav file
[id3](#id3) | Edit the id3 tags in a .mp3 file



## wavtags

Edit the "INFO" tags in a .wav file. Run with "--help" for documentation.

Includes libwav.[ch], a simple utility library for manipulating the
chunks in a .wav (or any Microsoft RIFF) file.

Also includes libid3.[ch], a simple utility library for manipulating
ID3 tags.

## id3

Edit the id3 tags in a .mp3 file. Run with --help or --long-help for
documentation. Can add images to the id3 tag or extract them. Includes the
"-f" option to let you format the output the way you want.

Written in python.
