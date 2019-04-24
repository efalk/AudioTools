# A collection of utilities for manipulating audio tools

Frankly, sox(1) does most of everything you ever need from
the command line, and Audacity gives you everything you need
from a GUI. There are just a few tools here that I find
useful.

Name | What it is
---- | ----
[wavtags](#wavtags) | Edit the "INFO" tags in a .wav file



## wavtags

Edit the "INFO" tags in a .wav file. Run with "--help" for documentation.

Includes libwav.[ch], a simple utility library for manipulating the
chunks in a .wav (or any Microsoft RIFF) file.
