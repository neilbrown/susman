
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
            lock_watcher = dnotify.dir('/var/run/suspend')

        self.f = open('/var/run/suspend/watching', 'r')
        self.getlock()
        while os.fstat(self.f.fileno()).st_nlink == 0:
            self.f.close()
            self.f = open('/var/run/suspend/watching', 'r')
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
        if os.fstat(self.f.fileno()).st_size == 0:
            if self.suspended and os.stat('/var/run/suspend/watching').st_size == 0:
                self.suspended = False
                if self.resume:
                    self.resume()
            return
        if not self.suspended and (not self.suspend or self.suspend()):
            # ready for suspend
            self.release()

    def release(self):
        # ready for suspend
        old = self.f
        self.f = open('/var/run/suspend/watching-next', 'r')
        self.getlock()
        self.suspended = True
        fcntl.flock(old, fcntl.LOCK_UN)
        old.close()

    def immediate(self, on):
        if on:
            if self.immediate_fd:
                return
            self.immediate_fd = open('/var/run/suspend/immediate','w')
            fcntl.flock(self.immediate_fd, fcntl.LOCK_EX)
            return
        else:
            if not self.immediate_fd:
                return
            self.immediate_fd.close()
            self.immediate_fd = None

blockfd = None
def block():
    global blockfd
    if blockfd:
        return
    try:
        blockfd = open('/var/run/suspend/disabled')
        fcntl.flock(blockfd, fcntl.LOCK_SH)
    except:
        pass

def unblock():
    global blockfd
    if blockfd:
        blockfd.close()
        blockfd = None


if __name__ == '__main__':
    import signal
    def sus(): print "Suspending"; return True
    def res(): print "Resuming"
    monitor(sus, res)
    print "ready"
    while True:
        signal.pause()
