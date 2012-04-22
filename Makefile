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

PROGS = lsusd lsused request_suspend wakealarmd susman
TESTS = block_test watch_test event_test alarm_test
LIBS = suspend_block.o watcher.o wakeevent.o wakealarm.o

DEST = /usr/local/bin
LIBDEST = /usr/local/lib
all: $(PROGS) $(TESTS)

lsusd: lsusd.o

lsused: lsused.o libsus.a
	$(CC) -o lsused lsused.o libsus.a -levent

wakealarmd: wakealarmd.o libsus.a
	$(CC) -o wakealarmd wakealarmd.o libsus.a -levent

%-m.o: %.c
	$(CC) -o $@ -c $(CFLAGS) -Dmain=$* $<

susman: susman.o lsusd-m.o lsused-m.o wakealarmd-m.o libsus.a
	$(CC) -o susman susman.o lsusd-m.o lsused-m.o wakealarmd-m.o libsus.a -levent

request_suspend: request_suspend.o

install: susman suspend.py block.sh libsus.a
	cp susman suspend.py $(DEST)
	cp block.sh $(DEST)/suspend.sh
	chmod 755 $(DEST)/susman $(DEST)/suspend.py $(DEST)/suspend.sh
	cp libsus.a $(LIBDEST)
	chmod 644 $(LIBDEST)/libsus.a

block_test: block_test.o libsus.a
	$(CC) -o block_test block_test.o libsus.a
watch_test: watch_test.o libsus.a
	$(CC) -o watch_test watch_test.o libsus.a -levent
event_test: event_test.o libsus.a
	$(CC) -o event_test event_test.o libsus.a -levent
alarm_test: alarm_test.o libsus.a
	$(CC) -o alarm_test alarm_test.o libsus.a -levent

libsus.a: $(LIBS)
	ar cr libsus.a $(LIBS)

clean:
	rm -f *.o *.a *.pyc $(PROGS) $(TESTS)

