#!/usr/bin/env python

# class to allow watching multiple files and
# calling a callback when any change (size or mtime)
#
# We take exclusive use of SIGIO and maintain a global list of
# watched files.
# As we cannot get siginfo in python, we check every file
# every time we get a signal.
# we report change is size, mtime, or ino of the file (given by name)

# Copyright (C) 2011 Neil Brown <neilb@suse.de>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


import os, fcntl, signal


dirlist = []
def notified(sig, stack):
    for d in dirlist:
        fcntl.fcntl(d.fd, fcntl.F_NOTIFY, (fcntl.DN_MODIFY|fcntl.DN_RENAME|
                                           fcntl.DN_CREATE|fcntl.DN_DELETE))
        d.check()

class dir():
    def __init__(self, dname):
        self.dname = dname
        self.fd = os.open(dname, 0)
        self.files = []
        self.callbacks = []
        fcntl.fcntl(self.fd, fcntl.F_NOTIFY, (fcntl.DN_MODIFY|fcntl.DN_RENAME|
                                              fcntl.DN_CREATE|fcntl.DN_DELETE))
        if not dirlist:
            signal.signal(signal.SIGIO, notified)
        dirlist.append(self)

    def watch(self, fname, callback):
        f = file(os.path.join(self.dname, fname), callback)
        self.files.append(f)
        return f

    def watchall(self, callback):
        self.callbacks.append(callback)

    def check(self):
        newlist = []
        for c in self.callbacks:
            if c():
                newlist.append(c)
        self.callbacks = newlist

        for f in self.files:
            f.check()

    def cancel(self, victim):
        if victim in self.files:
            self.files.remove(victim)

class file():
    def __init__(self, fname, callback):
        self.name = fname
        try:
            stat = os.stat(self.name)
        except OSError:
            self.ino = 0
            self.size = 0
            self.mtime = 0
        else:
            self.ino = stat.st_ino
            self.size = stat.st_size
            self.mtime = stat.st_mtime
        self.callback = callback

    def check(self):
        try:
            stat = os.stat(self.name)
        except OSError:
            if self.ino == 0:
                return False
            self.size = 0
            self.mtime = 0
            self.ino = 0
        else:
            if stat.st_size == self.size and stat.st_mtime == self.mtime \
                   and stat.st_ino == self.ino:
                return False
            self.size = stat.st_size
            self.mtime = stat.st_mtime
            self.ino = stat.st_ino

        self.callback(self)
        return True

    def cancel(self):
        global dirlist
        for d in dirlist:
            d.cancel(self)


if __name__ == "__main__" :
    import signal


    ##
    def ping(f): print "got ", f.name

    d = dir("/tmp/n")
    a = d.watch("a", ping)
    b = d.watch("b", ping)
    c = d.watch("c", ping)

    while True:
        signal.pause()
