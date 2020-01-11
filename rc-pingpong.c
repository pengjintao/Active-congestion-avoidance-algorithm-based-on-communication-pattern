#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <sys/time.h>
#include "rc-pingpong.h"
#include "IphCommTrace.h"
#include "Iph_Ib_comm.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <byteswap.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <mpi.h>
#include <time.h>
#include <inttypes.h>
#include <endian.h>

#define MAX_POLL_CQ_TIMEOUT 2000
static int msg_size = 4096;
static int proc_per_node = 1;
static int page_size;

struct resources
{
    struct ibv_context *context;
    struct ibv_comp_channel *channel;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_dm *dm;
    union {
        struct ibv_cq *cq;
        struct ibv_cq_ex *cq_ex;
    } cq_s;
    struct ibv_qp *qp;
    struct ibv_qp_ex *qpx;
    char *buf;
    int size;
    int send_flags;
    int rx_depth;
    int pending;
    struct ibv_port_attr portinfo;
    uint64_t completion_timestamp_mask;
    union ibv_gid my_gid;
    struct cm_con_data_t remote_props;
};
static struct config_t config =
    {
        NULL, /* dev_name */
        NULL, /* server_name */
        1,    /* ib_port */
        -1    /* gid_idx */
};
static struct resources *pp_init_ctx(struct ibv_device *ib_dev, int size,
                                     int rx_depth, int port,
                                     int use_event)
{
    struct resources *ctx = 0;
    int access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
    int tempn = sizeof(struct resources); // ctx;
    int cqe = 1000;
    ctx = (struct resources *)malloc(tempn * sizeof(char));
    if (!ctx)
        return NULL;
    ctx->size = size;
    ctx->send_flags = IBV_SEND_SIGNALED;
    ctx->rx_depth = rx_depth;
    ctx->buf = (char *)memalign(page_size, size);
    if (!ctx->buf)
    {
        fprintf(stderr, "Couldn't allocate work buf.\n");
        goto clean_ctx;
    }
    //open device
    ctx->context = ibv_open_device(ib_dev);
    if (!ctx->context)
    {
        fprintf(stderr, "Couldn't get context for %s\n",
                ibv_get_device_name(ib_dev));
        goto clean_buffer;
    }
    ctx->channel = NULL;
    //protection domain
    ctx->pd = ibv_alloc_pd(ctx->context);
    if (!ctx->pd)
    {
        fprintf(stderr, "Couldn't allocate PD\n");
        goto clean_comp_channel;
    }
    //init memory regiion
    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size, access_flags);
    if (!ctx->mr)
    {
        fprintf(stderr, "Error, ibv_reg_mr() failed\n");
        return 0;
    }
    //create CQ channel
    ctx->channel = ibv_create_comp_channel(ctx->context);
    if (!ctx->channel)
    {
        fprintf(stderr, "Error, ibv_create_comp_channel() failed\n");
        return 0;
    }
    //create CQ
    ctx->cq_s.cq = ibv_create_cq(ctx->context, cqe, NULL, ctx->channel, 0);
    if (!ctx->cq_s.cq)
    {
        fprintf(stderr, "Error, ibv_create_cq() failed\n");
        return 0;
    }
    // create qp
    struct ibv_qp_init_attr init_attr;
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.qp_type = IBV_QPT_RC;
    init_attr.sq_sig_all = 1;
    init_attr.send_cq = ctx->cq_s.cq;
    init_attr.recv_cq = ctx->cq_s.cq;
    init_attr.cap.max_send_wr = 1;
    init_attr.cap.max_recv_wr = 500;
    init_attr.cap.max_send_sge = 1;
    init_attr.cap.max_recv_sge = 1;
    ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
    if (!ctx->qp)
    {
        fprintf(stderr, "Error, ibv_create_qp() failed\n");
        return 0;
    }

    if (ibv_query_port(ctx->context, port, &ctx->portinfo) != 0)
    {
        fprintf(stderr, "Error, failed to query port  attributes in device \n");
        printf("errno = %d\n", errno);
        return 0;
    }
    return ctx;
    /*
clean_qp:
ibv_destroy_qp(ctx->qp);

clean_cq:
ibv_destroy_cq(ctx->cq_s.cq);

clean_mr:
ibv_dereg_mr(ctx->mr);

clean_pd:
ibv_dealloc_pd(ctx->pd);

clean_device:
ibv_close_device(ctx->context);
*/
clean_comp_channel:
    if (ctx->channel)
        ibv_destroy_comp_channel(ctx->channel);
clean_buffer:
    free(ctx->buf);

clean_ctx:
    free(ctx);
    return NULL;
}

/******************************************************************************
* Function: modify_qp_to_init
*
* Input
* qp QP to transition
*
* Output
* none
*
* Returns
* 0 on success, ibv_modify_qp failure code on failure
*
* Description
* Transition a QP from the RESET to INIT state
******************************************************************************/
static int modify_qp_to_init(struct ibv_qp *qp)
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = config.ib_port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
        fprintf(stderr, "failed to modify QP state to INIT\n");
    return rc;
}
/******************************************************************************
* Function: modify_qp_to_rtr
*
* Input
* qp QP to transition
* remote_qpn remote QP number
* dlid destination LID
* dgid destination GID (mandatory for RoCEE)
* Output
* none
*
* Returns
* 0 on success, ibv_modify_qp failure code on failure
*
* Description
* Transition a QP from the INIT to RTR state, using the specified QP number
******************************************************************************/
static int modify_qp_to_rtr(struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid, uint8_t *dgid)
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_4096;
    attr.dest_qp_num = remote_qpn;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dlid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = config.ib_port;
    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
            IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
    {
        fprintf(stderr, "failed to modify QP state to RTR\n");
        printf("errno = %d\n", errno);
    }
    return rc;
}

/******************************************************************************
* Function: modify_qp_to_rts
*
* Input
* qp QP to transition
*
* Output
* none
*
* Returns
* 0 on success, ibv_modify_qp failure code on failure
*
* Description
* Transition a QP from the RTR to RTS state
******************************************************************************/
static int modify_qp_to_rts(struct ibv_qp *qp)
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x12;
    attr.retry_cnt = 6;
    attr.rnr_retry = 0;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;
    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
    {
        fprintf(stderr, "failed to modify QP state to RTS A\n");
        printf("errno = %d\n", errno);
    }
    return rc;
}
/******************************************************************************
* Function: post_receive
*
* Input
* res pointer to resources structure
** Output
* none
*
* Returns
* 0 on success, error code on failure
*
* Description
*
******************************************************************************/
static int post_receive(struct resources *res)
{
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    int rc;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)res->buf;
    sge.length = msg_size;
    sge.lkey = res->mr->lkey;
    /* prepare the receive work request */
    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    rr.wr_id = 0;
    rr.sg_list = &sge;
    rr.num_sge = 1;
    /* post the Receive Request to the RQ */
    rc = ibv_post_recv(res->qp, &rr, &bad_wr);
    if (rc)
        fprintf(stderr, "failed to post RR\n");
    else
        fprintf(stdout, "Receive Request was posted\n");
    return rc;
}

static struct cm_con_data_t local_con_data;
static struct cm_con_data_t remote_con_data;
/******************************************************************************
* Function: post_send
*
* Input
* res pointer to resources structure
* opcode IBV_WR_SEND, IBV_WR_RDMA_READ or IBV_WR_RDMA_WRITE
*
* Output
* none
*
* Returns
* 0 on success, error code on failure
*
* Description
* This function will create and post a send work request
******************************************************************************/
static int post_send(struct resources *res, int opcode,int my_id)
{
    //puts("post_send");
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)res->buf;
    sge.length = msg_size;
    sge.lkey = res->mr->lkey;
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = (enum ibv_wr_opcode)opcode;
    sr.send_flags = IBV_SEND_SIGNALED;
    if (opcode != IBV_WR_SEND)
    {
        sr.wr.rdma.remote_addr = remote_con_data.addr;
        sr.wr.rdma.rkey = remote_con_data.rkey;
        //printf("rank = %d   remote_addr = 0x%x rkey = 0x%x\n",my_id,sr.wr.rdma.remote_addr,sr.wr.rdma.rkey);
    }
    
    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(res->qp, &sr, &bad_wr);
    if (rc != IBV_WC_SUCCESS)
        fprintf(stderr, "failed to post SR errno = %d\n", errno);
    // else
    // {
    //     switch (opcode)
    //     {
    //     case IBV_WR_SEND:
    //         fprintf(stdout, "Send Request was posted\n");
    //         break;
    //     case IBV_WR_RDMA_READ:
    //         fprintf(stdout, "RDMA Read Request was posted\n");
    //         break;
    //     case IBV_WR_RDMA_WRITE:
    //         fprintf(stdout, "RDMA Write Request was posted\n");
    //         break;
    //     default:
    //         fprintf(stdout, "Unknown Request was posted\n");
    //         break;
    //     }
    // }
    return rc;
}

/******************************************************************************
End of socket operations
******************************************************************************/
/* poll_completion */
/******************************************************************************
* Function: poll_completion
*
* Input
* res pointer to resources structure
*
* Output
* none
*
* Returns
* 0 on success, 1 on failure
*
* Description
* Poll the completion queue for a single event. This function will continue to
* poll the queue until MAX_POLL_CQ_TIMEOUT milliseconds have passed.
*
******************************************************************************/
static int poll_completion(struct resources *res)
{
    struct ibv_wc wc;
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    int rc = 0;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    do
    {
        poll_result = ibv_poll_cq(res->cq_s.cq, 1, &wc);
    } while (poll_result == 0);
    if (poll_result < 0)
    {
        /* poll CQ failed */
        fprintf(stderr, "poll CQ failed\n");
        rc = 1;
    }
    else if (poll_result == 0)
    { /* the CQ is empty */
        fprintf(stderr, "completion wasn't found in the CQ after timeout\n");
        rc = 1;
    }
    else
    {
        /* CQE found */
        //fprintf(stdout, "completion was found in CQ with status 0x%x\n", wc.status);
        //printf("result = %s\n",res->buf);
        /* check the completion status (here we don't care about the completion opcode */
        if (wc.status != IBV_WC_SUCCESS)
        {
            if (wc.status == IBV_WC_LOC_EEC_OP_ERR)
                printf("IBV_WC_LOC_EEC_OP_ERR ");
            else if (wc.status == IBV_WC_WR_FLUSH_ERR)
            {
                printf("IBV_WC_WR_FLUSH_ERR ");
            }
            else if (wc.status == IBV_WC_RETRY_EXC_ERR)
            {
                printf("IBV_WC_RETRY_EXC_ERR ");
            }
            fprintf(stderr, "got bad completion with status: %d, vendor syndrome: %d\n", wc.status,
                    wc.vendor_err);
            rc = 1;
        }
    }
    return rc;
}
int connect_qp(struct resources *res, int my_id)
{
    int rc = 0;
    char temp_char;
    union ibv_gid my_gid;
    memset(&my_gid, 0, sizeof my_gid);
    local_con_data.addr = (uintptr_t)res->buf;
    local_con_data.rkey = res->mr->rkey;
    local_con_data.qp_num = res->qp->qp_num;
    local_con_data.lid = res->portinfo.lid;
    memcpy(local_con_data.gid, &my_gid, 16);
    MPI_Status status;
    if (my_id == proc_per_node * 0)
    {
        MPI_Send(&local_con_data, sizeof(local_con_data), MPI_CHAR, 1 * proc_per_node, 0, MPI_COMM_WORLD);
        MPI_Recv((void *)&remote_con_data, sizeof(remote_con_data), MPI_CHAR, 1 * proc_per_node, 0, MPI_COMM_WORLD, &status);

        // if(0)
        // {
        //     uint8_t *p = (uint8_t *)&remote_con_data.gid;
        //     fprintf(stdout, "rand = %d remote address = 0x%x remote rkey = 0x%x remote QP number = 0x%x remote LID = 0x%x\n ",
        //             my_id,remote_con_data.addr,remote_con_data.rkey,remote_con_data.qp_num,remote_con_data.lid);
        // }
    }
    else if (my_id == proc_per_node * 1)
    {
        if(0) 
        {
            uint8_t *p = (uint8_t *)&my_gid;
            fprintf(stdout, "rand = %d Local address = 0x%x Local rkey = 0x%x Local QP number = 0x%x Local LID = 0x%x\n",
                    my_id,local_con_data.addr,local_con_data.rkey,local_con_data.qp_num,local_con_data.lid);
        }
        MPI_Recv((void *)&remote_con_data, sizeof(remote_con_data), MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
        MPI_Send(&local_con_data, sizeof(local_con_data), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        if(0)
        {
            uint8_t *p = (uint8_t *)&remote_con_data.gid;
            fprintf(stdout, "rand = %d remote address = 0x%x remote rkey = 0x%x remote QP number = 0x%x remote LID = 0x%x\n",
                    my_id,remote_con_data.addr,remote_con_data.rkey,remote_con_data.qp_num,remote_con_data.lid);
        }
    }
    /* modify the QP to init */
    rc = modify_qp_to_init(res->qp);
    if (rc)
    {
        fprintf(stderr, "change QP state to INIT failed\n");
        printf("errno = %d\n", errno);
        goto connect_qp_exit;
    }
    /* modify the QP to RTR */
    rc = modify_qp_to_rtr(res->qp, remote_con_data.qp_num, remote_con_data.lid, remote_con_data.gid);
    if (rc)
    {
        fprintf(stderr, "failed to modify QP state to RTR\n");
        printf("errno = %d\n", errno);
        goto connect_qp_exit;
    }
    rc = modify_qp_to_rts(res->qp);
    if (rc)
    {
        fprintf(stderr, "rank = %d failed to modify QP state to RTS\n", my_id);
        printf("errno = %d\n", errno);
        goto connect_qp_exit;
    }
    return rc;
connect_qp_exit:
    return rc;
}

int ping_pong(int my_id,int pingpong_size)
{
    msg_size = pingpong_size;
    char *ib_devname = NULL;
    int nprocs, rank;
    //MPI_Comm_size(Iph_comm,&nprocs);
    //MPI_Comm_rank(Iph_comm,&rank);
    page_size = sysconf(_SC_PAGESIZE);
    if (my_id % proc_per_node == 0)
    {
        struct ibv_device **dev_list;
        struct ibv_device *ib_dev;
        struct resources *ctx;
        int num_devices, i;
        int ib_port = config.ib_port;
        unsigned int rx_depth = 800;
        int use_event = 0;
        int size = msg_size;
        dev_list = ibv_get_device_list(&num_devices);
        if (!dev_list)
        {
            perror("Failed to get IB devices list");
            return 1;
        }

        if (!ib_devname)
        {
            ib_dev = *dev_list;
            if (!ib_dev)
            {
                fprintf(stderr, "No IB devices found\n");
                return 1;
            }
        }
        else
        {
            int i;
            for (i = 0; dev_list[i]; ++i)
                if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname))
                    break;
            ib_dev = dev_list[i];
            if (!ib_dev)
            {
                fprintf(stderr, "IB device %s not found\n", ib_devname);
                return 1;
            }
        }
        ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event);
        /* connect the QPs */
        if (connect_qp(ctx, my_id))
        {
            fprintf(stderr, "failed to connect QPs\n");
        }
        struct timespec time_start,time_end;
        double startT,endT;
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        clock_gettime(CLOCK_REALTIME,&time_start);
        startT = MPI_Wtime();
        /* let the server post the sr */
        int loopN = 1000;
        if (my_id == 1)
        {
            int loop = 0;
            for (loop = 0; loop < loopN; loop++)
            {
                if (post_send(ctx, IBV_WR_RDMA_WRITE,my_id))
                {
                    fprintf(stderr, "failed to post SR 21\n");
                }
                if (poll_completion(ctx))
                {
                    fprintf(stderr, "poll completion failed 2\n");
                }
                if (post_send(ctx, IBV_WR_RDMA_READ,my_id))
                {
                    fprintf(stderr, "failed to post sr\n");
                }

                if (poll_completion(ctx))
                {
                    fprintf(stderr, "poll completion failed 3\n");
                }
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        clock_gettime(CLOCK_REALTIME,&time_end);
        endT = MPI_Wtime();
        MPI_Barrier(MPI_COMM_WORLD);
        double time_us = ((time_end.tv_sec - time_start.tv_sec)*1000000.0 + (time_end.tv_nsec -time_start.tv_nsec)/1000.0)/loopN;
        double timeMPI = (1000000.0*(endT - startT))/loopN;
        double time;
	    MPI_Reduce(&time_us, &time, 1,MPI_DOUBLE,
               MPI_MAX, 0, MPI_COMM_WORLD);
        if (my_id == 0)
        {
            printf("%f\n", time);
        }
	    MPI_Reduce(&timeMPI, &time, 1,MPI_DOUBLE,
               MPI_MAX, 0, MPI_COMM_WORLD);
        // if (my_id == 0)
        // {
        //     printf("%f\n", time);
        // }
        if (ibv_destroy_qp(ctx->qp))
        {
            fprintf(stderr, "Error, ibv_destroy_qp() failed\n");
            printf("errno = %d\n", errno);
            return -1;
        }
        if (ibv_destroy_cq(ctx->cq_s.cq))
        {
            fprintf(stderr, "Error, ibv_destroy_cq() failed\n");
            printf("errno = %d\n", errno);
            return -1;
        }
        if (ibv_dereg_mr(ctx->mr))
        {
            fprintf(stderr, "Error, ibv_dereg_mr() failed\n");
            printf("errno = %d\n", errno);
            return -1;
        }
        if (ibv_dealloc_pd(ctx->pd))
        {
            fprintf(stderr, "Error, ibv_dealloc_pd() failed\n");
            printf("errno = %d\n", errno);
            return -1;
        }
        if (ibv_destroy_comp_channel(ctx->channel))
        {
            fprintf(stderr, "Error, ibv_destroy_comp_channel() failed\n");
            printf("errno = %d\n", errno);
            return -1;
        }
        ibv_close_device(ctx->context);
        ibv_free_device_list(dev_list);
    }
    else
    {
    }
    MPI_Barrier(MPI_COMM_WORLD);
}