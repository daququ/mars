/*
 * MARS Long Distance Replication Software
 *
 * Copyright (C) 2010-2014 Thomas Schoebel-Theuer
 * Copyright (C) 2011-2014 1&1 Internet AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef XIO_SIO_H
#define XIO_SIO_H

#include "lib_mapfree.h"

#define WITH_THREAD			16

struct sio_aio_aspect {
	GENERIC_ASPECT(aio);
	struct list_head io_head;
	int alloc_len;
	bool do_dealloc;
};

struct sio_brick {
	XIO_BRICK(sio);
	/*  parameters */
	bool o_direct;
	bool o_fdsync;
};

struct sio_input {
	XIO_INPUT(sio);
};

struct sio_threadinfo {
	struct sio_output *output;
	struct list_head aio_list;
	struct task_struct *thread;

	wait_queue_head_t event;
	spinlock_t lock;
	atomic_t queue_count;
	atomic_t fly_count;
	atomic_t total_count;
	unsigned long last_jiffies;
};

struct sio_output {
	XIO_OUTPUT(sio);
	/*  private */
	struct mapfree_info *mf;
	struct sio_threadinfo tinfo[WITH_THREAD+1];
	spinlock_t g_lock;
	atomic_t work_count;
	int index;
};

XIO_TYPES(sio);

#endif
