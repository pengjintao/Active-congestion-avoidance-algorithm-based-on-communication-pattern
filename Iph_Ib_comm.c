#include "Iph_Ib_comm.h"
#include <arpa/inet.h>
#include <stdlib.h>
#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include "IphCommTrace.h"
#include "proc.h"
using namespace std;
//#define USING_RDMA_READ 1
#define USING_RDMA_WRITE 1
int page_size;
#define IB_PORT 1
struct config_t config =
{
		NULL, /* dev_name */
		NULL, /* server_name */
		1,	/* ib_port */
		-1	/* gid_idx */
};
struct IPH_resources;
struct IPH_resources
{
    struct ibv_context *context;
    struct ibv_comp_channel *channel;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_srq *srq;
    struct ibv_qp *qp[MAX_QP];
    int qpNum;
    struct ibv_device_attr  dev_cap;
    void *buf;
    int size;
    int send_flags;
    int num_qp;
    int rx_depth;
    int pending[MAX_QP];
    struct ibv_port_attr portinfo;
    IphComm *schd;
};
static struct IPH_resources *iph_init_ctx(struct ibv_device *ib_dev,
                                          int rx_depth, int port,
                                          int use_event,int rank,int nprocs)
{
    int num_qp = nprocs;
    struct IPH_resources *ctx;
    int i = 0;
    ctx = (struct IPH_resources *)calloc(1, sizeof *ctx);
    ctx->send_flags = IBV_SEND_SIGNALED;
    ctx->rx_depth = rx_depth;

    //open device
    ctx->context = ibv_open_device(ib_dev);
    if (!ctx->context)
    {
        fprintf(stderr, "Couldn't get context for %s\n",
                ibv_get_device_name(ib_dev));
        puts("error open device");
    }
    int ret = ibv_query_device(ctx->context, &ctx->dev_cap);
    if (ret) {
        printf("error:ibv_query_device\n");
        return 0;
    }
    if (use_event)
    {
        //为RDMA设备上下文context创建一个完成事件通道。
        //“完成通道”是libibverbs引入的抽象，在InfiniBand体系结构动词规范或RDMA协议动词规范中不存在。
        //完成通道本质上是文件描述符，用于将完成通知传递给用户空间进程。当为完成队列（CQ）生成完成事件时，该事件通过附加到该CQ的完成通道传递。
        //这对于通过使用多个完成通道将完成事件引导到不同线程可能很有用。
        ctx->channel = ibv_create_comp_channel(ctx->context);
        if (!ctx->channel)
        {
            fprintf(stderr, "Couldn't create completion channel\n");
            goto clean_device;
        }
    }
    else
        ctx->channel = NULL;

    //分配或取消保护域（PD）
    /*
    创建的PD将用于：创建AH，SRQ，QP 注册MR  分配MW
    PD是一种保护手段，可帮助您创建可以一起工作的一组对象。如果使用PD1创建了几个对象，而使用PD2创建了其他对象，则将来自group1的对象与来自group2的对象一起使用将最终以错误结束。
    */
    ctx->pd = ibv_alloc_pd(ctx->context);
    if (!ctx->pd)
    {
        fprintf(stderr, "Couldn't allocate PD\n");
        goto clean_comp_channel;
    }
    //对每一个消息都要注册内存
    for(i = 0;i< msgn;i++)
    {
        if(rank == schd[i].source)
        {
            //printf("%s\n",schd[i].bufS);
            schd[i].mr = ibv_reg_mr(ctx->pd, schd[i].bufS, schd[i].size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
        }else if (rank == schd[i].target){
            //printf("%s\n",schd[i].bufR);
            schd[i].mr = ibv_reg_mr(ctx->pd, schd[i].bufR, schd[i].size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
        }else
        {
            puts("error message");
        }
        if (!schd[i].mr) {
            fprintf(stderr, "Couldn't register rank = %d errno = %d size = %d\n",rank,errno,schd[i].size);
            goto clean_pd;
        }
    }

    //创建CQ，CQ一次创建可以在多个QP之间共享
	ctx->cq = ibv_create_cq(ctx->context,rx_depth+1, NULL,
				ctx->channel, 0);
	if (!ctx->cq) {
		fprintf(stderr, "Couldn't create CQ rank = %d errno = %d\n",rank,errno);
    }
    // //创建SRQ
    // {
    //     /*
    //     是一个共享接收队列。与使用队列对（每个队列对都有自己的接收队列）相比，使用SRQ减少了已发布的接收请求总数。
    //     use one SRQ with different QPs
    //      use one SRQ with QPs with different transport types
    //     */
    //     struct ibv_srq_init_attr attr = {
    //         .attr = {
    //             .max_wr = msgn+10,
    //             .max_sge = 1}};

    //     ctx->srq = ibv_create_srq(ctx->pd, &attr);
    //     if (!ctx->srq)
    //     {
    //         fprintf(stderr, "Couldn't create SRQ  rank = %d errno = %d\n",rank,errno);
    //     }
    // }
    //开始初始化QP
    ctx->qpNum = nprocs;
    for(i = 0;i<nprocs;i++)
    {
		struct ibv_qp_attr attr;
		struct ibv_qp_init_attr init_attr;
        memset(&init_attr, 0, sizeof(init_attr));
        init_attr.send_cq = ctx->cq;
        init_attr.recv_cq = ctx->cq;
        //init_attr.cap.max_send_wr = ctx->dev_cap.max_qp_wr;
        init_attr.cap.max_send_wr = ctx->dev_cap.max_qp_wr;
        init_attr.cap.max_recv_wr = ctx->dev_cap.max_qp_wr;
        init_attr.cap.max_send_sge = 1;
        init_attr.cap.max_recv_sge = 1;
        init_attr.qp_type = IBV_QPT_RC;
        init_attr.sq_sig_all = 1;
        //在每一个节点对间创建一对qp
		ctx->qp[i] = ibv_create_qp(ctx->pd, &init_attr);
		if (!ctx->qp[i])  {
			fprintf(stderr, "Couldn't create QP[%d]\n", i);
            puts("error creating qp");
        }
        //printf("max_inline_data = %d\n",init_attr.cap.max_inline_data);
    }
    //Transition a QP from the RESET to INIT state
    for (i = 0; i < nprocs; ++i)
    {
        struct ibv_qp_attr attr;
        memset(&attr, 0, sizeof(struct ibv_qp_attr));
        attr.qp_state = IBV_QPS_INIT;
        attr.pkey_index = 0;
        attr.port_num = config.ib_port;
        attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
        if (ibv_modify_qp(ctx->qp[i], &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS))
        {
            fprintf(stderr, "Failed to modify QP[%d] to INIT\n", i);
        }
    }
    return ctx;
clean_comp_channel:
    if (ctx->channel)
        ibv_destroy_comp_channel(ctx->channel);
clean_device:
    ibv_close_device(ctx->context);
clean_pd:
	ibv_dealloc_pd(ctx->pd);
    return NULL;
}
static int modify_qp_to_rtr(struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid, uint8_t *dgid)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;
	memset(&attr, 0, sizeof(attr));
	attr.qp_state = IBV_QPS_RTR;
	attr.path_mtu = IBV_MTU_4096;
	attr.dest_qp_num = remote_qpn;
	attr.rq_psn = 11;
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
static int modify_qp_to_rts(struct ibv_qp *qp)
{
	struct ibv_qp_attr attr;
	int flags;
	int rc;
	memset(&attr, 0, sizeof(attr));
	attr.qp_state = IBV_QPS_RTS;
	attr.timeout = 14;
	attr.retry_cnt = 7;
	attr.rnr_retry = 7;
	attr.sq_psn = 11;
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

int iph_connect_qp(struct IPH_resources * res, int rank,int procn,MPI_Comm IPH_COMM_WORLD)
{
    //printf("rank = %d\n",rank);
	int i = 0;
    int rc;
    MPI_Request * send_request = (MPI_Request *)malloc(sizeof(MPI_Request)*msgn);
    MPI_Request * recv_request = (MPI_Request *)malloc(sizeof(MPI_Request)*msgn);
    MPI_Status  * status       = (MPI_Status  *)malloc(sizeof(MPI_Status)*msgn);
    int request_index = 0;
    int * valid_qp = (int * )malloc(sizeof(int)*procn);
    memset(valid_qp, 0, sizeof(int) * procn);

    //端口lid查询
    if (ibv_query_port(res->context, config.ib_port, &res->portinfo) != 0)
    {
        fprintf(stderr, "Error, failed to query port  attributes in device \n");
        printf("errno = %d\n", errno);
    }
    //端口gid查询
    union ibv_gid my_gid;
    if (config.gid_idx >= 0)
    {
        rc = ibv_query_gid(res->context, config.ib_port, config.gid_idx, &my_gid);
        if (rc)
        {
            fprintf(stderr, "could not get gid for port %d, index %d\n", config.ib_port, config.gid_idx);
            return rc;
        }
        if (config.gid_idx >= 0)
        {
            uint8_t *p = (uint8_t *)&my_gid;
            // fprintf(stdout, "Local GID =%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
            //         p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
        }
    }
	else
		memset(&my_gid, 0, sizeof my_gid);
    for(i = 0;i<msgn;i++)
    {
        
        int commT = 0;
        if(rank == schd[i].source)
            commT = schd[i].target;
        else
            commT = schd[i].source;
        //对每一个消息都将需要交换其信息。
        if(rank == schd[i].source){
            schd[i].local_connect_data_t.addr = (uintptr_t)schd[i].bufS;
            schd[i].local_connect_data_t.qp_num = res->qp[schd[i].target]->qp_num;  
        }
        else
        {
            schd[i].local_connect_data_t.addr = (uintptr_t)schd[i].bufR;
            schd[i].local_connect_data_t.qp_num = res->qp[schd[i].source]->qp_num;  
        }
        //printf("%p %p rank = %d msgn = %d\n",schd[i].bufS,schd[i].bufR,rank,msgn);

        schd[i].local_connect_data_t.rkey = schd[i].mr->rkey;
        schd[i].local_connect_data_t.lid = res->portinfo.lid;
        memcpy(schd[i].local_connect_data_t.gid, &my_gid, 16);
        // {
        //     uint8_t *p = (uint8_t *)&my_gid;
        //     printf("local rank = %d,qp_num[%d]= %d,buffer = %p,rkey = %d,lid = %d GID =%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",rank,commT,
        //                                                                             schd[i].local_connect_data_t.qp_num,
        //                                                                             schd[i].local_connect_data_t.addr,
        //                                                                             schd[i].local_connect_data_t.rkey,
        //                                                                             schd[i].local_connect_data_t.lid,
        //                                                                             p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
        // }
        //将端口号初始化到交换数据结构中去。
        //使用MPI 将端口号传输到远端消息的数据结构中去
        MPI_Isend(&schd[i].local_connect_data_t,sizeof(struct cm_con_data_t),MPI_CHAR,commT,0,IPH_COMM_WORLD,&send_request[request_index]);
        MPI_Irecv(&schd[i].remote_connect_data_t,sizeof(struct cm_con_data_t),MPI_CHAR,commT,0,IPH_COMM_WORLD,&recv_request[request_index]);
        //printf("(%d)->(%d) + (%d)<-(%d)\n",rank,commT,rank,commT);

        request_index++;
    }
    //printf("start wait rank = %d request_index = %d\n",rank,request_index);
    MPI_Waitall(request_index,send_request,status);
    MPI_Waitall(request_index,recv_request,status);
    free(send_request);
    free(recv_request);
    free(status);
    MPI_Barrier(IPH_COMM_WORLD);

    //对每一个QP，设置QP的状态
    /* modify the QP to RTR */
    for(i = 0;i<msgn;i++)
    {
        int commT = 0;
        if(rank == schd[i].source)
            commT = schd[i].target;
        else
            commT = schd[i].source;

        //需要跳过已经设置好了状态的QP
        
        if(valid_qp[commT])
            continue;
        else
            valid_qp[commT] = 1;
        rc = modify_qp_to_rtr(res->qp[commT],
                                schd[i].remote_connect_data_t.qp_num,
                                schd[i].remote_connect_data_t.lid,
                                schd[i].remote_connect_data_t.gid);
        // {
        //     uint8_t *p = (uint8_t *)&schd[i].remote_connect_data_t.gid;
        //     printf("rank %d qp[%d] rtr qp_num=%d,lid=%d,GID =%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",rank,commT,
        //                                                         schd[i].remote_connect_data_t.qp_num,
        //                                                         schd[i].remote_connect_data_t.lid,
        //                                                         p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
        // }
        if (rc)
        { 
            fprintf(stderr, "failed to modify QP state to RTR\n");
            printf("errno = %d\n", errno);
        }

        rc = modify_qp_to_rts(res->qp[commT]);
        if (rc)
        {
            fprintf(stderr, "rank = %d failed to modify QP state to RTS\n", rank);
            printf("errno = %d\n", errno);
        }
    }
    return 0;
}
void iph_prepare_rdma_write_requests(struct IPH_resources * res, int rank,struct  ibv_send_wr ** WRPs,struct ibv_sge ** SgePs,struct  ibv_send_wr ** BAD_WRPs)
{

    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    (*WRPs) = (struct  ibv_send_wr *) malloc(sizeof(struct ibv_send_wr)*msgn);
    (*BAD_WRPs) = (struct  ibv_send_wr *) malloc(sizeof(struct ibv_send_wr)*msgn);
    (*SgePs) = (struct ibv_sge *) malloc(sizeof(struct ibv_sge) * msgn);
    struct  ibv_send_wr * WRs = (*WRPs);
    struct  ibv_sge     * Sges = (*SgePs);
    struct  ibv_send_wr * Bad_wrs = (*BAD_WRPs);
    int i = 0;
    for(i = 0;i<msgn;i++)
    {
        schd[i].time_end = 0.0;
        schd[i].time_start = 0.0;
        memset(&Sges[i], 0, sizeof(struct ibv_sge));
        memset(&WRs[i], 0 ,sizeof(struct ibv_send_wr));
        if(rank == schd[i].source)
        {
            Sges[i].addr = (uintptr_t)schd[i].bufS;
            Sges[i].length = schd[i].size;
            Sges[i].lkey    = schd[i].mr->lkey;
    
     	    /* prepare the send work request */
            WRs[i].next = NULL;
            WRs[i].wr_id = i;
            WRs[i].sg_list = &Sges[i];
            WRs[i].num_sge = 1;
            WRs[i].opcode = IBV_WR_RDMA_WRITE;
            ibv_query_qp(res->qp[schd[i].target], &attr, IBV_QP_CAP, &init_attr);
            //puts("check");
            if(schd[i].round == 1){
                //puts("check");
                WRs[i].send_flags = IBV_SEND_FENCE | IBV_SEND_INLINE;
            }
            else
                WRs[i].send_flags = IBV_SEND_SIGNALED| IBV_SEND_INLINE ;
		    // if (init_attr.cap.max_inline_data >= schd[i].size){
			//     WRs[i].send_flags = IBV_SEND_INLINE;
            // }
            //printf("init_attr.cap.max_inline_data = %d\n",init_attr.cap.max_inline_data);
            WRs[i].wr.rdma.remote_addr  = schd[i].remote_connect_data_t.addr;
            WRs[i].wr.rdma.rkey         = schd[i].remote_connect_data_t.rkey;
        }
    }
}
void iph_prepare_rdma_read_requests(struct IPH_resources * res, int rank,struct  ibv_send_wr ** WRPs,struct ibv_sge ** SgePs,struct  ibv_send_wr ** BAD_WRPs)
{
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;
    (*WRPs) = (struct  ibv_send_wr *) malloc(sizeof(struct ibv_send_wr)*msgn);
    (*BAD_WRPs) = (struct  ibv_send_wr *) malloc(sizeof(struct ibv_send_wr)*msgn);
    (*SgePs) = (struct ibv_sge *) malloc(sizeof(struct ibv_sge) * msgn);
    struct  ibv_send_wr * WRs = (*WRPs);
    struct  ibv_sge     * Sges = (*SgePs);
    struct  ibv_send_wr * Bad_wrs = (*BAD_WRPs);
    int i = 0;
    for(i = 0;i<msgn;i++)
    {
        schd[i].time_end = 0.0;
        schd[i].time_start = 0.0;
        memset(&Sges[i], 0, sizeof(struct  ibv_sge));
        memset(&WRs[i], 0 ,sizeof(struct  ibv_send_wr));
        if(rank == schd[i].target)
        {
            Sges[i].addr = (uintptr_t)schd[i].bufR;
            Sges[i].length = schd[i].size;
            Sges[i].lkey    = schd[i].mr->lkey;
    
     	    /* prepare the send work request */
            WRs[i].next = NULL;
            WRs[i].wr_id = i;
            WRs[i].sg_list = &Sges[i];
            WRs[i].num_sge = 1;
            WRs[i].opcode = IBV_WR_RDMA_READ;
            ibv_query_qp(res->qp[schd[i].source], &attr, IBV_QP_CAP, &init_attr);
            if(schd[i].round == 1)
                WRs[i].send_flags = IBV_SEND_FENCE | IBV_SEND_INLINE;
            else
                WRs[i].send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
            WRs[i].wr.rdma.remote_addr  = schd[i].remote_connect_data_t.addr;
            WRs[i].wr.rdma.rkey         = schd[i].remote_connect_data_t.rkey;
        }
    }
}

inline int calc(struct IPH_resources * res,int delay,double startT, struct ibv_wc * wc)
{
	//puts("checkp");
	// //如果延迟太低，则不做任何处理
    // if(delay < 10)
    //     return 0;
    //usleep(delay);
    //return 0;
    int received_msgN = 0;
	double  start = get_time(startT);
	double  end	= get_time(startT);
    int poll_result = 0;
    do
    {
		poll_result = ibv_poll_cq(res->cq, 1, wc);
        if(poll_result)
        {
            poll_result = 0;
            received_msgN++;
            //puts("check finished");
            //printf("%d writed size %d WRID = %d\n",rank, wc.byte_len, wc.wr_id);
            
            if (wc->status != IBV_WC_SUCCESS)
            {
                if(wc->status == IBV_WC_WR_FLUSH_ERR)
                {
                    printf("IBV_WC_WR_FLUSH_ERR ");
                }else if(wc->status == IBV_WC_RETRY_EXC_ERR)
                {
                 printf("BV_WC_RETRY_EXC_ERR ");   
                }
                printf("got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc->status,
                        wc->vendor_err);
            }
            schd[wc->wr_id].time_end += 1000000.0*(MPI_Wtime() - startT);
            //puts("check finished");
        }
		end = get_time(startT);
    } while (end - start< delay);
	return received_msgN;
}

inline double iph_schedule_request(struct IPH_resources * res,int rank,struct  ibv_send_wr  * WRs,double delay_factor,MPI_Comm IPH_COMM_WORLD,double startT)
{
    int i = 0;
    int re = 0;
    int requestN = 0;
    struct ibv_send_wr ** Bad_wrs = (struct ibv_send_wr **)malloc(sizeof(struct ibv_send_wr* )*msgn);
 	struct ibv_send_wr *bad_wr = NULL;
    memset(Bad_wrs,0,sizeof(struct ibv_send_wr *)*msgn);
    int finished_pstn = 0,posted_pstn = 0;
    struct ibv_wc wc;
    //开始post 消息
    long long iphtime = 0;
    for(i = 0;i<msgn;i++)
    {
#ifdef USING_RDMA_WRITE
        if(rank == schd[i].source)
        {

            requestN++;
            // if(iphtime != 0)
            //     finished_pstn +=calc(res,delay_factor*(schd[i].delay - iphtime),startT,&wc);
            // iphtime = schd[i].size;
            // // printf("delay = %f delay_factor = %f schd[i].delay=%d  iphtime = %d \n",
            // //     delay_factor*(schd[i].delay - iphtime),delay_factor,schd[i].delay,iphtime);
            do{
                schd[i].time_start += 1000000.0*(MPI_Wtime() - startT);
                re = ibv_post_send(res->qp[schd[i].target], &WRs[i],&Bad_wrs[i]);
                //printf("rank %d post WR[%d] from qp[%d] to  remote_addr = %p, rkey = %d\n",rank,i,schd[i].target,WRs[i].wr.rdma.remote_addr,
                //                                                                                                WRs[i].wr.rdma.rkey);
                if (re != IBV_WC_SUCCESS){
                    printf ("failed to post SR <%d,%d> rank = %d errno = %d\n",schd[i].source,schd[i].target,rank,errno);
                    continue;
                }
                posted_pstn  += 1;
               // finished_pstn +=calc(res,0,startT,&wc);
            }while (re != IBV_WC_SUCCESS);
            //finished_pstn +=calc(res,0,startT,&wc);
            //puts("check");
        }
#elif USING_RDMA_READ
        if(rank == schd[i].target)
        {
            requestN++;
            do{
                re = ibv_post_send(res->qp[schd[i].source], &WRs[i],&Bad_wrs[i]);
                schd[i].time_start += 1000000.0*(MPI_Wtime() - startT);
                if (re != IBV_WC_SUCCESS){
                    printf("failed to post SR <%d,%d> rank = %d errno = %d\n",schd[i].source,schd[i].target,rank,errno);
                }
                // printf("rank %d post WR[%d] from qp[%d] to  remote_addr = %p, rkey = %d\n",rank,i,schd[i].target,WRs[i].wr.rdma.remote_addr,
                //                                                                                                WRs[i].wr.rdma.rkey);
                finished_pstn +=calc(res,0,startT,&wc);
            }while (re!= IBV_WC_SUCCESS); 
            //puts("check");
        }
#endif
    }
    //开始接收
    int poll_result = 0;
    while(finished_pstn < requestN)
    {
    //printf("rank = %d, finished_pstn = %d,requestN = %d \n",rank,finished_pstn,requestN);
		poll_result = ibv_poll_cq(res->cq, 1, &wc);
        if(poll_result > 0)
        {
            poll_result = 0;
            finished_pstn++;
            //printf("%d writed size %d WRID = %d\n",rank, wc.byte_len, wc.wr_id);
            // 		/* check the completion status (here we don't care about the completion opcode */
            if(wc.status != IBV_WC_SUCCESS)
            {
                if(wc.status == IBV_WC_WR_FLUSH_ERR)
                {
                    printf("This event is generated when an invalid remote error is thrown when the responder detects an invalid request.\n It may be that the operation is not supported by the request queue or there is insufficient buffer space to receive the request.\n");
                }else if(wc.status == IBV_WC_RETRY_EXC_ERR)
                {
                 printf("BV_WC_RETRY_EXC_ERR ");   
                }
                printf("got bad completion with status: 0x%x, vendor syndrome: 0x%x rank = %d\n", wc.status,
                        wc.vendor_err,rank);
            }
            schd[wc.wr_id].time_end += 1000000.0*(MPI_Wtime() - startT);
            //printf("rank =%d check finished \n",rank);
        }
      //  usleep(10);
    }
    //printf("return %d\n",rank);
    double t =  MPI_Wtime() - startT;
    return t;
}


bool Iph_finish_cmp(const IphComm & a,const IphComm &b)
{
   return  a.time_end < b.time_end;
}
static int iph_resources_destroy(struct IPH_resources *res, int rank)
{
	int rc = 0;
    int i = 0;
    //销毁qp资源
	if (res->qpNum) {
        for(i = 0;i<res->qpNum;i++)
        {
            if (ibv_destroy_qp(res->qp[i])) {
                fprintf(stderr, "failed to destroy QP\n");
                rc = 1;
                return rc;
            }
        }
	}
    //销毁内存管理资源
    for (i = 0;i<msgn;i++)
    {
        if (schd[i].mr) {
            if (ibv_dereg_mr(schd[i].mr)) {
                fprintf(stderr, "failed to deregister MR\n");
                rc = 1;
                return rc;
            }
        }
        if(rank == schd[i].source)
        {
            free(schd[i].bufS);
        }else{
            free(schd[i].bufR);
        }
    }

	if (res->cq) {
		if (ibv_destroy_cq(res->cq)) {
			fprintf(stderr, "failed to destroy CQ\n");
			rc = 1;
            return rc;
		}
	}
	if (res->pd) {
		if (ibv_dealloc_pd(res->pd)) {
			fprintf(stderr, "failed to deallocate PD\n");
			rc = 1;
            return rc;
		}
	}

	if (res->context) {
		if (ibv_close_device(res->context)) {
			fprintf(stderr, "failed to close device context\n");
			rc = 1;
            return rc;
		}
	}

	return rc;
}
int iph_main(char * argv[],int rank,int nprocs, MPI_Comm IPH_COMM_WORLD)
{
   // puts("check");
    struct ibv_device **dev_list; //设备表列
    struct ibv_device *ib_dev;    //设备指针
    struct ibv_wc *wc;
    int num_devices;     //设备数量
    uint64_t my_guid;    //本设备guid
    struct IPH_resources *ctx;  //自定义资源结构体。
    int rx_depth = 1000; //Requested maximum number of outstanding WRs in the SRQ
    int ib_port = config.ib_port;
    int use_event = 0; //whether arms the notification mechanism for the indicated completion queue (CQ).
                       /*
The notification mechanism will only be armed for one notification.
Once a notification is sent, the mechanism must be re-armed with a new call to ibv_req_notify_cq. 
*/
    page_size = sysconf(_SC_PAGESIZE);
    int i = 0;
    sscanf(argv[2],"%d",&i);
    double delay_factor = (double)(i*4.0/100.0)*(1.0/12500.0);
    //获取设备表列
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list)
    {
        perror("Failed to get IB devices list");
        return 1;
    }
    //输出设备名
    //printf("num_devices = %d\n", num_devices);
    // for ( i = 0; i < num_devices; ++i)
    // {
    //     printf("    %-16s\t%016llx\n",
    //            ibv_get_device_name(dev_list[i]),
    //            (unsigned long long)be64toh(ibv_get_device_guid(dev_list[i])));
    //     printf("RDMA device[%d]:name+%s\n", i, ibv_get_device_name(dev_list[i]));
    // }
    // for(i = 0;i<nprocs;i++)
	// {
	// 	if(rank == i)
	// 		printf("%d\n0x%016llx\n",rank,(unsigned long long)be64toh(ibv_get_device_guid(dev_list[0])));
	// 	MPI_Barrier(IPH_COMM_WORLD);
	// }
    //获取当前默认第一个IB网络设备
    ib_dev = *dev_list;
    my_guid = ibv_get_device_guid(ib_dev);
    if (!ib_dev)
    {
        fprintf(stderr, "No IB devices found\n");
        return 1;
    }
    //初始化当前资源结构（设备，
    ctx = iph_init_ctx(ib_dev, rx_depth, ib_port, use_event,rank,nprocs);
	if (!ctx){
		return 1;
    }
    if(rank == 0)
    {
        //打印出设备的ctx->dev_cap.max_qp_wr;
        //printf("ctx->dev_cap.max_qp_wr = %d\n",ctx->dev_cap.max_qp_wr);
    }
	if (iph_connect_qp(ctx, rank,nprocs,IPH_COMM_WORLD))
	{ 
		fprintf(stderr, "failed to connect QPs\n");
	}
    struct  ibv_send_wr  * WRs;
    struct ibv_sge       * Sges;
    struct  ibv_send_wr * Bad_wrs;
#ifdef USING_RDMA_WRITE
    iph_prepare_rdma_write_requests(ctx,rank,&WRs,&Sges,&Bad_wrs);
#elif USING_RDMA_READ
    iph_prepare_rdma_read_requests(ctx,rank,&WRs,&Sges,&Bad_wrs);
#endif
    double time = 0.0;
    double time1 = 0.0;
    int loopN = 100;
    double startT;
    MPI_Barrier(IPH_COMM_WORLD); 
    //warm up 热身运动
    for(i = 0;i<loopN;i++)
    {
        //printf("loop %d\n",i);
        startT = MPI_Wtime();
        iph_schedule_request(ctx,rank,WRs,delay_factor,IPH_COMM_WORLD,startT);
        MPI_Barrier(IPH_COMM_WORLD); 
    }
    for(i = 0;i<msgn;i++)
    {
        schd[i].time_end = 0.0;
        schd[i].time_start = 0.0;
    }
    MPI_Barrier(IPH_COMM_WORLD); 
    MPI_Barrier(IPH_COMM_WORLD); 
    MPI_Barrier(IPH_COMM_WORLD); 
    double timeS = MPI_Wtime();
    double timeS1 = get_time(0.0);
    for(i = 0;i<loopN;i++)
    {
        //printf("loop %d\n",i);
        startT = MPI_Wtime();
        iph_schedule_request(ctx,rank,WRs,delay_factor,IPH_COMM_WORLD,startT);
        MPI_Barrier(IPH_COMM_WORLD); 
    }
    double timeE = MPI_Wtime();
    double timeE1 = get_time(0.0);
    //printf("finish loop %d \n",rank);

    if(iph_resources_destroy(ctx,rank))
    {
        puts("---------------------------------------------------------");
        printf("error destroy resources \n");
        puts("---------------------------------------------------------");
    }
    double barStartT = MPI_Wtime();
    for(i = 0;i<loopN;i++)
    {
        MPI_Barrier(IPH_COMM_WORLD); 
    }
    double barEndT = MPI_Wtime();
    //printf("finish barrier %d \n",rank);

    sort(schd,schd+msgn,Iph_finish_cmp);
    for (i = 0;i<msgn;i++)
    {
        // if(rank == schd[i].source)
        //    printf("(%d\t%d\t%d)\t started:%f finished: %f \n",schd[i].source,schd[i].target,schd[i].target,
        //         schd[i].time_start/loopN, schd[i].time_end / loopN);
    }
    //puts("check");
    time = 1000000.0*(timeE - timeS - barEndT + barStartT)/loopN;
    time1 = (timeE1 - timeS1- barEndT + barStartT)/loopN;
    double re = 0.0,re1 = 0.0;
	MPI_Reduce(&time, &re, 1,MPI_DOUBLE,
               MPI_MAX, 0, IPH_COMM_WORLD);
	MPI_Reduce(&time1, &re1, 1,MPI_DOUBLE,
               MPI_MAX, 0, IPH_COMM_WORLD);
    //printf("time = %f\n",time);
	if (rank == 0){
		printf("%f\n", re);
    }
    MPI_Barrier(IPH_COMM_WORLD);
    //puts("finish barrier");
}