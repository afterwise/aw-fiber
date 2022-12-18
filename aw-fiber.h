/* vim: set ts=4 sw=4 noet : */
/*
   Copyright (c) 2016-2022 Malte Hildingsson, malte (at) afterwi.se

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
 */

#ifndef AW_FIBER_H
#define AW_FIBER_H

/*
  Inspired by:
  Simon Tatham's Coroutines in C: http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
  Adam Dunkels' Protothreads: http://dunkels.com/adam/pt/index.html
 */

#if !_MSC_VER || _MSC_VER >= 1800
# include <stdbool.h>
#endif
#if !_MSC_VER || _MSC_VER >= 1600
# include <stdint.h>
#endif
#include <stddef.h>

#if __GNUC__
# define _fiber_alwaysinline inline __attribute__((always_inline))
#elif _MSC_VER
# define _fiber_alwaysinline __forceinline
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Low-level coroutine api */

typedef unsigned short coroutine_t;

#define coroutine_begin(co) \
	switch (co) { \
	case 0:

#define coroutine_end(co) \
		(co) = __LINE__; \
		case __LINE__:; \
	}

#define coroutine_yield(co,...) do { \
		(co) = __LINE__; \
		return __VA_ARGS__; \
		case __LINE__:; \
	} while (0)

#define coroutine_cond_yield(co,cond,...) do { \
		(co) = __LINE__; \
		case __LINE__:; \
		if (cond) return __VA_ARGS__; \
	} while (0)

/* High-level fiber api */

enum {
	FIBER_READY,
	FIBER_RECEIVE,
	FIBER_SEND,
	FIBER_REPLY,
	FIBER_DEAD
};

typedef uintptr_t fibermsg_t;

struct fiberlist {
	struct fiberlist *prev;
	struct fiberlist *next;
};

struct fiber {
	struct fiberlist elem;
	struct fiberlist inbox;
	fibermsg_t message;
	coroutine_t coroutine;
	unsigned char state;
};

_fiber_alwaysinline
static void fiberlist_init(struct fiberlist *list) {
	list->prev = list;
	list->next = list;
}

_fiber_alwaysinline
static bool fiberlist_empty(struct fiberlist *list) {
	return list->next == list;
}

_fiber_alwaysinline
static void fiberlist_add_back(struct fiberlist *list, struct fiberlist *elem) {
	elem->next = list;
	elem->prev = list->prev;
	list->prev->next = elem;
	list->prev = elem;
}

_fiber_alwaysinline
static void fiberlist_remove(struct fiberlist *elem) {
	elem->next->prev = elem->prev;
	elem->prev->next = elem->next;
}

_fiber_alwaysinline
static void fiber_init(struct fiber *fib) {
	fiberlist_init(&fib->inbox);
	fib->message = 0;
	fib->coroutine = 0;
	fib->state = FIBER_READY;
}

_fiber_alwaysinline
static bool fiber_ready(struct fiber *fib) {
	return fib->state == FIBER_READY ||
		(fib->state == FIBER_RECEIVE && !fiberlist_empty(&fib->inbox));
}

#define fiber_begin(fib) coroutine_begin((fib)->coroutine)

#define fiber_end(fib) \
	coroutine_end((fib)->coroutine); \
	do { (fib)->state = FIBER_DEAD; } while (0)

#define fiber_send(fib,to,msg,reply,...) do { \
		(fib)->message = (msg); \
		(fib)->state = FIBER_SEND; \
		fiberlist_add_back(&(to)->inbox, &(fib)->elem); \
		coroutine_cond_yield((fib)->coroutine, (fib)->state != FIBER_READY, __VA_ARGS__); \
		*(reply) = (fib)->message; \
		(fib)->message = 0; \
	} while (0)

#define fiber_receive(fib,from,msg,...) do { \
		(fib)->state = FIBER_RECEIVE; \
		coroutine_cond_yield((fib)->coroutine, fiberlist_empty(&(fib)->inbox), __VA_ARGS__); \
		(fib)->state = FIBER_READY; \
		*(from) = (struct fiber *) (fib)->inbox.next; \
		*(msg) = ((struct fiber *) (fib)->inbox.next)->message; \
		((struct fiber *) (fib)->inbox.next)->state = FIBER_REPLY; \
		fiberlist_remove((fib)->inbox.next); \
	} while (0)

#define fiber_reply(fib,to,msg,...) do { \
		coroutine_cond_yield((fib)->coroutine, (to)->state != FIBER_REPLY, __VA_ARGS__); \
		(to)->message = (msg); \
		(to)->state = FIBER_READY; \
	} while (0)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_FIBER_H */

