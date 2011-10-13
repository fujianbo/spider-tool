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

 /*!
 * spdobj objects are always preceded by this data structure,
 * which contains a lock, a reference counter,
 * the flags and a pointer to a destructor.
 * The refcount is used to decide when it is time to
 * invoke the destructor.
 * The magic number is used for consistency check.
 * XXX the lock is not always needed, and its initialization may be
 * expensive. Consider making it external.
 */

struct _priv_data {
	spd_mutex_lock lock;
	int ref_counter;
	obj_destructor_fn destructor_fn;
	/*! for stats */
	size_t data_size;
	/*! magic number.  This is used to verify that a pointer passed in is a
	 *  valid obj */
	uint32_t magic;
};

#define	OBJ_MAGIC	0xa570b123

/*!
 * What an spdobj object looks like: fixed-size private data
 * followed by variable-size user data.
 */
struct obj {
	struct _priv_data priv_data;
	void *user_data[0];
};

/*!
 * \brief convert from a pointer _p to a user-defined object
 *
 * \return the pointer to the astobj2 structure
 */
static inline struct obj *internal_obj(void *user_data)
{
	struct obj *p;

	if(!user_data) {
		spd_log(LOG_ERROR, "User data is NULL\n");
		return NULL;
	}

	p = (struct obj *)((char *)user_data - sizeof(*p));

	if(OBJ_MAGIC != p->priv_data.magic) {
		spd_log(LOG_ERROR, "bad magic number 0x%x for %p\n", p->priv_data.magic, p);
		return NULL;
	}

	return p;
}

enum obj_callback_type {
	DEFAULT,
	WITH_DATA,
};

/*!
 * \brief convert from a pointer _p to an spdobj object
 *
 * \return the pointer to the user-defined portion.
 */
 #define EXTERNAL_OBJ(_p) ((_p) == NULL ? NULL : (_p)->user_data)

static int internal_obj_ref(void *user_data, const int delta);
 
static struct obj_container *internal_obj_container_alloc(struct spd_container *c, const uint n_buckets, spd_hash_fn *hash_fn,
							  spd_callback_fn *cmp_fn);
static struct bucket_entry *internal_obj_link(struct obj_container *c, void *user_data, int flags, const char *file, int line, const char *func);

static void *internal_obj_callback(struct obj_container *c,
				   const enum search_flags flags, void *cb_fn, void *arg, void *data, enum obj_callback_type type,
				   char *tag, char *file, int line, const char *funcname);

static void *internal_obj_iterator_next(struct obj_iterator *a, struct bucket_entry **q);

int __obj_lock(void *user_data, const char *file, const char *func, int line, const char *var)
{
	struct obj *p = internal_obj(user_data);

	if(p == NULL)
		return -1;

	return spd_mutex_lock(&p->priv_data.lock);
}

int __obj_unlock(void *user_data, const char *file, const char *func, int line, const char *var)
{
	struct obj *p = internal_obj(user_data);

	if(p == NULL)
		return -1;

	return spd_mutex_unlock(&p->priv_data.lock);
}

int __obj_trylock(void * user_data, const char * file, const char * func, int line, const char * var)
{
	struct obj *p = internal_obj(user_data);

	if(p == NULL)
		return -1;

	return spd_mutex_trylock(&p->priv_data.lock);
}

void *spd_object_get_lockaddr(void *obj)
{
	struct obj *p = internal_obj(obj);

	if(p == NULL)
		return NULL;

	return &p->priv_data.lock;
}

int __obj_ref(void *user_data, const int delta)
{
	struct obj *p = internal_obj(user_data);

	if(p == NULL)
		return -1;

	return internal_obj_ref(user_data, delta);
}

int __internal_obj_ref(void * user_data, const int delta)
{
	struct obj *p = internal_obj(user_data);
	int ret, current_value;
	
	if(p == NULL)
		return -1;
	if(delta == 0)
		return p->priv_data.ref_counter;

	/* we modify with an atomic operation the reference counter */
	ret = spd_atomic_fetchadd_int(&p->priv_data.ref_counter, delta);
	current_value = ret + delta;
	
	if(current_value < 0)
		spd_log(LOG_ERROR, "obj ref counter is negitave\n");

	/* ref counter to zero, destroy obj */
	if(current_value <= 0) {
		if(p->priv_data.destructor_fn != NULL) {
			p->priv_data.destructor_fn(user_data);
		}

		spd_mutex_destroy(&p->priv_data.lock);

		/* for safety, zero-out the obj header and also the
		 * first word of the user-data, which we make sure is always
		 * allocated. */
		memset(obj, '\0', sizeof(struct obj *) + sizeof(void *) );
		spd_safe_free(p);
	}

	return ret;
}

/*
 * We always alloc at least the size of a void *,
 * for debugging purposes.
 */

void *internal_obj_alloc(size_t data_size, obj_destructor_fn destructor_fn, const char  *file, const int line, const char *file)
{
	struct obj *p;

	if(data_size < sizeof(void *))
		data_size = sizeof(void *);

	p = (struct obj *)spd_calloc(1, sizeof(*p) + data_size);

	if(p == NULL)
		return NULL;

	spd_mutex_init(&p->priv_data.lock);
	p->priv_data.destructor_fn = destructor_fn; /* can be NULL */
	p->priv_data.magic = OBJ_MAGIC;
	p->priv_data.ref_counter = 1;
	p->priv_data.data_size = data_size; 

	return EXTERNAL_OBJ(p);
}

void *__obj_alloc(size_t data_size, obj_destructor_fn destructor_fn)
{
	return internal_obj_alloc(data_size, destructor_fn, __FILE__, __LINE__, __FUNCTION__);
}



/******************* spd obj container part support  ***********************************/
/* internal callback to destroy a container */
static void container_destruct(void *c);

/*!
 * A structure to create a linked list of entries,
 * used within a bucket.
 * XXX \todo this should be private to the container code
 */
struct bucket_entry {
	SPD_LIST_ENTRY(bucket_entry)entry;
	int version;
	struct obj *pobj;   /* pointer to internal data */
};

/* each bucket in the container is a tailq. */
SPD_LIST_HEAD_NOLOCK(bucket, bucket_entry);

/*!
 * A container; stores the hash and callback functions, information on
 * the size, the hash bucket heads, and a version number, starting at 0
 * (for a newly created, empty container)
 * and incremented every time an object is inserted or deleted.
 * The assumption is that an object is never moved in a container,
 * but removed and readded with the new number.
 * The version number is especially useful when implementing iterators.
 * In fact, we can associate a unique, monotonically increasing number to
 * each object, which means that, within an iterator, we can store the
 * version number of the current object, and easily look for the next one,
 * which is the next one in the list with a higher number.
 * Since all objects have a version >0, we can use 0 as a marker for
 * 'we need the first object in the bucket'.
 *
 * \todo Linking and unlink objects is typically expensive, as it
 * involves a malloc() of a small object which is very inefficient.
 * To optimize this, we allocate larger arrays of bucket_entry's
 * when we run out of them, and then manage our own freelist.
 * This will be more efficient as we can do the freelist management while
 * we hold the lock (that we need anyways).
 */

struct obj_container {
	obj_hash_fn *hash_fn;
	obj_callback_fn *cmp_fn;
	int n_buckets;
	int elements;
	int version;
	/*!variable size */
	struct bucket buckets[0]; /*! lengthen tailq, each bucket is a linkedlist */
};

/*!
 * \brief always zero hash function
 *
 * it is convenient to have a hash function that always returns 0.
 * This is basically used when we want to have a container that is
 * a simple linked list.
 *
 * \returns 0
 */
static int hash_zero(void *user_data, const int flags)
{
	return 0;
}

static struct obj_container *internal_obj_container_alloc(struct obj_container *c, const unsigned int n_buckets, obj_hash_fn *hash_fn,
							  obj_callback_fn *cmp_fn)
{
	if(!c)
		return NULL;

	c->n_buckets = hash_fn ? n_buckets : 1;
	c->version = 1;
	c->hash_fn = hash_fn ? hash_fn : hash_zero;
	c->cmp_fn = cmp_fn;

	return c;
}

static struct obj_container *__obj_container_alloc(const unsigned int n_bucket, obj_hash_fn hash_fn, obj_callback_fn cmp_fn)
{

	const unsigned int num_bucket = hash_fn ? n_bucket : 1;
	
	size_t container_size = sizeof(struct obj_container) + num_bucket *(struct bucket);

	struct obj_container c = __obj_alloc(container_size, container_destruct);

	return internal_obj_container_alloc(c, n_bucket, hash_fn, cmp_fn);
}

/*!
 * return the number of elements in the container
 */
int obj_container_count(struct obj_container *c)
{
	return c->elements;
}

static struct bucket_entry *internal_obj_link(struct obj_container * c, void * user_data, int flags, const char * file, int line, const char * func)
{
	int i;
	struct bucket_entry *p;
	struct obj *pobj = internal_obj(user_data);

	if(pobj == NULL)
		return NULL;

	if(internal_obj(c) == NULL)
		return NULL;

	p = sizeof(*p);

	p = (struct bucket_entry *)spd_calloc(1, sizeof(*p));

	if(p == NULL)
		return NULL;

	i = abs(c->hash_fn(user_data, OBJ_POINTER));
	if(!flags & OBJ_NOLOCK) {
		obj_lock(c);  /* lock the container when link obj to it*/
	}

	i %= c->n_buckets;

	p->pobj = pobj;

	p->version = spd_atomic_fetchadd_int(&c->version, 1)
	SPD_LIST_INSERT_TAIL(c->buckets[i], p, entry);
	spd_atomic_fetchadd_int(&c->elements, 1);

	return p;
}

void *__obj_link(struct obj_container * c, void *user_data, int flags)
{
	struct bucket_entry *p = internal_obj_link(c, user_data, flags, __FILE__, __LINE__, __PREFTTY_FUNCTION__);

	if(p) {
		__obj_ref(user_data, +1);
		if(!flags & OBJ_NOLOCK) {
			obj_unlock(c);
		}
	}

	return p;
		
}

/*!
 * \brief another convenience function is a callback that matches on address
 */
int obj_match_by_addr(void *user_data, void *arg, int flags)
{
	return (user_data == arg) ? (CMP_MATCH | CMP_STOP) : 0;
}

void *__obj_unlink(struct obj_container * c, void * user_data, int flags)
{
	if(internal_obj(user_data) == NULL)
		return NULL;

	flags |= (OBJ_UNLINK | OBJ_POINTER | OBJ_NODATA);

	__obj_callback(c, flags, obj_match_by_addr, user_data);

	return NULL;
}

/*! 
 * \brief special callback that matches all 
 */ 

static int  cb_true(void *user_data, void *arg, int flags)
{
	return CMP_MATCH;
}

/*!
 * \brief similar to cb_true, but is an obj_callback_data_fn instead
 */
static int cb_data_true(void *user_data, void *arg, void *data, int flags)
{
	return CMP_MATCH;
}

/*!
 * Browse the container using different stategies accoding the flags.
 * \return Is a pointer to an object or to a list of object if OBJ_MULTIPLE is 
 * specified.
 * Luckily, for debug purposes, the added args (tag, file, line, funcname)
 * aren't an excessive load to the system, as the callback should not be
 * called as often as, say, the ao2_ref func is called.
 */
static void *internal_obj_callback(struct obj_container *c,
				   const enum search_flags flags, void *cb_fn, void *arg, void *data, enum obj_callback_type type,
				   char *tag, char *file, int line, const char *funcname)
{
	int i, start, last;
	void *ret = NULL;
	obj_callback_fn *cb_default = NULL;
	obj_callback_data_fn *cb_withdata = NULL;
	struct obj_container *multi_container = NULL;
	struct obj_iterator *multi_iterator = NULL;

	if(internal_obj(c) == NULL) /* safety check on the argument */
		return NULL;

	/*
	 * This logic is used so we can support OBJ_MULTIPLE with OBJ_NODATA
	 * turned off.  This if statement checks for the special condition
	 * where multiple items may need to be returned.
	 */
	 if((flags & (OBJ_MULTIPLE | OBJ_NODATA)) == OBJ_MULTIPLE) {
		/* we need to return an obj_iterator with the results,
		 * as there could be more than one. the iterator will
		 * hold the only reference to a container that has all the
		 * matching objects linked into it, so when the iterator
		 * is destroyed, the container will be automatically
		 * destroyed as well.
		 */
		 if (!(multi_container = __obj_container_alloc(1, NULL, NULL))) {
			return NULL;
		}
		 if(!(multi_iterator = spd_calloc(1, sizeof(*multi_iterator)))) {
			obj_ref(multi_container, -1);
			return NULL;
		}
	 }

	 /* override the match function if necessary */
	 if(cb_fn == NULL) {
		if(type == WITH_DATA) {
			cb_withdata = cb_data_true;
		} else {
			cb_default = cb_true; 
		}
	 } else {

	 	/* We do this here to avoid the per object casting penalty (even though
		   that is probably optimized away anyway). */
		if(type == WITH_DATA) {
			cb_withdata = cb_fn;
		} else {
			cb_default = cb_fn;
		}
	 }

	 /*
	 * XXX this can be optimized.
	 * If we have a hash function and lookup by pointer,
	 * run the hash function. Otherwise, scan the whole container
	 * (this only for the time being. We need to optimize this.)
	 */
	 if((flags & OBJ_POINTER))
	 	start = i = c->hash_fn(arg, flags & OBJ_POINTER) % c->n_buckets;
	 else
	 	start = i = -1; /* don't know, let's scan all buckets */

	 /* determine the search boundaries: i..last-1 */
	 if(i < 0) {
		start = i = 0;
		last = c->n_buckets;
	 } else if((flags & OBJ_CONTINUE)) {
		last = c->n_buckets;
	 } else {
		last = i + 1;
	 }

	 if(!(flags & OBJ_NOLOCK)) {
		obj_lock(c); /* avoid modifications to the content */
	 }

	 for(; i < last; i++) {
		struct bucket_entry *cur;

		SPD_LIST_TRAVERSE_SAFE_BEGIN(&c->buckets[i], cur, entry) {
			int match = (CMP_MATCH | CMP_STOP);

			if(type == WITH_DATA) {
				match &= cb_withdata(EXTERNAL_OBJ(cur->pobj), arg, data, flags);
			} else {
				match & = cb_default(EXTERNAL_OBJ(cur->pobj), arg, flags);
			}
			
			/* we found the object, performing operations according flags */
			if (match == 0) {	/* no match, no stop, continue */
				continue;
			} else if (match == CMP_STOP) {	/* no match but stop, we are done */
				i = last;
				break;
			}

			/* we have a match (CMP_MATCH) here */
			if(!(flags &OBJ_NODATA)) {
				ret = EXTERNAL_OBJ(cur->pobj);
				if(!(flags & (OBJ_UNLINK | OBJ_MULTIPLE))) {
					if(tag)
						__obj_ref(ret,1);
					else
						__obj_ref(ret, 1);
				}
			}

			/* If we are in OBJ_MULTIPLE mode and OBJ_NODATE is off, 
			 * link the object into the container that will hold the results.
			 */
			 if (ret && (multi_container != NULL)) {
				if (tag) {
					__obj_link(multi_container, ret, flags);
				} else {
					__obj_link(multi_container, ret, flags);
				}
				ret = NULL;
			}

			if(flags & OBJ_UNLINK) {
				spd_atomic_fetchadd_int(&c->version, 1);
				SPD_LIST_REMOVE_CURRENT(entry, field)
			
				/* update number of elements */
				spd_atomic_fetchadd_int(&c->elements, -1);

				/* - When unlinking and not returning the result, (OBJ_NODATA), the ref from the container
				 * must be decremented.
				 * - When unlinking with OBJ_MULTIPLE the ref from the original container
				 * must be decremented regardless if OBJ_NODATA is used. This is because the result is
				 * returned in a new container that already holds its own ref for the object. If the ref
				 * from the original container is not accounted for here a memory leak occurs. */

				if(flags & (OBJ_NODATA | OBJ_MULTIPLE)) {
					obj_ref(EXTERNAL_OBJ(cur->pobj), -1);
				}
				spd_safe_free(cur);
	 		}	

			if ((match & CMP_STOP) || !(flags & OBJ_MULTIPLE)) {
				/* We found our only (or last) match, so force an exit from
				   the outside loop. */
				i = last;
				break;
			}
		}
		SPD_LIST_TRAVERSE_SAFE_END;

		if (ret) {
			break;
		}

		if (i == c->n_buckets - 1 && (flags & OBJ_POINTER) && (flags & OBJ_CONTINUE)) {
			/* Move to the beginning to ensure we check every bucket */
			i = -1;
			last = start;
		}
	}

	if (!(flags & OBJ_NOLOCK)) {
		obj_unlock(c);
	}

	/* if multi_container was created, we are returning multiple objects */
	if (multi_container != NULL) {
		*multi_iterator = obj_iterator_init(multi_container,
						    OBJ_ITERATOR_DONTLOCK | OBJ_ITERATOR_UNLINK | OBJ_ITERATOR_MALLOCD);
		obj_ref(multi_container, -1);
		return multi_iterator;
	} else {
		return ret;
	}
}

void *__obj_callback(struct obj_container * c, enum search_flags flags, obj_callback_fn cb_fn, void * arg)
{
	return internal_obj_callback(c,flags, cb_fn, arg, NULL, DEFAULT, NULL, NULL, 0, NULL);
}

void *__obj_callback_data(struct obj_container *c, const enum search_flags flags,
			  obj_callback_data_fn *cb_fn, void *arg, void *data)
{
	return internal_obj_callback(c, flags, cb_fn, arg, data, WITH_DATA, NULL, NULL, 0, NULL);
}

void *__obj_find(struct obj_container *c, void *arg, enum search_flags flags)
{
	return __obj_callback(c, flags, c->cmp_fn, arg);
}

/*!
 * initialize an iterator so we start from the first object
 */

struct obj_iterator obj_iterator_init(struct obj_container * c, int flags)
{
	struct obj_iterator a = {
		.c = c,
		.flags = flags
	};

	obj_ref(c, +1);

	return a;
}

/*!
 * destroy an iterator
 */
void obj_iterator_destory(struct obj_iterator * i)
{
	obj_ref(i, -1);
	if(i->flags & OBJ_ITERATOR_MALLOCD) {
		spd_safe_free(i);
	}
	i->c = NULL;
}

/*
 * move to the next element in the container.
 */
static void *internal_obj_iterator_next(struct obj_iterator *a, struct bucket_entry **q)
{
	int lim;
	struct bucket_entry *p = NULL;
	void *ret = NULL;

	*q = NULL;
	
	if (internal_obj(a->c) == NULL)
		return NULL;

	if (!(a->flags & OBJ_ITERATOR_DONTLOCK))
		obj_lock(a->c);

	/* optimization. If the container is unchanged and
	 * we have a pointer, try follow it
	 */
	if (a->c->version == a->c_version && (p = a->obj)) {
		if ((p = SPD_LIST_NEXT(p, entry)))
			goto found;
		/* nope, start from the next bucket */
		a->bucket++;
		a->version = 0;
		a->obj = NULL;
	}

	lim = a->c->n_buckets;

	/* Browse the buckets array, moving to the next
	 * buckets if we don't find the entry in the current one.
	 * Stop when we find an element with version number greater
	 * than the current one (we reset the version to 0 when we
	 * switch buckets).
	 */
	for (; a->bucket < lim; a->bucket++, a->version = 0) {
		/* scan the current bucket */
		SPD_LIST_TRAVERSE(&a->c->buckets[a->bucket], p, entry) {
			if (p->version > a->version)
				goto found;
		}
	}

found:
	if (p) {
		ret = EXTERNAL_OBJ(p->astobj);
		if (a->flags & OBJ_ITERATOR_UNLINK) {
			/* we are going to modify the container, so update version */
			spd_atomic_fetchadd_int(&a->c->version, 1);
			SPD_LIST_REMOVE(&a->c->buckets[a->bucket], p, entry);
			/* update number of elements */
			spd_atomic_fetchadd_int(&a->c->elements, -1);
			a->version = 0;
			a->obj = NULL;
			a->c_version = a->c->version;
			spd_safe_free(p);
		} else {
			a->version = p->version;
			a->obj = p;
			a->c_version = a->c->version;
			/* inc refcount of returned object */
			*q = p;
		}
	}

	return ret;
}

void *__obj_iterator_next(struct obj_iterator *a)
{
	struct bucket_entry *p = NULL;
	void *ret = NULL;

	ret = internal_obj_iterator_next(a, &p);
	
	if (p) {
		/* inc refcount of returned object */
		__obj_ref(ret, 1);
	}

	if (!(a->flags & OBJ_ITERATOR_DONTLOCK))
		obj_unlock(a->c);

	return ret;
}

/* callback for destroying container.
 * we can make it simple as we know what it does
 */
static int cd_cb(void *obj, void *arg, int flag)
{
	__obj_ref(obj, -1);
	return 0;
}

static void container_destruct(void *_c)
{
	struct obj_container *c = _c;
	int i;

	__obj_callback(c, OBJ_UNLINK, cd_cb, NULL);

	for (i = 0; i < c->n_buckets; i++) {
		struct bucket_entry *current;

		while ((current = SPD_LIST_REMOVE_HEAD(&c->buckets[i], entry))) {
			spd_safe_free(current);
		}
	}
}
