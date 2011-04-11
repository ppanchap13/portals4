/*
 * ptl_ni.h
 */

#ifndef PTL_NI_H
#define PTL_NI_H

/* These values will need to come from runtime environment */
#define MAX_QP_SEND_WR		(10)
#define MAX_QP_SEND_SGE		(16) // Best if >= MAX_INLINE_SGE
#define MAX_QP_RECV_WR		(10)
#define MAX_QP_RECV_SGE		(10)
#define MAX_SRQ_RECV_WR		(100)

struct ni;

/* Describes the current state of a connection with a remote rank or node. */
struct nid_connect {
	/* Destination. NID is used for both logical and physical. PID is only used for
	 * physical. rank is not used. May be replace with nid/pid instead
	 * to avoid confusion. */
	ptl_process_t id;	/* keep me first */

	pthread_mutex_t	mutex;

	struct ni *ni;				/* backpointer to owner */

	/* Used for logical NI only.
	 *
	 * For receive side: links the receiving NI together. For send
	 * side, used to wait until main rank is connected to. */
	struct list_head list;

	enum {
		GBLN_DISCONNECTED,
		GBLN_RESOLVING_ADDR,
		GBLN_RESOLVING_ROUTE,
		GBLN_CONNECT,
		GBLN_CONNECTING,
		GBLN_CONNECTED,
	} state;

	/* CM */
	struct rdma_cm_id *cm_id;
	struct sockaddr_in sin;		/* IPV4 address, in network order */

	int retry_resolve_addr;
	int retry_resolve_route;
	int retry_connect;

	/* xi/xt awaiting connection establishment. In case of logical NI,
	 * they will only hold something if the rank is not the main rank
	 * and the main rank is not yet connected. */
	struct list_head xi_list;
	struct list_head xt_list;

	/* For logical NI only. There's only one connection, with the
	 * main rank on the remote node. */
	struct nid_connect *main_connect;
};

/* Remote rank. There's one record per rank. Logical NIs only. */
struct rank_entry {
	ptl_rank_t rank;
	ptl_rank_t main_rank;		/* main rank on NID */
	ptl_nid_t nid;
	ptl_pid_t pid;
	uint32_t remote_xrc_srq_num;
	struct nid_connect connect;
};

/*
 * per NI info
 */
typedef struct ni {
	obj_t			obj;

	gbl_t			*gbl;

	rt_t			rt;

	ptl_ni_limits_t		limits;
	ptl_ni_limits_t		current;

	int			ref_cnt;

	struct iface		*iface;
	unsigned int		ifacenum;
	unsigned int		options;
	unsigned int		ni_type;

	ptl_sr_value_t		status[_PTL_SR_LAST];

	ptl_size_t		num_recv_pkts;
	ptl_size_t		num_recv_bytes;
	ptl_size_t		num_recv_errs;
	ptl_size_t		num_recv_drops;

	pt_t			*pt;
	pthread_mutex_t		pt_mutex;
	ptl_pt_index_t		last_pt;

	struct list_head	md_list;
	pthread_spinlock_t	md_list_lock;

	struct list_head	ct_list;
	pthread_spinlock_t	ct_list_lock;

	struct list_head	xi_wait_list;
	pthread_spinlock_t	xi_wait_list_lock;

	struct list_head	xt_wait_list;
	pthread_spinlock_t	xt_wait_list_lock;

	struct list_head	mr_list;
	pthread_spinlock_t	mr_list_lock;

	/* Can be held outside of EQ object lock */
	pthread_mutex_t		eq_wait_mutex;
	pthread_cond_t		eq_wait_cond;
	int			eq_waiting;

	/* Can be held outside of CT object lock */
	pthread_mutex_t		ct_wait_mutex;
	pthread_cond_t		ct_wait_cond;
	int			ct_waiting;

	/* Pending send and receive operations. */
	struct list_head	send_list;
	pthread_spinlock_t	send_list_lock;

	struct list_head	recv_list;
	pthread_spinlock_t	recv_list_lock;

	/* NI identifications */
	ptl_process_t		id;
	ptl_uid_t		uid;

	/* IB */
	struct ibv_cq		*cq;
	struct ibv_comp_channel	*ch;
	ev_io			cq_watcher;
	struct ibv_srq		*srq;	/* either regular or XRC */

	obj_type_t		mr_pool;
	obj_type_t		md_pool;
	obj_type_t		me_pool;
	obj_type_t		le_pool;
	obj_type_t		eq_pool;
	obj_type_t		ct_pool;
	obj_type_t		xi_pool;
	obj_type_t		xt_pool;
	obj_type_t		buf_pool;

	/* Connection mappings. */
	union {
		struct {
			/* Logical NI. */

			/* On a NID, the process creating the domain is going to
			 * be the one with the lowest PID. Connection attempts to
			 * the other PIDs will be rejected. Also, locally, the
			 * XI/XT will not be queued on the non-main ranks, but on
			 * the main rank. */
			int is_main;
			int main_rank;

			/* Rank table. This is used to connection TO remote ranks */
			int map_size;
			struct rank_entry *rank_table;

			/* Connection list. This is a set of passive connections,
			 * used for connections FROM remote ranks. */
			pthread_mutex_t lock;
			struct list_head connect_list;

			/* IB XRC support. */
			int			xrc_domain_fd;
			struct ibv_xrc_domain	*xrc_domain;
			uint32_t		xrc_rcv_qpn;
	
		} logical;
		struct {
			/* Physical NI. */

			void *tree;			/* binary tree root, of nid_connect elements */
			pthread_mutex_t lock;
		} physical;
	};


} ni_t;

static inline int ni_alloc(obj_type_t *type, ni_t **ni_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(type, &obj);
	if (err) {
		*ni_p = NULL;
		return err;
	}

	*ni_p = container_of(obj, ni_t, obj);
	return PTL_OK;
}

static inline void ni_ref(ni_t *ni)
{
	obj_ref(&ni->obj);
}

static inline int ni_put(ni_t *ni)
{
	return obj_put(&ni->obj);
}

static inline int ni_get(ptl_handle_ni_t handle, ni_t **ni_p)
{
	int err;
	obj_t *obj;
	ni_t *ni;

	err = obj_get(OBJ_TYPE_NI, (ptl_handle_any_t)handle, &obj);
	if (err)
		goto err;

	ni = container_of(obj, ni_t, obj);

	/* this is because we can call PtlNIFini
	   and still get the object if someone
	   is holding a reference */
	if (ni && ni->ref_cnt <= 0) {
		ni_put(ni);
		err = PTL_ARG_INVALID;
		goto err;
	}

	*ni_p = ni;
	return PTL_OK;

err:
	*ni_p = NULL;
	return err;
}

static inline ptl_handle_ni_t ni_to_handle(ni_t *ni)
{
	return (ptl_handle_ni_t)ni->obj.obj_handle;
}

static inline ni_t *to_ni(void *obj)
{
	return ((obj_t *)obj)->obj_ni;
}

static inline void ni_inc_status(ni_t *ni, ptl_sr_index_t index)
{
	if (index < _PTL_STATUS_LAST) {
		pthread_spin_lock(&ni->obj.obj_lock);
		ni->status[index]++;
		pthread_spin_unlock(&ni->obj.obj_lock);
	}
}

int init_connect(ni_t *ni, struct nid_connect *connect);

#endif /* PTL_NI_H */