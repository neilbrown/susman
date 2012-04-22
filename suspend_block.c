/*
 * Library routine to block and re-enable suspend.
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
#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "libsus.h"

int suspend_open()
{
	return open("/run/suspend/disabled", O_RDONLY|O_CLOEXEC);
}

int suspend_block(int handle)
{
	if (handle < 0)
		handle = suspend_open();
	if (handle < 0)
		return handle;

	flock(handle, LOCK_SH);
	return handle;
}

void suspend_allow(int handle)
{
	flock(handle, LOCK_UN);
}

int suspend_close(int handle)
{
	if (handle >= 0)
		close(handle);
}

void suspend_abort(int handle)
{
	int h = handle;
	char c;
	if (handle < 0)
		h = suspend_open();
	read(h, &c, 1);
	if (handle < 0)
		suspend_close(h);
}
