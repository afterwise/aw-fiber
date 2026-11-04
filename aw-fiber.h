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

#if !defined(_MSC_VER) || _MSC_VER >= 1800
# include <stdbool.h>
#endif
#if !defined(_MSC_VER) || _MSC_VER >= 1600
# include <stdint.h>
#endif
#include <stddef.h>

#if defined(__GNUC__)
# define _fiber_alwaysinline inline __attribute__((always_inline))
#elif defined(_MSC_VER)
# define _fiber_alwaysinline __forceinline
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Low-level coroutine api */

typedef unsigned int coroutine_t;

#define coroutine_begin(co) \
	switch (co) { \
	case 0:

/* __COUNTER__ gives each resume point its own label, and unlike __LINE__
   it is still a constant when MSVC compiles with /ZI */
#if defined(__COUNTER__)
# define _coroutine_label (__COUNTER__ + 1)
#else
# define _coroutine_label __LINE__
#endif

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
# define _coroutine_fallthrough [[fallthrough]]
#elif defined(__has_attribute)
# if __has_attribute(fallthrough)
#  define _coroutine_fallthrough __attribute__((fallthrough))
# endif
#endif
#ifndef _coroutine_fallthrough
# define _coroutine_fallthrough ((void) 0)
#endif

#define coroutine_end(co) _coroutine_end(co, _coroutine_label)
#define _coroutine_end(co,label) \
		(co) = (label); \
		_coroutine_fallthrough; \
		case (label):; \
	}

#define coroutine_yield(co,...) _coroutine_yield(co, _coroutine_label, __VA_ARGS__)
#define _coroutine_yield(co,label,...) do { \
		(co) = (label); \
		return __VA_ARGS__; \
		case (label):; \
	} while (0)

#define coroutine_cond_yield(co,cond,...) _coroutine_cond_yield(co, _coroutine_label, cond, __VA_ARGS__)
#define _coroutine_cond_yield(co,label,cond,...) do { \
		(co) = (label); \
		_coroutine_fallthrough; \
		case (label):; \
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

/* fibers still waiting to send to a fiber that ends get a reply of 0 */
_fiber_alwaysinline
static void _fiber_end(struct fiber *fib) {
	struct fiber *from;

	fib->state = FIBER_DEAD;

	while (!fiberlist_empty(&fib->inbox)) {
		from = (struct fiber *) fib->inbox.next;
		fiberlist_remove(&from->elem);
		from->message = 0;
		from->state = FIBER_READY;
	}
}

#define fiber_begin(fib) coroutine_begin((fib)->coroutine)

#define fiber_end(fib) \
	coroutine_end((fib)->coroutine); \
	do { _fiber_end(fib); } while (0)

#define fiber_send(fib,to,msg,reply,...) do { \
		(fib)->message = (msg); \
		(fib)->state = FIBER_SEND; \
		if ((to)->state != FIBER_DEAD) \
			fiberlist_add_back(&(to)->inbox, &(fib)->elem); \
		else { \
			(fib)->message = 0; \
			(fib)->state = FIBER_READY; \
		} \
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

