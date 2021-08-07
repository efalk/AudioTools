#!/usr/bin/env python

from __future__ import print_function

usage = """Simple interface to cddb database. Linux only at present.

For now, only enough functionality to support gnudb.gnudb.org
cd database.

Usage:  template [options] arguments

        -d /dev/cdrom           specify cdrom device
        -s host                 specify server (gnudb.gnudb.org)
        -p port                 specify port (80)
        -u username             specify username
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
import getpass
import os
import platform
import re
import signal
import socket
import string
import struct
import sys
import time
from time import sleep

APPNAME = 'cdinfo'
VERSION = "0.2"

default_device = "/dev/cdrom"

freedb_server = "gnudb.gnudb.org"
freedb_web_server = "gnudb.org"
freedb_port = 8880
#freedb_server="us.freedb.org"
#freedb_port = 80

PY3 = sys.version_info[0] >= 3
if PY3:
  import urllib.request as urllib
  from urllib.parse import urlencode
  from urllib.request import urlopen
  basestring = str
else:
  import urllib
  from urllib import urlencode
  from urllib2 import urlopen

if sys.platform.startswith('linux'):
    OS = 'Linux'
elif sys.platform.startswith('darwin'):
    OS = 'MacOS'
elif sys.platform.startswith('win'):
    OS = 'Windows'
else:
    OS = 'Unknown'
    # TODO: other operating systems as the need arises.


verbose = 0
user = None

# Copied from <linux/cdrom.h>
CDROMREADTOCHDR = 0x5305
# 2 bytes: 1st, last
CDROMREADTOCENTRY = 0x5306
CDROM_LBA = 0x01 # "logical block": first frame is #0 */
CDROM_MSF = 0x02 # "minute-second-frame": binary, not bcd here! */
CDROM_LEADOUT = 0xAA


#       SECTION 1 - CdInfo class to obtain and hold info about
#       the CD. Contains system-specific code.

class CdInfo(object):
  """Obtain track info from a CD. Device may be a path, file object, or
  integer file descriptor. If not specified, uses an os-appropriate
  default path. Set to None to defer opening the device."""

  @staticmethod
  def FromDevice(device=default_device):
    """Create CdInfo object from device path, device fd, or file object."""
    cdinfo = CdInfoImpl()
    if isinstance(device, basestring):
      cdinfo.open(device)
    else:
      cdinfo.dev = device
    if cdinfo.dev is not None:
      cdinfo.read_cdtoc_from_drive()
      if isinstance(device, basestring):
        cdinfo.dev.close()
        cdinfo.dev = None
      return cdinfo
    return None

  @staticmethod
  def FromOffsets(offsets, leadout):
    """Create CdInfo object from list of offsets and leadout value"""
    cdinfo = CdInfoImpl()
    cdinfo.first = 1
    cdinfo.tot_trks = len(offsets)
    for i,off in enumerate(offsets):
      cdinfo.cdtoc.append(CdInfo.CdTrack.FromOffset(i, off))
    cdinfo.leadout = CdInfo.CdTrack.FromOffset(CDROM_LEADOUT, leadout)
    return cdinfo

  def __init__(self):
    self.dev = None
    self.first = None
    self.tot_trks = None
    self.cdtoc = []
    self.leadout = None

  def read_cdtoc_from_drive(self):
    """Read the CD table of contents."""
    buffer = self.tocHeader()
    if buffer:
      self.first = buffer[0]
      self.tot_trks = buffer[1]
      for track in xrange(self.first, self.tot_trks+1):
        trk = self.CdTrack(self.tocEntry(track))
        self.cdtoc.append(trk)
      self.leadout = self.CdTrack(self.tocEntry(CDROM_LEADOUT))

  def trackCount(self):
    """Return the track count"""
    return self.tot_trks

  def totalSeconds(self):
    """Return the total seconds of audio."""
    return self.leadout.getFrames()//75 - self.cdtoc[0].getFrames()//75

  def cddb_discid(self):
    """Compute the cddb-compatible disc id."""
    t = 0
    n = 0
    for track in self.cdtoc:
      n += CdInfo.cddb_sum(track.getSeconds())
    # Programming note: I'm not sure what the -2 is for, but it's
    # needed to get the same results the cddb discid command gets
    t = self.totalSeconds() - 2
    return (n % 0xff) << 24 | t << 8 | self.tot_trks

  def getOffsets(self):
    """Return a list of track offsets in frames. Used for cddb lookups."""
    offsets = []
    for track in self.cdtoc:
      offsets.append(track.getFrames())
    return offsets

  @staticmethod
  def cddb_sum(n):
    """Utility: split n into its base-10 digits and sum them."""
    ret = 0
    while n > 0:
      ret += n%10
      n //= 10
    return ret

  # Methods that must be overridden in subclass
  # tocHeader(): Read the toc header.
  # tocEntry(track): Read one track info.

  def __repr__(self):
    return '<CdInfo %d tracks>' % (self.tot_trks)
  def __len__(self):
    return len(self.cdtoc)
  def __getitem__(self, key):
    return self.cdtoc[key]
  def __setitem(self, key, value):
    self.cdtoc[key] = value
  def __iter__(self):
    return iter(self.cdtoc)
  def append(self, track):
    self.cdtoc.append(track)

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

if OS == 'Linux':

  class CdInfoImpl(CdInfo):
    """CdInfo class for Linux."""
    def open(self, device):
      try:
        self.dev = open(device, "r")
      except IOError as e:
        print("Failed to open %s, %s" % (device, e), file=sys.stderr)
    def tocHeader(self):
      buffer = array.array('B', [0,0])
      rval = fcntl.ioctl(self.dev, CDROMREADTOCHDR, buffer)
      if rval != 0:
        print("CDROMREADTOCHDR failed,", os.strerror(rval), file=sys.stderr)
      return list(buffer)
    def tocEntry(self, track):
      buffer = struct.pack('BBBBBBBBiB', track, 0, CDROM_MSF,0,0,0,0,0,0,0)
      rval = fcntl.ioctl(self.dev, CDROMREADTOCENTRY, buffer)
      return struct.unpack('BBBBBBBBiB', rval)

else:
  class CdInfoImpl(CdInfo):
    def open(self, device):
      osname = platform.system()
      print("OS %s not supported yet" % osname, file=sys.stderr)
      return None
    def tocHeader(self):
      osname = platform.system()
      print("OS %s not supported yet" % osname, file=sys.stderr)
      return None

# Note: see https://sourceforge.net/p/discid/code/HEAD/tree/trunk/src/mac/ for mac
# See https://sourceforge.net/p/discid/code/HEAD/tree/trunk/src/win32/ for Win32




#       SECTION 2 - code to interact with CD database

response_re = re.compile(r'(\d\d\d) (.+)')
match_re = re.compile(r'(\S+) ([\da-fA-F]+) (.+)')
db_re = re.compile(r'(\S+?)=(.+)')


class CDDB(object):
  def __init__(self, server, port, username=None):
    global user
    self.server = server or freedb_server
    self.port = port or freedb_port
    self.user = username or user
    self.sock = None
    self.sockfile = None

  def connect(self):
    """Connect to cddb server and execute initial handshake.
    Return (socket, socketfile) or None on failure"""
    global verbose
    self.sock = socket.create_connection((self.server, self.port), 30)
    self.sockfile = self.sock.makefile("r")
    code, resp = self.cddb_get()
    if code != 200:
      print(resp.rstrip(), file=sys.stderr)
      return False
    code, resp = self.cddb_send('cddb hello %s %s %s %s' % \
      (getpass.getuser(), socket.getfqdn(), APPNAME, VERSION))
    if code >= 300:
      print(resp.rstrip(), file=sys.stderr)
      return False
    return True

  def cddb_get(self):
    """Read a CDDB response from sockfile."""
    global verbose
    resp = self.sockfile.readline()
    if verbose >= 2:
      print(resp.rstrip())
    code = int(resp[0:3])
    return code, resp

  def cddb_send(self, s):
    """Send string s to sock, read response from sockfile and return."""
    global verbose
    if PY3: s = s.encode("UTF-8")
    self.sock.send(s+b"\r\n")
    return self.cddb_get()

  def lookup(self, cd_info):
    """performs a cddb-based lookup using a DiscID
    on the given server and port.

    Returns the list of potential matches from the server. List elements
    are (genre, discid, title) tuples. (Note that the returned disc id
    is a key used by the database and will not match the cddb_discid()
    value from cd_info.)
    """
    global verbose

    matches = []

    try:
      if not self.sock and not self.connect():
        return None
      query = ["cddb query %08X %d" % (cd_info.cddb_discid(), cd_info.trackCount())]
      query.extend(map(str, cd_info.getOffsets()))
      query.append(str(cd_info.totalSeconds()))
      query = ' '.join(query)
      code, resp = self.cddb_send(query)
      if code == 200:
        _, category, discid, dtitle = resp.split(' ', 3)
        matches.append((category, discid, dtitle))
      elif code == 202:
        pass
      elif code in (211,210):
        line = self.sockfile.readline().rstrip()
        #if PY3: line = line.decode('utf-8')
        while line != '.':
          match = tuple(line.split(' ', 2))
          matches.append(match)
          line = self.sockfile.readline().rstrip()
          #if PY3: line = line.decode('utf-8')
      else:
        raise ValueError("unexpected response from server: " + line)
    except Exception as e:
      print("Failed to connect to %s, %s" % (self.server, e), file=sys.stderr)
      return None

    return matches


  def fetch(self, genre, key):
    """Given e.g. "rock", "be0b1b0d", return the database entry for
    that item as a list of tuples."""
    global verbose

    rval = []
    try:
      if self.server == "gnudb.gnudb.org":
	# The gnudb server has a bug in it which causes the
	# the DYEAR and DGENRE fields to be lost if you use the API. The
	# web interface doesn't have that issue, so we use that instead.
	req = "http://%s/gnudb/%s/%s" % (freedb_web_server, genre, key)
	if verbose >= 1:
	  print("open:", req)
	f = urllib.urlopen(req)
      else:
	if not self.sock and not self.connect():
	  return None
	code, resp = self.cddb_send("cddb read %s %s" % (genre, key))
	if code != 210:
	  raise ValueError("server response %d" % code)
	f = self.sockfile

      for line in f:
	line = line.rstrip()
	if line .startswith('.'):
	  break
	if line .startswith('#'):
	  continue
	mo = db_re.match(line)
	if mo:
	  rval.append((mo.group(1), mo.group(2)))

      if self.server == "gnudb.gnudb.org":
	f.close()

      return rval

    except Exception as e:
      print("Failed to connect to %s, %s" % (self.server, e), file=sys.stderr)
      return None


def main():
  global device, freedb_server, freedb_port, verbose, user

  test_mode = False
  long_form = False

  # Get arguments with getopt
  long_opts = ['help', 'device=', 'server=', 'port=']
  try:
    (optlist, args) = getopt.getopt(sys.argv[1:], 'hd:s:p:u:Tvlg:', long_opts)
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
      elif flag in ('-u', "--port"):
        user = value
      elif flag == '-v':
        verbose += 1
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

  if test_mode:
    # Max Sharam, A million year girl
    cd_info = CdInfo.FromOffsets((150, 17395, 34292, 53067,
        71137, 84725, 85962, 104055, 124290, 141842, 162472,
        175480, 194340), 213525)
  else:
    cd_info = CdInfo.FromDevice()
    if not cd_info:
      return 3

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

  id = cd_info.cddb_discid()
  if verbose:
    print("id: %08X" % id)

  cddb = CDDB(freedb_server, freedb_port, user)
  try:
    if not cddb.connect():
      print("Failed to connect to %s" % (freedb_server), file=sys.stderr)
  except Exception as e:
    print("Failed to connect to %s, %s" % (freedb_server, e), file=sys.stderr)
    return 3

  matches = cddb.lookup(cd_info)
  if matches is None:
    return 4
  for i, match in enumerate(matches):
    print("%2d: %8s %8s %s" % ((i,) + match))

  if long_form:
    for match in matches:
      print()
      time.sleep(1)       # don't spam the server
      disc = cddb.fetch(match[0], match[1])
      if not disc:
        print("Failed to read %s/%s" % (match[0], match[1]), file=sys.stderr)
        continue
      for kv in disc:
        print("%8s = %s" % kv)

  return 0


if __name__ == '__main__':
  signal.signal(signal.SIGPIPE, signal.SIG_DFL)
  try:
    sys.exit(main())
  except KeyboardInterrupt as e:
    print()
    sys.exit(1)
