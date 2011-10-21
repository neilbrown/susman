/*
 * block_test  - test program for blocking suspend
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "libsus.h"

main(int argc, char *argv[])
{
	int sleep_time = 5;
	int handle;
	if (argc == 2)
		sleep_time = atoi(argv[1]);

	handle = suspend_block(-1);
	printf("Have the block - waiting\n");
	sleep(sleep_time);
	printf("Releasing...\n");
	suspend_allow(handle);
	sleep(1);
	handle = suspend_block(handle);
	printf("Blocked again\n");
	sleep(sleep_time);
	suspend_close(handle);
	printf("OK all done\n");
	exit(1);
}
