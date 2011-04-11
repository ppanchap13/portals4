/*
 * ptl_obj.h
 */

#ifndef PTL_OBJ_H
#define PTL_OBJ_H

struct ni;

enum obj_type_index {
	OBJ_TYPE_NONE,
	OBJ_TYPE_NI,
	OBJ_TYPE_PT,
	OBJ_TYPE_MR,
	OBJ_TYPE_LE,
	OBJ_TYPE_ME,
	OBJ_TYPE_MD,
	OBJ_TYPE_EQ,
	OBJ_TYPE_CT,
	OBJ_TYPE_XI,
	OBJ_TYPE_XT,
	OBJ_TYPE_BUF,
	OBJ_TYPE_LAST,		/* keep me last */
};

/*
 * segment_t
 *	per segment info
 */
typedef struct segment {
	void			*addr;
	void			*priv;
} segment_t;

/*
 * segment_list_t
 *	holds a list of segment pointers that
 *	point to allocated segments
 */
typedef struct segment_list {
	struct list_head	chunk_list;
	unsigned int		max_segments;
	unsigned int		num_segments;
	segment_t		segment_list[0];
} segment_list_t;

/*
 * obj_type_t
 *	per object type info
 */
typedef struct obj_type {
	struct obj		*parent;
	char			*name;
	int			(*init)(void *arg, void *parm);
	void			(*fini)(void *arg);
	int			(*alloc)(void *arg);
	void			(*free)(void *arg);
	struct list_head	chunk_list;
	struct list_head	free_list;
	pthread_spinlock_t	free_list_lock;
	enum obj_type_index	type;
	int			count;
	int			size;
	int			round_size;
	int			segment_size;
	int			obj_per_segment;
} obj_type_t;

/*
 * obj_t
 *	common per object info
 */
typedef struct obj {
	int			obj_free;
	obj_type_t		*obj_type;
	struct obj		*obj_parent;
	struct ni		*obj_ni;
	ptl_handle_any_t	obj_handle;
	pthread_spinlock_t	obj_lock;
	ref_t			obj_ref;
	struct list_head	obj_list;
} obj_t;

/*
 * obj_type_init
 *	initialize a pool of objects
 */
int obj_type_init(obj_type_t *pool, char *name, int size,
		  int type, obj_t *parent);

/*
 * obj_type_fini
 *	finalize a pool of objects
 */
void obj_type_fini(obj_type_t *type);

/*
 * obj_release
 *	called when last reference is dropped by obj_put
 *	releases object to free list
 */
void obj_release(ref_t *ref);

/*
 * obj_alloc
 *	allocate a new object of given type and optional
 *	parent. If parent is specified takes a reference
 *	on parent. Takes a reference on the object.
 */
int obj_alloc(obj_type_t *pool, obj_t **p_obj);

/*
 * obj_get
 *	lookup object from its handle. If type is specified
 *	and handle has a type set then they must match
 *	takes a reference on the object
 */
int obj_get(unsigned int type, ptl_handle_any_t handle, obj_t **obj_p);

/*
 * obj_ref
 *	take a reference on an object
 */
static inline void obj_ref(obj_t *obj)
{
	if (obj) ref_get(&obj->obj_ref);
}

/*
 * obj_put
 *	drop a reference to an object
 */
static inline int obj_put(obj_t *obj)
{
	return obj ? ref_put(&obj->obj_ref, obj_release) : 0;
}

#define HANDLE_INDEX_MASK	(0xffffffff)

/*
 * obj_handle_to_index
 *	convert a handle to an object index
 */
static inline unsigned int obj_handle_to_index(ptl_handle_any_t handle)
{
	return handle & HANDLE_INDEX_MASK;
}

#endif /* PTL_OBJ_H */