/***
 * Copyright 2009-2017 by Gabriel Parmer.  All rights reserved.
 * Redistribution of this file is permitted under the BSD 2 clause license.
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2017
 *
 * History:
 * - Initial implementation, ~2009
 * - Adapted for parsec and relicensed, 2016
 */

/*
 * API Conventions:
 * - obj, new, and tmp are pointers of type T where T is the struct containing the linked list
 * - head is a pointer to struct ps_list_head
 * - l is the list field name within T
 * - type is T without any ()
 * - all ps_list_head_* functions should be applied to struct ps_list_head pointers
 * - all ps_list_* functions should be passed items of type T, not struct ps_list_head
 * - as with most macro-based APIs, please avoid passing in functions that cannot be multiply evaluated;
 *   generally passing only variables is a good move
 *
 * Example Usage:
 *
 * struct ps_list_head h;
 * struct foo {
 *         struct ps_list l;
 *         void *d;
 * } node, *i, *tmp;
 *
 * ps_list_head_init(&h);
 * ps_list_init(&node);
 * ps_list_head_add(&h, &node, l);
 * ...
 * for (i = ps_list_head_first(&h, struct foo, l) ;
 *      i != ps_list_head(&h, struct foo, l) ;
 *      i = ps_list_next(i, l)) { ... }
 *
 * for (ps_list_iter_init(&h, i, l) ; !ps_list_iter_term(&h, i, l) ; i = ps_list_next(i, l)) { ... }
 *
 * ps_list_foreach(&h, i, l) { ... }
 *
 * ps_list_foreach_del(&h, i, tmp, l) {
 *          ps_list_rem(i, l);
 *          ps_free(i);
 * }
 *
 */

#ifndef PS_LIST_H
#define PS_LIST_H

struct ps_list {
	struct ps_list *n, *p;
};

/*
 * This is a separate type to 1) provide guidance on how to use the
 * API, and 2) to prevent developers from comparing pointers that
 * should not be compared.
 */
struct ps_list_head {
	struct ps_list l;
};

#define PS_LIST_DEF_NAME list

static inline void
ps_list_ll_init(struct ps_list *l)
{ l->n = l->p = l; }

static inline void
ps_list_head_init(struct ps_list_head *lh)
{ ps_list_ll_init(&lh->l); }

static inline int
ps_list_ll_empty(struct ps_list *l)
{ return l->n == l; }

static inline int
ps_list_head_empty(struct ps_list_head *lh)
{ return ps_list_ll_empty(&lh->l); }

static inline void
ps_list_ll_add(struct ps_list *l, struct ps_list *new)
{
	new->n    = l->n;
	new->p    = l;
	l->n      = new;
	new->n->p = new;
}

static inline void
ps_list_ll_rem(struct ps_list *l)
{
	l->n->p = l->p;
	l->p->n = l->n;
	l->p = l->n = l;
}

#define ps_offsetof(s, field) __builtin_offsetof(s, field)
//#define ps_offsetof(s, field) ((unsigned long)&(((s *)0)->field))

#define ps_container(intern, type, field)				\
	((type *)((char *)(intern) - ps_offsetof(type, field)))

/*
 * Get a pointer to the object containing *l, of a type shared with
 * *o.  Importantly, "o" is not accessed here, and is _only_ used for
 * its type.  It will typically be the iterator/cursor working through
 * a list.  Do _not_ use this function.  It is a utility used by the
 * following functions.
 */
#define ps_list_obj_get(l, o, lname) \
	ps_container(l, __typeof__(*(o)), lname)

//(typeof (*(o)) *)(((char*)(l)) - ps_offsetof(typeof(*(o)), lname))

/***
 * The object API.  These functions are called with pointers to your
 * own (typed) structures.
 */

#define ps_list_is_head(lh, o, lname)   (ps_list_obj_get((lh), (o), lname) == (o))

/* functions for if we don't use the default name for the list field */
#define ps_list_singleton(o, lname)    ps_list_ll_empty(&(o)->lname)
#define ps_list_init(o, lname)         ps_list_ll_init(&(o)->lname)
#define ps_list_next(o, lname)         ps_list_obj_get((o)->lname.n, (o), lname)
#define ps_list_prev(o, lname)         ps_list_obj_get((o)->lname.p, (o), lname)
#define ps_list_add(o, n, lname)       ps_list_ll_add(&(o)->lname, &(n)->lname)
#define ps_list_append(o, n, lname)    ps_list_add(ps_list_prev((o), lname), n, lname)
#define ps_list_rem(o, lname)          ps_list_ll_rem(&(o)->lname)
#define ps_list_head_add(lh, o, lname) ps_list_ll_add((&(lh)->l), &(o)->lname)
#define ps_list_head_append(lh, o, lname) ps_list_ll_add(((&(lh)->l)->p), &(o)->lname)

/**
 * Explicit type API: Pass in the types of the nodes in the list, and
 * the name of the ps_list field in that type.
 */

#define ps_list_head_first(lh, type, lname) \
	ps_container(((lh)->l.n), type, lname)
#define ps_list_head_last(lh, type, lname) \
	ps_container(((lh)->l.p), type, lname)

/* If your struct named the list field "list" (as defined by PS_LIST_DEF_NAME */
#define ps_list_is_head_d(lh, o)           ps_list_is_head(lh, o, PS_LIST_DEF_NAME)
#define ps_list_singleton_d(o)             ps_list_singleton(o, PS_LIST_DEF_NAME)
#define ps_list_init_d(o)                  ps_list_init(o, PS_LIST_DEF_NAME)
#define ps_list_next_d(o)                  ps_list_next(o, PS_LIST_DEF_NAME)
#define ps_list_prev_d(o)                  ps_list_prev(o, PS_LIST_DEF_NAME)
#define ps_list_add_d(o, n)                ps_list_add(o, n, PS_LIST_DEF_NAME)
#define ps_list_append_d(o, n)             ps_list_append(o, n, PS_LIST_DEF_NAME)
#define ps_list_rem_d(o)                   ps_list_rem(o, PS_LIST_DEF_NAME)

#define ps_list_head_last_d(lh, o)         ps_list_head_last(lh, o, PS_LIST_DEF_NAME)
#define ps_list_head_first_d(lh, type)     ps_list_head_first(lh, type, PS_LIST_DEF_NAME)
#define ps_list_head_add_d(lh, o)          ps_list_head_add(lh, o, PS_LIST_DEF_NAME)
#define ps_list_head_append_d(lh, o)       ps_list_head_append(lh, o, PS_LIST_DEF_NAME)

/**
 * Iteration API
 */

/* Iteration without mutating the list */
#define ps_list_foreach(head, iter, lname)				\
	for (iter = ps_list_head_first((head), __typeof__(*iter), lname) ; \
	     !ps_list_is_head((head), iter, lname)    ;			\
	     (iter) = ps_list_next(iter, lname))

#define ps_list_foreach_d(head, iter) ps_list_foreach(head, iter, PS_LIST_DEF_NAME)

/*
 * Iteration where the current node can be ps_list_rem'ed.
 * Notes:
 * - typeof(iter) == typeof(tmp)
 * - ps_list_add can be used on iter, but the added node will not be iterated over
 *
 * TODO: Add SMR/parallel version of this macro
 */
#define ps_list_foreach_del(head, iter, tmp, lname)			\
	for (iter = ps_list_head_first((head), __typeof__(*iter), lname), \
             (tmp) = ps_list_next((iter), lname) ;		        \
	     !ps_list_is_head((head), iter, lname) ;			\
	     (iter) = (tmp), (tmp) = ps_list_next((tmp), lname))

#define ps_list_foreach_del_d(head, iter, tmp) ps_list_foreach_del(head, iter, tmp, PS_LIST_DEF_NAME)

#endif	/* PS_LIST_H */
