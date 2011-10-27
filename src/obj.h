/*
 * Spider -- An open source C language toolkit.
 *
 * Copyright (C) 2011 , Inc.
 *
 * lidp <openser@yeah.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#ifndef _SPIDER_OBJ_H
#define _SPIDER_OBJ_H

#include "linkedlist.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/*
Object Model implementing objects and containers.

This module implements an abstraction for objects (with locks and
reference counts), and containers for these user-defined objects,
also supporting locking, reference counting and callbacks.

The internal implementation of objects and containers is opaque to the user,
so we can use different data structures as needs arise.
*/
/*
USAGE - OBJECTS

An obj object is a block of memory that the user code can access,
and for which the system keeps track (with a bit of help from the
programmer) of the number of references around.  When an object has
no more references (refcount == 0), it is destroyed, by first
invoking whatever 'destructor' function the programmer specifies
(it can be NULL if none is necessary), and then freeing the memory.
This way objects can be shared without worrying who is in charge
of freeing them.
As an additional feature, obj objects are associated to individual
locks.

Creating an object requires the size of the object and
and a pointer to the destructor function:

    struct foo *o;

    o = obj_alloc(sizeof(struct foo), my_destructor_fn);

The value returned points to the user-visible portion of the objects
(user-data), but is also used as an identifier for all object-related
operations such as refcount and lock manipulations.

On return from obj_alloc():

 - the object has a refcount = 1;
 - the memory for the object is allocated dynamically and zeroed;
 - we cannot realloc() the object itself;
 - we cannot call free(o) to dispose of the object. Rather, we
   tell the system that we do not need the reference anymore:

    obj_ref(o, -1)

  causing the destructor to be called (and then memory freed) when
  the refcount goes to 0.

- obj_ref(o, +1) can be used to modify the refcount on the
  object in case we want to pass it around.

- obj_lock(obj), obj_unlock(obj), obj_trylock(obj) can be used
  to manipulate the lock associated with the object.


\section  USAGE - CONTAINERS

An obj container is an abstract data structure where we can store
obj objects, search them (hopefully in an efficient way), and iterate
or apply a callback function to them. A container is just an obj object
itself.

A container must first be allocated, specifying the initial
parameters. At the moment, this is done as follows:

    <b>Sample Usage:</b>
    \code

    struct obj_container *c;

    c = obj_container_alloc(MAX_BUCKETS, my_hash_fn, my_cmp_fn);
    \endcode

where

- MAX_BUCKETS is the number of buckets in the hash table,
- my_hash_fn() is the (user-supplied) function that returns a
  hash key for the object (further reduced modulo MAX_BUCKETS
  by the container's code);
- my_cmp_fn() is the default comparison function used when doing
  searches on the container,

A container knows little or nothing about the objects it stores,
other than the fact that they have been created by obj_alloc().
All knowledge of the (user-defined) internals of the objects
is left to the (user-supplied) functions passed as arguments
to obj_container_alloc().

If we want to insert an object in a container, we should
initialize its fields -- especially, those used by my_hash_fn() --
to compute the bucket to use.
Once done, we can link an object to a container with

   obj_link(c, o);

The function returns NULL in case of errors (and the object
is not inserted in the container). Other values mean success
(we are not supposed to use the value as a pointer to anything).
Linking an object to a container increases its refcount by 1
automatically.

\note While an object o is in a container, we expect that
my_hash_fn(o) will always return the same value. The function
does not lock the object to be computed, so modifications of
those fields that affect the computation of the hash should
be done by extracting the object from the container, and
reinserting it after the change (this is not terribly expensive).

\note A container with a single buckets is effectively a linked
list. However there is no ordering among elements.
*/

/*! \brief
 * Typedef for an object destructor. This is called just before freeing
 * the memory for the object. It is passed a pointer to the user-defined
 * data of the object.
 */
typedef void (*obj_destructor_fn)(void *);



/*! \brief
 * Allocate and initialize an object.
 *
 * \param data_size The sizeof() of the user-defined structure.
 * \param destructor_fn The destructor function (can be NULL)
 * \param debug_msg
 * \return A pointer to user-data.
 *
 * Allocates a struct obj with sufficient space for the
 * user-defined structure.
 * \note
 * - storage is zeroed; XXX maybe we want a flag to enable/disable this.
 * - the refcount of the object just created is 1
 * - the returned pointer cannot be free()'d or realloc()'ed;
 *   rather, we just call obj_ref(o, -1);
 *
 * @{
 */
#define obj_alloc(data_size, destructor_fn) __obj_alloc((data_size), (destructor_fn))

#define obj_t_alloc(data_size, destructor_fn, dbmsg) __obj_alloc((data_size), (destructor_fn))

void * __obj_alloc(size_t data_size, obj_destructor_fn destructor_fn);



/*! @} */

/*! \brief
 * Reference/unreference an object and return the old refcount.
 *
 * \param o A pointer to the object
 * \param delta Value to add to the reference counter.
 * \param tag used for debugging
 * \return The value of the reference counter before the operation.
 *
 * Increase/decrease the reference counter according
 * the value of delta.
 *
 * If the refcount goes to zero, the object is destroyed.
 *
 * \note The object must not be locked by the caller of this function, as
 *       it is invalid to try to unlock it after releasing the reference.
 *
 * \note if we know the pointer to an object, it is because we
 * have a reference count to it, so the only case when the object
 * can go away is when we release our reference, and it is
 * the last one in existence.
 *
 * @{
 */


#define obj_ref(user_data, delta) \
	__obj_ref(user_data,delta)

int __obj_ref(void *user_data, const int delta);

/*! @} */

/*! \brief
 * Lock an object.
 *
 * \param a A pointer to the object we want to lock.
 * \return 0 on success, other values on error.
 */
int __obj_lock(void * user_data, const char * file, const char * func, int line, const char * var);

#define obj_lock(a)  __obj_lock(a, __FILE__, __PRETTY_FUNCTION__, __LINE__, #a)


/*! @} */

/*! \brief
 * UnLock an object.
 *
 * \param a A pointer to the object we want to Unlock.
 * \return 0 on success, other values on error.
 */
int __obj_unlock(void * user_data, const char * file, const char * func, int line, const char * var);

#define obj_unlock(a) __obj_unlock(a, __FILE__, __PRETTY_FUNCTION__, __LINE__, #a)

/*! @} */

/*! \brief
 * TryLock an object. (don't block if fail)
 *
 * \param a A pointer to the object we want to Trylock.
 * \return 0 on success, other values on error.
 */
int __obj_trylock(void * user_data, const char * file, const char * func, int line, const char * var);

#define obj_trylock(a) __obj_trylock(a, __FILE__, __PRETTY_FUNCTION__, __LINE__, #a)


/*!
 * \brief Return the lock address of an object
 *
 * \param[in] obj A pointer to the object we want.
 * \return the address of the lock, else NULL.
 *
 * This function comes in handy mainly for debugging locking
 * situations, where the locking trace code reports the
 * lock address, this allows you to correlate against
 * object address, to match objects to reported locks.
 *
 */
void *spd_object_get_lockaddr(void *obj);


/******************* spd obj container support ***********************************/
/*!
 \page  obj Containers

Containers are data structures meant to store several objects,
and perform various operations on them.
Internally, objects are stored in lists, hash tables or other
data structures depending on the needs.

\note NOTA BENE: at the moment the only container we support is the
	hash table and its degenerate form, the list.

Operations on container include:

  -  c = \b obj_container_alloc(size, hash_fn, cmp_fn)
	allocate a container with desired size and default compare
	and hash function
         -The compare function returns an int, which
         can be 0 for not found, CMP_STOP to stop end a traversal,
         or CMP_MATCH if they are equal
         -The hash function returns an int. The hash function
         takes two argument, the object pointer and a flags field,

  -  \b obj_find(c, arg, flags)
	returns zero or more element matching a given criteria
	(specified as arg). 'c' is the container pointer. Flags
    can be:
	OBJ_UNLINK - to remove the object, once found, from the container.
	OBJ_NODATA - don't return the object if found (no ref count change)
	OBJ_MULTIPLE - don't stop at first match
	OBJ_POINTER	- if set, 'arg' is an object pointer, and a hashtable
                  search will be done. If not, a traversal is done.

  -  \b obj_callback(c, flags, fn, arg)
	apply fn(obj, arg) to all objects in the container.
	Similar to find. fn() can tell when to stop, and
	do anything with the object including unlinking it.
	  - c is the container;
      - flags can be
	     OBJ_UNLINK   - to remove the object, once found, from the container.
	     OBJ_NODATA   - don't return the object if found (no ref count change)
	     OBJ_MULTIPLE - don't stop at first match
	     OBJ_POINTER  - if set, 'arg' is an object pointer, and a hashtable
                        search will be done. If not, a traversal is done through
                        all the hashtable 'buckets'..
      - fn is a func that returns int, and takes 3 args:
        (void *obj, void *arg, int flags);
          obj is an object
          arg is the same as arg passed into obj_callback
          flags is the same as flags passed into obj_callback
         fn returns:
           0: no match, keep going
           CMP_STOP: stop search, no match
           CMP_MATCH: This object is matched.

	Note that the entire operation is run with the container
	locked, so noone else can change its content while we work on it.
	However, we pay this with the fact that doing
	anything blocking in the callback keeps the container
	blocked.
	The mechanism is very flexible because the callback function fn()
	can do basically anything e.g. counting, deleting records, etc.
	possibly using arg to store the results.

  -  \b iterate on a container
	this is done with the following sequence

\code

	    struct obj_container *c = ... // our container
	    struct obj_iterator i;
	    void *o;

	    i = obj_iterator_init(c, flags);

	    while ((o = obj_iterator_next(&i))) {
		... do something on o ...
		obj_ref(o, -1);
	    }

	    obj_iterator_destroy(&i);
\endcode

	The difference with the callback is that the control
	on how to iterate is left to us.

    - \b obj_ref(c, -1)
	dropping a reference to a container destroys it, very simple!

Containers are obj objects themselves, and this is why their
implementation is simple too.

Before declaring containers, we need to declare the types of the
arguments passed to the constructor - in turn, this requires
to define callback and hash functions and their arguments.
 */


/*! \brief
 * Type of a generic callback function
 * \param obj  pointer to the (user-defined part) of an object.
 * \param arg callback argument from obj_callback()
 * \param flags flags from obj_callback()
 *
 * The return values are a combination of enum _cb_results.
 * Callback functions are used to search or manipulate objects in a container.
 */
typedef int (obj_callback_fn)(void *obj, void *arg, int flags);

/*! \brief
 * Type of a generic callback function
 * \param obj pointer to the (user-defined part) of an object.
 * \param arg callback argument from obj_callback()
 * \param data arbitrary data from obj_callback()
 * \param flags flags from obj_callback()
 *
 * The return values are a combination of enum _cb_results.
 * Callback functions are used to search or manipulate objects in a container.
 */
typedef int(obj_callback_data_fn)(void *obj, void *arg, void *data, int flags);

/*! \brief a very common callback is one that matches by address. */
obj_callback_fn obj_match_by_addr;

/*! \brief
 * A callback function will return a combination of CMP_MATCH and CMP_STOP.
 * The latter will terminate the search in a container.
 */
enum _cb_results {
	CMP_MATCH = 0x1, /*!< the object matches the request */
	CMP_STOP = 0x2,  /*!< stop the search now */
};

/*! \brief
 * Flags passed to obj_callback() and obj_hash_fn() to modify its behaviour.
 */
enum search_flags {
	/*! Unlink the object for which the callback function
	 *  returned CMP_MATCH.
	 */
	OBJ_UNLINK	 = (1 << 0),
	/*! On match, don't return the object hence do not increase
	 *  its refcount.
	 */
	OBJ_NODATA	 = (1 << 1),
	/*! Don't stop at the first match in obj_callback() unless the result of
	 *  of the callback function == (CMP_STOP | CMP_MATCH).
	 */
	OBJ_MULTIPLE = (1 << 2),
	/*! obj is an object of the same type as the one being searched for,
	 *  so use the object's hash function for optimized searching.
	 *  The search function is unaffected (i.e. use the one passed as
	 *  argument, or match_by_addr if none specified).
	 */
	OBJ_POINTER	 = (1 << 3),
	/*! 
	 * \brief Continue if a match is not found in the hashed out bucket
	 *
	 * This flag is to be used in combination with OBJ_POINTER.  This tells
	 * the obj_callback() core to keep searching through the rest of the
	 * buckets if a match is not found in the starting bucket defined by
	 * the hash value on the argument.
	 */
	OBJ_CONTINUE     = (1 << 4),
	/*! 
	 * \brief By using this flag, the ao2_container being searched will _NOT_
	 * be locked.  Only use this flag if the obj_container is being protected
	 * by another mechanism other that the internal ao2_lock.
	 */
	OBJ_NOLOCK     = (1 << 5),
};

/*!
 * Type of a generic function to generate a hash value from an object.
 * flags is ignored at the moment. Eventually, it will include the
 * value of OBJ_POINTER passed to obj_callback().
 */
typedef int (obj_hash_fn)(const void *obj, const int flags);

struct obj_container;

/*! \brief
 * Allocate and initialize a container
 * with the desired number of buckets.
 *
 * We allocate space for a struct obj_container, struct container
 * and the buckets[] array.
 *
 * \param arg1 Number of buckets for hash
 * \param arg2 Pointer to a function computing a hash value.
 * \param arg3 Pointer to a function comparating key-value
 * 			with a string. (can be NULL)
 * \param arg4
 *
 * \return A pointer to a struct container.
 *
 * \note Destructor is set implicitly.
 */
#define obj_t_container_alloc(arg1,arg2,arg3,arg4) __obj_container_alloc((arg1), (arg2), (arg3))
#define obj_container_alloc(arg1,arg2,arg3)        __obj_container_alloc((arg1), (arg2), (arg3))

struct obj_container *__obj_container_alloc(const unsigned int n_buckets,
					    obj_hash_fn *hash_fn, obj_callback_fn *cmp_fn);
struct obj_container *__obj_container_alloc_debug(const unsigned int n_buckets,
						  obj_hash_fn *hash_fn, obj_callback_fn *cmp_fn,
						  char *tag, char *file, int line, const char *funcname,
						  int ref_debug);


/*! \brief
 * Returns the number of elements in a container.
 */
int obj_container_count(struct obj_container *c);


/*! \name Object Management
 * Here we have functions to manage objects.
 *
 * We can use the functions below on any kind of
 * object defined by the user.
 */
/*@{ */

/*!
 * \brief Add an object to a container.
 *
 * \param arg1 the container to operate on.
 * \param arg2 the object to be added.
 * \param arg3 used for debuging.
 *
 * \retval NULL on errors.
 * \retval newobj on success.
 *
 * This function inserts an object in a container according its key.
 *
 * \note Remember to set the key before calling this function.
 *
 * \note This function automatically increases the reference count to account
 *       for the reference that the container now holds to the object.
 */

#define obj_link(arg1, arg2)  __obj_link((arg1), (arg2), 0)
#define obj_t_link(arg1, arg2, arg3) __obj_link((arg1), (arg2), 0)
#define obj_link_nolock(arg1, arg2, arg3) __obj_link((arg1), (arg2), OBJ_NOLOCK)

void *__obj_link(struct obj_container *c, void *new_obj, int flags);


/*!
 * \brief Remove an object from a container
 *
 * \param arg1 the container
 * \param arg2 the object to unlink
 * \param arg3 tag for debugging
 *
 * \retval NULL, always
 *
 * \note The object requested to be unlinked must be valid.  However, if it turns
 *       out that it is not in the container, this function is still safe to
 *       be called.
 *
 * \note If the object gets unlinked from the container, the container's
 *       reference to the object will be automatically released. (The
 *       refcount will be decremented).
 */


#define obj_unlink(arg1, arg2)   __obj_unlink((arg1), (arg2), 0)
#define obj_t_unlink(arg1, arg2, arg3) __obj_unlink((arg1), (arg2), 0)
#define obj_unlink_nolock(arg1, arg2, arg3) __obj_unlink((arg1), (arg2), OBJ_NOLOCK)

void *__obj_unlink(struct obj_container *c, void *new_obj, int flags);


/*@} */

/*! \brief
 * obj_callback() is a generic function that applies cb_fn() to all objects
 * in a container, as described below.
 *
 * \param c A pointer to the container to operate on.
 * \param flags A set of flags specifying the operation to perform,
	partially used by the container code, but also passed to
	the callback.
     - If OBJ_NODATA is set, obj_callback will return NULL. No refcounts
       of any of the traversed objects will be incremented.
       On the converse, if it is NOT set (the default), The ref count
       of each object for which CMP_MATCH was set will be incremented,
       and you will have no way of knowing which those are, until
       the multiple-object-return functionality is implemented.
     - If OBJ_POINTER is set, the traversed items will be restricted
       to the objects in the bucket that the object key hashes to.
 * \param cb_fn A function pointer, that will be called on all
    objects, to see if they match. This function returns CMP_MATCH
    if the object is matches the criteria; CMP_STOP if the traversal
    should immediately stop, or both (via bitwise ORing), if you find a
    match and want to end the traversal, and 0 if the object is not a match,
    but the traversal should continue. This is the function that is applied
    to each object traversed. Its arguments are:
        (void *obj, void *arg, int flags), where:
          obj is an object
          arg is the same as arg passed into obj_callback
          flags is the same as flags passed into obj_callback (flags are
           also used by obj_callback).
 * \param arg passed to the callback.
 * \param tag used for debuging.
 * \return when OBJ_MULTIPLE is not included in the flags parameter,
 *         the return value will be either the object found or NULL if no
 *         no matching object was found. if OBJ_MULTIPLE is included,
 *         the return value will be a pointer to an obj_iterator object,
 *         which must be destroyed with obj_iterator_destroy() when the
 *         caller no longer needs it.
 *
 * If the function returns any objects, their refcount is incremented,
 * and the caller is in charge of decrementing them once done.
 *
 * Typically, obj_callback() is used for two purposes:
 * - to perform some action (including removal from the container) on one
 *   or more objects; in this case, cb_fn() can modify the object itself,
 *   and to perform deletion should set CMP_MATCH on the matching objects,
 *   and have OBJ_UNLINK set in flags.
 * - to look for a specific object in a container; in this case, cb_fn()
 *   should not modify the object, but just return a combination of
 *   CMP_MATCH and CMP_STOP on the desired object.
 * Other usages are also possible, of course.

 * This function searches through a container and performs operations
 * on objects according on flags passed.
 * XXX describe better
 * The comparison is done calling the compare function set implicitly.
 * The p pointer can be a pointer to an object or to a key,
 * we can say this looking at flags value.
 * If p points to an object we will search for the object pointed
 * by this value, otherwise we serch for a key value.
 * If the key is not unique we only find the first matching valued.
 *
 * The use of flags argument is the follow:
 *
 *	OBJ_UNLINK 		unlinks the object found
 *	OBJ_NODATA		on match, do return an object
 *				Callbacks use OBJ_NODATA as a default
 *				functions such as find() do
 *	OBJ_MULTIPLE		return multiple matches
 *				Default is no.
 *	OBJ_POINTER 		the pointer is an object pointer
 *
 * \note When the returned object is no longer in use, ao2_ref() should
 * be used to free the additional reference possibly created by this function.
 *
 * @{
 */

#define obj_callback(c, flags, cb_fn,arg)	 __obj_callback((c), (flags), (cb_fn),(arg))
#define obj_t_callback(c, flags, cb_fn,arg, tag)  __obj_callback((c), (flags), (cb_fn),(arg))

void *__obj_callback(struct obj_container *c, enum search_flags flags, obj_callback_fn cb_fn, void *arg);

void *__obj_callback_data(struct obj_container *c, const enum search_flags flags,
			  obj_callback_data_fn *cb_fn, void *arg, void *data);

/*! \brief
 *obj_callback_data() is a generic function that applies cb_fn() to all objects
 * in a container.  It is functionally identical to obj_callback() except that
 * instead of taking an ao2_callback_fn *, it takes an ao2_callback_data_fn *, and
 * allows the caller to pass in arbitrary data.
 *
 * This call would be used instead of ao2_callback() when the caller needs to pass
 * OBJ_POINTER as part of the flags argument (which in turn requires passing in a
 * prototype ao2 object for 'arg') and also needs access to other non-global data
 * to complete it's comparison or task.
 *
 * See the documentation for obj_callback() for argument descriptions.
 *
 * \see obj_callback()
 */
void *__obj_callback_data(struct obj_container * c, const enum search_flags flags, obj_callback_data_fn * cb_fn, void * arg, void * data);

/*! obj_find() is a short hand for obj_callback(c, flags, c->cmp_fn, arg)
 * XXX possibly change order of arguments ?
 */

#define obj_find(c, arg, flag)  __obj_find((c), (arg), (flag))

void *__obj_find(struct obj_container *c, void *arg, enum search_flags flags);



/*! \brief
 *
 *
 * When we need to walk through a container, we use an
 * ao2_iterator to keep track of the current position.
 *
 * Because the navigation is typically done without holding the
 * lock on the container across the loop, objects can be inserted or deleted
 * or moved while we work. As a consequence, there is no guarantee that
 * we manage to touch all the elements in the container, and it is possible
 * that we touch the same object multiple times.
 *
 * However, within the current hash table container, the following is true:
 *  - It is not possible to miss an object in the container while iterating
 *    unless it gets added after the iteration begins and is added to a bucket
 *    that is before the one the current object is in.  In this case, even if
 *    you locked the container around the entire iteration loop, you still would
 *    not see this object, because it would still be waiting on the container
 *    lock so that it can be added.
 *  - It would be extremely rare to see an object twice.  The only way this can
 *    happen is if an object got unlinked from the container and added again
 *    during the same iteration.  Furthermore, when the object gets added back,
 *    it has to be in the current or later bucket for it to be seen again.
 *
 * An iterator must be first initialized with ao2_iterator_init(),
 * then we can use o = ao2_iterator_next() to move from one
 * element to the next. Remember that the object returned by
 * ao2_iterator_next() has its refcount incremented,
 * and the reference must be explicitly released when done with it.
 *
 * In addition, ao2_iterator_init() will hold a reference to the container
 * being iterated, which will be freed when ao2_iterator_destroy() is called
 * to free up the resources used by the iterator (if any).
 *
 * Example:
 *
 *  \code
 *
 *  struct ao2_container *c = ... // the container we want to iterate on
 *  struct ao2_iterator i;
 *  struct my_obj *o;
 *
 *  i = ao2_iterator_init(c, flags);
 *
 *  while ((o = ao2_iterator_next(&i))) {
 *     ... do something on o ...
 *     ao2_ref(o, -1);
 *  }
 *
 *  ao2_iterator_destroy(&i);
 *
 *  \endcode
 *
 */

/*! \brief
 * The obj iterator
 *
 * \note You are not supposed to know the internals of an iterator!
 * We would like the iterator to be opaque, unfortunately
 * its size needs to be known if we want to store it around
 * without too much trouble.
 * Anyways...
 * The iterator has a pointer to the container, and a flags
 * field specifying various things e.g. whether the container
 * should be locked or not while navigating on it.
 * The iterator "points" to the current object, which is identified
 * by three values:
 *
 * - a bucket number;
 * - the object_id, which is also the container version number
 *   when the object was inserted. This identifies the object
 *   uniquely, however reaching the desired object requires
 *   scanning a list.
 * - a pointer, and a container version when we saved the pointer.
 *   If the container has not changed its version number, then we
 *   can safely follow the pointer to reach the object in constant time.
 *
 * Details are in the implementation of ao2_iterator_next()
 * A freshly-initialized iterator has bucket=0, version=0.
 */

struct obj_iterator {
	/*! the container */
	struct obj_container *c;
	/*! operation flags */
	int flags;
	/*! current bucket */
	int bucket;
	/*! container version */
	unsigned int c_version;
	/*! pointer to the current object */
	void *obj;
	/*! container version when the object was created */
	unsigned int version;
};

/*! Flags that can be passed to ao2_iterator_init() to modify the behavior
 * of the iterator.
 */
 enum obj_iterator_flags {
	/*! Prevents obj_iterator_next() from locking the container
	 * while retrieving the next object from it.
	 */
	OBJ_ITERATOR_DONTLOCK = (1 << 0),
	/*! Indicates that the iterator was dynamically allocated by
	 * obj API and should be freed by ao2_iterator_destroy().
	 */
	OBJ_ITERATOR_MALLOCD = (1 << 1),
	/*! Indicates that before the iterator returns an object from
	 * the container being iterated, the object should be unlinked
	 * from the container.
	 */
	OBJ_ITERATOR_UNLINK = (1 << 2),
};

/*!
 * \brief Create an iterator for a container
 *
 * \param c the container
 * \param flags one or more flags from ao2_iterator_flags
 *
 * \retval the constructed iterator
 *
 * \note This function does \b not take a pointer to an iterator;
 *       rather, it returns an iterator structure that should be
 *       assigned to (overwriting) an existing iterator structure
 *       allocated on the stack or on the heap.
 *
 * This function will take a reference on the container being iterated.
 *
 */
struct obj_iterator obj_iterator_init(struct obj_container *c, int flags);


/*!
 * \brief Destroy a container iterator
 *
 * \param i the iterator to destroy
 *
 * \retval none
 *
 * This function will release the container reference held by the iterator
 * and any other resources it may be holding.
 */
void obj_iterator_destory(struct obj_iterator *i);

#define obj_iterator_next(arg1) __obj_iterator_next((arg1))
void * __obj_iterator_next(struct obj_iterator *a);

#if defined(__cpluseplus) || defined(c_plusplus)
}
#endif
#endif
