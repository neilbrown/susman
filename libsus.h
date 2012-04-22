/* headers for libsus - supporting suspend management.
 *
 * Copyright (C) 2011 Neil Brown <neilb@suse.de>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
int suspend_open();
int suspend_block(int handle);
void suspend_allow(int handle);
int suspend_close(int handle);
void suspend_abort(int handle);

void *suspend_watch(int (*will_suspend)(void *data),
		    void (*did_resume)(void *data),
		    void *data);
void suspend_ok(void *han);
void suspend_unwatch(void *v);

struct event *wake_set(int fd, void(*fn)(int,short,void*),
		       void *data, int prio);
void wake_destroy(struct event *ev);

struct event *wakealarm_set(time_t when, void(*fn)(int, short, void*),
			    void *data);
void wakealarm_destroy(struct event *ev);


