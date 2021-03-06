
#
# interact with lsusd to provide suspend notification
#
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

import dnotify, fcntl, os

lock_watcher = None

class monitor:
    def __init__(self, suspend_callback, resume_callback):
        """
        Arrange that suspend_callback is called before we suspend, and
        resume_callback is called when we resume.
        If suspend_callback returns False, it must have arranged for
        'release' to be called soon to allow suspend to continue.
        """
        global lock_watcher
        if not lock_watcher:
            lock_watcher = dnotify.dir('/run/suspend')

        self.f = open('/run/suspend/watching', 'r')
        self.getlock()
        while os.fstat(self.f.fileno()).st_nlink == 0:
            self.f.close()
            self.f = open('/run/suspend/watching', 'r')
            self.getlock()

        self.suspended = False
        self.suspend = suspend_callback
        self.resume = resume_callback
        self.watch = lock_watcher.watch("watching", self.change)
        self.immediate_fd = None

    def getlock(self):
        # lock file, protecting againt getting IOError when we get signalled.
        locked = False
        while not locked:
            try:
                fcntl.flock(self.f, fcntl.LOCK_SH)
                locked = True
            except IOError:
                pass

    def change(self, watched):
        if self.suspended:
            # resume has happened if watching-next has been renamed.
            if (os.fstat(self.f.fileno()).st_ino ==
                os.stat('/run/suspend/watching').st_ino):
                global lock_watcher
                self.suspended = False
                self.watch.cancel()
                self.watch = lock_watcher.watch("watching", self.change)
                if self.resume:
                    self.resume()
            else:
                return
        # not suspended, but maybe it's time
        if os.fstat(self.f.fileno()).st_size > 0:
            if not self.suspend or self.suspend():
                # ready for suspend
                self.release()

    def release(self):
        # ready for suspend
        global lock_watcher
        old = self.f
        self.f = open('/run/suspend/watching-next', 'r')
        self.getlock()
        self.suspended = True
        self.watch.cancel()
        self.watch = lock_watcher.watch("watching-next", self.change)
        fcntl.flock(old, fcntl.LOCK_UN)
        old.close()

    def immediate(self, on):
        if on:
            if self.immediate_fd:
                return
            self.immediate_fd = open('/run/suspend/immediate','w')
            fcntl.flock(self.immediate_fd, fcntl.LOCK_EX)
            return
        else:
            if not self.immediate_fd:
                return
            self.immediate_fd.close()
            self.immediate_fd = None

class blocker:
    def __init__(self, blocked = True):
        self.blockfd = open('/run/suspend/disabled')
        if blocked:
            self.block()
    def block(self):
        fcntl.flock(self.blockfd, fcntl.LOCK_SH)
    def unblock(self):
        fcntl.flock(self.blockfd, fcntl.LOCK_UN)
    def close(self):
        self.blockfd.close()
        self.blockfd = None
    def abort(self):
        self.blockfd.read(1)


def abort_cycle():
    fd = open('/run/suspend/disabled')
    fd.read(1)
    fd.close()

if __name__ == '__main__':
    import signal
    def sus(): print "Suspending"; return True
    def res(): print "Resuming"
    monitor(sus, res)
    print "ready"
    while True:
        signal.pause()
