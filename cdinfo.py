#!/usr/bin/env python3

from __future__ import print_function

usage = """Simple interface to cdrom drive for Linux.

For now, only enough functionality to support gnudb.gnudb.org
cd database.

Usage:  template [options] arguments

        -d /dev/cdrom           specify cdrom device
        -s host                 specify server (gnudb.gnudb.org)
        -p port                 specify port (80)
        -l                      long form, show data for all matches
        -v                      verbose
        -T                      test mode

Exit codes:

        0 - command accepted, successful return
        2 - user error
        3 - system error, e.g. unable to open device
        4 - sign error, probably bad protocol
        5 - sign error: buffer full
        6 - sign error: unrecognized response
"""

import array
import errno
import fcntl
import getopt
import os
import platform
import re
import signal
import socket
import string
import struct
import sys
import time
import urllib
from time import sleep

PY3 = sys.version_info[0] >= 3
if PY3:
  #import urllib.request
  from urllib.parse import urlencode
  from urllib.request import urlopen
else:
  from urllib import urlencode
  from urllib2 import urlopen

APPNAME = 'cdinfo'
VERSION = "0.1"

default_device = "/dev/cdrom"

freedb_server = "gnudb.gnudb.org"
freedb_port = 80
#freedb_server="us.freedb.org"
#freedb_port = 80

class CdInfo(object):
  """Obtain info from a CD. Device may be a path, file object, or
  integer file descriptor. If not specified, uses an os-appropriate
  default path. Set to None to defer opening the device."""
  @classmethod
  def FromOffsets(cls, offsets, leadout):
    """Create CdInfo object from list of offsets and leadout value"""
    rval = cls()
    rval.first = 1
    rval.last = len(offsets)
    for i,off in enumerate(offsets):
      rval.tracks.append(CdInfo.CdTrack.FromOffset(i, off))
    rval.leadout = CdInfo.CdTrack.FromOffset(CDROM_LEADOUT, leadout)
    return rval

  @classmethod
  def FromDevice(cls, device=default_device):
    """Create CdInfo object from device path, fd, or file object."""
    rval = cls()
    if isinstance(device, (str, unicode)):
      rval.fd = open(device, "r")
    else:
      rval.fd = device
    rval.getToc()
    if isinstance(device, (str, unicode)):
      rval.fd.close()
      rval.fd = None
    return rval

  def __init__(self):
    self.fd = None
    self.first = None
    self.last = None
    self.tracks = []
    self.leadout = None

  def getToc(self):
    """Read the CD table of contents."""
    buffer = self.tocHeader()
    if buffer:
      self.first = buffer[0]
      self.last = buffer[1]
      for track in xrange(self.first, self.last+1):
        trk = self.CdTrack(self.tocEntry(track))
        self.tracks.append(trk)
      self.leadout = self.CdTrack(self.tocEntry(CDROM_LEADOUT))

  def trackCount(self):
    """Return the track count"""
    return self.last

  def totalSeconds(self):
    """Return the total seconds of audio."""
    return self.leadout.getFrames()//75 - self.tracks[0].getFrames()//75

  def discId(self):
    """Compute the cddb-compatible disc id."""
    checksum = 0
    for track in self.tracks:
      checksum += CdInfo.digit_sum(track.getSeconds())
    return (checksum % 0xff) << 24 | self.totalSeconds() << 8 | self.last

  def getOffsets(self):
    """Return a list of track offsets in frames. Used for cddb lookups."""
    offsets = []
    for track in self.tracks:
      offsets.append(track.getFrames())
    return offsets

  @staticmethod
  def digit_sum(n):
    """Utility: split n into its base-10 digits and sum them."""
    rval = 0
    while n > 0:
      rval += n%10
      n //= 10
    return rval

  # Methods that must be overridden in subclass
  # tocHeader(): Read the toc header.
  # tocEntry(track): Read one track info.

  def __repr__(self):
    return '<CdInfo %d tracks>' % (self.last)
  def __len__(self):
    return len(self.tracks)
  def __getitem__(self, key):
    return self.tracks[key]
  def __setitem(self, key, value):
    self.tracks[key] = value
  def __iter__(self):
    return iter(self.tracks)
  def append(self, track):
    self.tracks.append(track)

  class CdTrack(object):
    @classmethod
    def FromOffset(cls, trkno, offset):
      """Return a track from an offset in frames."""
      seconds = offset // 75
      frames = offset % 75
      minutes = seconds//60
      seconds %= 60
      tocentry = [trkno, 0, 0, 3, minutes, seconds, frames, 0,0,0]
      return cls(tocentry)
    def __init__(self, tocentry):
      self.track = tocentry[0]
      self.minutes = tocentry[4]
      self.seconds = tocentry[5]
      self.frames = tocentry[6]
      self.mode = tocentry[9]
    def getSeconds(self):
      return self.minutes*60+self.seconds
    def getFrames(self):
      return (self.minutes*60+self.seconds)*75+self.frames
    def __repr__(self):
      return 'CdTrack(%d, %d:%02d:%02d, mode %d)' % \
        (self.track, self.minutes, self.seconds, self.frames, self.mode)

# Copied from <linux/cdrom.h>
CDROMREADTOCHDR = 0x5305
# 2 bytes: 1st, last
CDROMREADTOCENTRY = 0x5306
CDROM_LBA = 0x01 # "logical block": first frame is #0 */
CDROM_MSF = 0x02 # "minute-second-frame": binary, not bcd here! */
CDROM_LEADOUT = 0xAA

class CdInfoLinux(CdInfo):
  """CdInfo class with Linux-specific code."""
  def tocHeader(self):
    buffer = array.array('B', [0,0])
    rval = fcntl.ioctl(self.fd, CDROMREADTOCHDR, buffer)
    if rval != 0:
      print("CDROMREADTOCHDR failed,", os.strerror(rval), file=sys.stderr)
    return list(buffer)
  def tocEntry(self, track):
    buffer = struct.pack('BBBBBBBBiB', track, 0, CDROM_MSF,0,0,0,0,0,0,0)
    rval = fcntl.ioctl(self.fd, CDROMREADTOCENTRY, buffer)
    return struct.unpack('BBBBBBBBiB', rval)


response_re = re.compile(r'(\d\d\d) (.+)')
match_re = re.compile(r'(\S+) ([\da-fA-F]+) (.+)')
db_re = re.compile(r'(\S+?)=(.+)')

def cddb_lookup(cd_info, server, port):
  """performs a web-based lookup using a DiscID
  on the given server and port.

  Returns the list of potential matches from the server. List elements
  are (genre, discid, title) tuples. (Note that the returned disc id
  is a key used by the database and will not match the discId()
  value from cd_info.)
  """

  url = 'http://%s:%d/~cddb/cddb.cgi' % (server, port)

  query = ["cddb query %08X %d" % (cd_info.discId(), cd_info.trackCount())]
  query.extend(map(str, cd_info.getOffsets()))
  query.append(str(cd_info.totalSeconds()))
  post = [('cmd', ' '.join(query)),
        ('hello', 'user %s %s %s' % (socket.getfqdn(), APPNAME, VERSION)),
        ('proto', '6')]
  post = urlencode(post)
  if PY3:
    post = post.encode("UTF-8")

  results = urlopen(url, post)

  if results.getcode() != 200:
    raise ValueError("server response %d" % results.getcode())
  #print('status code:', results.getcode())

  # First line is a response code. 200=single exact match. 211 or 210
  # = multiple exact or inexact matches.

  matches = []

  line = results.readline().rstrip()
  if PY3: line = line.decode('utf-8')
  mo = response_re.match(line)
  if not mo:
    raise ValueError("unexpected response from server: " + line)
  if mo.group(1) == '200':
    mo = match_re.match(mo.group(2))
    if not mo:
      raise ValueError("invalid query result: " + line)
    matches.append((mo.group(1), mo.group(2), mo.group(3)))
  elif mo.group(1) == '202':
    pass        # no match
  elif mo.group(1) in ('211','210'):
    line = results.readline().rstrip()
    if PY3: line = line.decode('utf-8')
    while not line.startswith('.'):
      mo = match_re.match(line)
      if not mo:
        raise ValueError("invalid query result: " + line)
      matches.append((mo.group(1), mo.group(2), mo.group(3)))
      line = results.readline().rstrip()
      if PY3: line = line.decode('utf-8')
  else:
    raise ValueError("unexpected response from server: " + line)

  return matches


def cddb_fetch(genre, key, server, port):
  """Given e.g. "rock", "be0b1b0d", return the database entry for
  that item as a list of tuples."""

  url = 'http://%s:%d/~cddb/cddb.cgi' % (server, port)

  post = [("cmd", "cddb read %s %s" % (genre, key)),
          ('hello', 'user %s %s %s' % (socket.getfqdn(), APPNAME, VERSION)),
          ('proto', '6')]
  post = urlencode(post)
  if PY3:
    post = post.encode("UTF-8")
  #print("post:", post)

  results = urlopen(url, post)

  if results.getcode() != 200:
    raise ValueError("server response %d" % results.getcode())
  #print('status code:', results.getcode())

  rval = []
  line = results.readline().rstrip()
  if PY3: line = line.decode('utf-8')
  #print(line)
  mo = response_re.match(line)
  if not mo:
    raise ValueError("unexpected response from server: " + line)
  line = results.readline().rstrip()
  if PY3: line = line.decode('utf-8')
  while not line.startswith('.'):
    #print(line)
    if not line.startswith('#'):
      mo = db_re.match(line)
      if mo:
        rval.append((mo.group(1), mo.group(2)))
    line = results.readline().rstrip()
    if PY3: line = line.decode('utf-8')
  return rval


def main():
  global device, freedb_server, freedb_port

  verbose = False
  test_mode = False
  long_form = False

  # Get arguments with getopt
  long_opts = ['help', 'device=', 'server=', 'port=']
  try:
    (optlist, args) = getopt.getopt(sys.argv[1:], 'hd:s:p:Tvl', long_opts)
    for flag, value in optlist:
      if flag in ('-h', "--help"):
        print(usage)
        return 0
      elif flag in ('-d', "--device"):
        default_device = value
      elif flag in ('-s', "--server"):
        freedb_server = value
      elif flag in ('-p', "--port"):
        freedb_port = value
      elif flag == '-v':
        verbose = True
      elif flag == '-l':
        long_form = True
      elif flag == '-T':
        test_mode = True
  except getopt.GetoptError as e:
    print(e, file=sys.stderr)
    sys.exit(2)

#  test_track = CdInfo.CdTrack.FromOffset(1, 86037)
#  print(test_track)
#  print(test_track.getSeconds())
#  print(test_track.getFrames())
#  return 0

  osname = platform.system()
  if test_mode:
    # Max Sharam, A million year girl
    cd_info = CdInfoLinux.FromOffsets((150, 17395, 34292, 53067,
        71137, 84725, 85962, 104055, 124290, 141842, 162472,
        175480, 194340), 213525)
  elif osname == 'Linux':
    cd_info = CdInfoLinux.FromDevice()
  else:
    print("OS %s not supported yet" % osname, file=sys.stderr)
    return 2

  if verbose:
    print(cd_info)
    for track in cd_info:
      print(track, track.getSeconds(), track.getFrames())
    track = cd_info.leadout
    print(track, track.getSeconds(), track.getFrames())

    print("track count:", cd_info.trackCount())
    seconds = cd_info.totalSeconds()
    print("total time:", seconds, "seconds")

    offsets = cd_info.getOffsets()
    print("offsets:", offsets)

  id = cd_info.discId()
  print("id: %08X" % id)

  matches = cddb_lookup(cd_info, freedb_server, freedb_port)
  for i, match in enumerate(matches):
    print("%2d: %8s %8s %s" % ((i,) + match))

  if long_form:
    for match in matches:
      print()
      time.sleep(1)       # don't spam the server
      disc = cddb_fetch(match[0], match[1], freedb_server, freedb_port)
      for kv in disc:
        print("%8s = %s" % kv)



if __name__ == '__main__':
  signal.signal(signal.SIGPIPE, signal.SIG_DFL)
  try:
    sys.exit(main())
  except KeyboardInterrupt as e:
    print()
    sys.exit(1)
