/**
 * @file ptl_buf.h
 *
 * Buf object methods.
 */

#include "ptl_loc.h"

/**
 * Setup a buf.
 *
 * Called each time the buf is allocated from the buf pool freelist.
 *
 * @param arg opaque reference to buf
 *
 * @return status
 */
int buf_setup(void *arg)
{
	buf_t *buf = arg;

	buf->num_mr = 0;
	buf->xt = NULL;
	buf->comp = 0;
	buf->data = buf->internal_data;
	buf->rdma.recv_wr.next = NULL;

	memset(&buf->xi, 0, sizeof(buf->xi));
	//	INIT_LIST_HEAD(&buf->xt.rdma_list);

	return PTL_OK;
}

/**
 * Cleanup a buf.
 *
 * Called each time buf is freed to the buf pool.
 *
 * @param arg opaque reference to buf
 */ 
void buf_cleanup(void *arg)
{
	buf_t *buf = arg;
	int i;

	for (i = 0; i < buf->num_mr; i++)
		mr_put(buf->mr_list[i]);

	if (buf->xt)
		xt_put(buf->xt);

	buf->type = BUF_FREE;
}

/**
 * Init a buf.
 *
 * Called once when buf is created.
 * Sets information that is constant for the life
 * of the buf.
 *
 * @param buf which to init
 * @param parm parameter passed that contains the mr
 *
 * @return status
 */
int buf_init(void *arg, void *parm)
{
	buf_t *buf = arg;

	INIT_LIST_HEAD(&buf->list);

	buf->length = 0;
	buf->type = BUF_FREE;

	if (parm) {
		/* This buffer carries an MR, so it's an IB buffer, not a
		 * buffer in shared memory. */
		struct ibv_mr *mr = parm;

		buf->rdma.send_wr.next = NULL;
		buf->rdma.send_wr.wr_id = (uintptr_t)buf;
		buf->rdma.send_wr.sg_list = buf->rdma.sg_list;
		buf->rdma.send_wr.num_sge = 1;

		buf->rdma.sg_list[0].addr = (uintptr_t)buf->internal_data;
		buf->rdma.sg_list[0].lkey = mr->lkey;
	}

	//	pthread_spin_init(&buf->xi.rdma_list_lock, PTHREAD_PROCESS_PRIVATE);

	return 0;
}

#ifdef todo
int buf_fini(void *arg, void *parm)
{
	xi_t *xi = arg;

	pthread_spin_destroy(&xi->rdma_list_lock);
}
#endif


/**
 * Debug print buf parameters.
 *
 * @param buf which to dump
 */
void buf_dump(buf_t *buf)
{
	hdr_t *hdr = (hdr_t *)buf->data;

	printf("buf: %p\n", buf);
	printf("buf->size	= %d\n", BUF_DATA_SIZE);
	printf("buf->length	= %d\n", buf->length);
	printf("hdr->version	= %d\n", hdr->version);
	printf("hdr->operation	= %d\n", hdr->operation);
	printf("hdr->ni_type	= %d\n", hdr->ni_type);
	printf("hdr->pkt_fmt	= %d\n", hdr->pkt_fmt);
	printf("hdr->length	= %" PRId64 "\n", le64_to_cpu(hdr->length));
	printf("\n");
}

/**
 * Post a receive buffer.
 *
 * Used to post receive buffers to the OFA verbs
 * shared receive queue (SRQ). A buf is allocated
 * from the ni's normal buf pool which takes a reference.
 * The buf is initialized as a receive buffer and is
 * posted to the SRQ. The buf is added to the ni
 * recv_list in the order that it was posted.
 *
 * @param ni for which to post receive buffer
 * @param count the desired number of buffers to post
 *
 * @return status
 */
int ptl_post_recv(ni_t *ni, int count)
{
	int err;
	buf_t *buf;
	struct ibv_recv_wr *bad_wr;
	int actual;
	struct ibv_recv_wr *wr;
	struct list_head list;

	if (count == 0)
		return PTL_OK;

	INIT_LIST_HEAD(&list);
	wr = NULL;

	for (actual = 0; actual < count; actual++) {
		err = buf_alloc(ni, &buf);
		if (err)
			break;

		buf->rdma.sg_list[0].length = BUF_DATA_SIZE;
		buf->type = BUF_RECV;
		buf->rdma.recv_wr.next = wr;
		wr = &buf->rdma.recv_wr;

		list_add_tail(&buf->list, &list);
	}

	/* couldn't alloc any buffers */
	if (!actual) {
		WARN();
		return PTL_FAIL;
	}

	/* add buffers to ni recv_list for recovery during shutdown */
	pthread_spin_lock(&ni->rdma.recv_list_lock);
	list_splice_tail(&list, &ni->rdma.recv_list);
	pthread_spin_unlock(&ni->rdma.recv_list_lock);

	/* account for posted buffers */
	(void)__sync_fetch_and_add(&ni->rdma.num_posted_recv, actual);

	err = ibv_post_srq_recv(ni->rdma.srq, &buf->rdma.recv_wr, &bad_wr);
	if (err) {
		WARN();

		/* re-stock any unposted buffers */
		pthread_spin_lock(&ni->rdma.recv_list_lock);
		for (wr = bad_wr; wr; wr = wr->next) {
			buf = container_of(wr, buf_t, rdma.recv_wr);
			list_del(&buf->list);
			buf_put(buf);

			/* account for failed buffers */
			(void)__sync_fetch_and_sub(&ni->rdma.num_posted_recv, 1);
		}
		pthread_spin_unlock(&ni->rdma.recv_list_lock);
	}

	return PTL_OK;
}