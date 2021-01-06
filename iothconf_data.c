/*
 *   iothconf_data.c: data structure for ioth auto configuration
 *
 *   Copyright 2021 Renzo Davoli - Virtual Square Team
 *   University of Bologna - Italy
 *
 *   This library is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or (at
 *   your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>

#include <iothconf_data.h>

struct ioth_confdata {
	struct ioth_confdata *next;
	struct ioth *stack;
	time_t timestamp;
	uint32_t ifindex;
	uint16_t datalen;
	uint8_t type;
	uint8_t flags;
};

static pthread_mutex_t ioth_confdata_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct ioth_confdata *ioth_confdata_root;

void ioth_confdata_add(struct ioth *stack, uint32_t ifindex, uint8_t type, time_t timestamp, uint8_t flags,
		void *data, uint16_t datalen) {
	struct ioth_confdata **scan, *this;
	pthread_mutex_lock(&ioth_confdata_mutex);
	for (scan = &ioth_confdata_root; *scan != NULL; scan = &this->next) {
		this = *scan;
		if (stack == this->stack && type == this->type && datalen == this->datalen &&
				memcmp(this + 1, data, datalen) == 0) {
			if (timestamp > this->timestamp)
				this->timestamp = timestamp;
			break;
		}
	}
	if (*scan == NULL) {
		this = malloc(sizeof(struct ioth_confdata) + datalen);
		if (this != NULL) {
			*this = (struct ioth_confdata) {
				.next = NULL,
					.stack = stack,
					.timestamp = timestamp,
					.ifindex = ifindex,
					.datalen = datalen,
					.type = type,
					.flags = flags,
			};
			memcpy(this + 1, data, datalen);
			*scan = this;
		}
	}
	pthread_mutex_unlock(&ioth_confdata_mutex);
}

typedef int ioth_confdata_forall_cb(void *data, void *arg);

void ioth_confdata_forall_mask(struct ioth *stack, uint32_t ifindex, uint8_t type, uint8_t mask,
		ioth_confdata_forall_cb *callback,  void *callback_arg) {
	struct ioth_confdata **scan, *this;
	int cb_retval = 0;
	type &= mask;
	pthread_mutex_lock(&ioth_confdata_mutex);
	scan = &ioth_confdata_root;
	while(*scan != NULL) {
		this = *scan;
		if ((stack == IOTH_CONFDATA_ANYSTACK || stack == this->stack) &&
				(ifindex == 0 || ifindex == this->ifindex) &&
				(type == 0 || type == (this->type & mask)) &&
				(cb_retval = callback(this + 1, callback_arg)) & IOTH_CONFDATA_FORALL_DELETE) {
			*scan = this->next;
			free(this);
		} else
			scan = &this->next;
		if (cb_retval & IOTH_CONFDATA_FORALL_BREAK)
			break;
	}
	pthread_mutex_unlock(&ioth_confdata_mutex);
}

static int delete_cb(void *data, void *arg) {
	time_t *timestamp = arg;
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	return (*timestamp == 0 || *timestamp > ioth_confdata->timestamp) ?
		IOTH_CONFDATA_FORALL_DELETE : 0;
}

void ioth_confdata_free(struct ioth *stack, uint32_t ifindex, uint8_t type, time_t timestamp) {
	ioth_confdata_forall(stack, ifindex, type, delete_cb, &timestamp);
}

struct ioth *ioth_confdata_getstack(void *data) {
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	return ioth_confdata->stack;
}

uint8_t ioth_confdata_gettype(void *data) {
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	return ioth_confdata->type;
}

uint32_t ioth_confdata_getifindex(void *data) {
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	return ioth_confdata->ifindex;
}

time_t ioth_confdata_gettimestamp(void *data) {
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	return ioth_confdata->timestamp;
}

uint16_t ioth_confdata_getdatalen(void *data) {
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	return ioth_confdata->datalen;
}

uint8_t ioth_confdata_setflags(void *data, uint8_t flags) {
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	uint8_t oldflags = ioth_confdata->flags;
	ioth_confdata->flags |= flags;
	return oldflags;
}

uint8_t ioth_confdata_clrflags(void *data, uint8_t flags) {
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	uint8_t oldflags = ioth_confdata->flags;
	ioth_confdata->flags &= ~flags;
	return oldflags;
}

static int read_timestamp_cb(void *data, void *arg) {
	time_t *timestamp = arg;
	struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
	*timestamp = ioth_confdata->timestamp;
	return IOTH_CONFDATA_FORALL_BREAK;
}

time_t ioth_confdata_read_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type) {
	time_t timestamp = 0;
	type = TIMESTAMP(type);
	ioth_confdata_forall(stack, ifindex, type, read_timestamp_cb, &timestamp);
	return timestamp;
}

time_t ioth_confdata_new_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type) {
	time_t oldtimestamp = ioth_confdata_read_timestamp(stack, ifindex, type);
	type = TIMESTAMP(type);
	for (;;) {
		struct timeval newtimestamp;
		gettimeofday(&newtimestamp, NULL);
		if (newtimestamp.tv_sec > oldtimestamp)
			return newtimestamp.tv_sec;
		usleep(1000000 - newtimestamp.tv_usec);
	}
}

void ioth_confdata_write_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type, time_t timestamp) {
	ioth_confdata_add(stack, ifindex, type, timestamp, 0, NULL, 0);
}

static int del_timestamp_cb(void *data, void *arg) {
  time_t *timestamp = arg;
  struct ioth_confdata *ioth_confdata = ((struct ioth_confdata *) data) - 1;
  *timestamp = ioth_confdata->timestamp;
  return IOTH_CONFDATA_FORALL_DELETE;
}

void ioth_confdata_del_timestamp(struct ioth *stack, uint32_t ifindex, uint8_t type) {
  time_t timestamp = 0;
  type = TIMESTAMP(type);
  ioth_confdata_forall(stack, ifindex, type, del_timestamp_cb, &timestamp);
}
