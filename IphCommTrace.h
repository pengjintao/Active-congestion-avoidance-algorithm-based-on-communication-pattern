#ifndef IPHCOMMTRACE_H
#define IPHCOMMTRACE_H

#include <mpi.h>
#include <infiniband/verbs.h>
#include <psm2.h> /* required for core PSM2 functions */
#include <psm2_mq.h>

struct cm_con_data_t
{
    uint64_t addr;   /* Buffer address */
    uint32_t rkey;   /* Remote key */
    uint32_t qp_num; /* QP number */
    uint16_t lid;    /* LID of the IB port */
    uint8_t gid[16]; /* gid */
} __attribute__((packed));

typedef struct COMMUNICATIONS{
    int source;
    int target;
    //size > 0 means send,size < 0 means recv
    int   size;
    int   round;
    char * bufS;
    char * bufR;
	long long	  delay;	
	double time;
    double time_start;
    double time_end;
	struct ibv_mr *mr;
    struct cm_con_data_t local_connect_data_t;
    struct cm_con_data_t remote_connect_data_t;
    //IB平台数据结构    
    //intel PSM2 数据结构
    psm2_mq_req_t req_mq;
}IphComm;

void iph_comm_copy(IphComm  * sour,IphComm * tar);
void dispatch_all_message(IphComm ** SubComms,int * Num,int Mainrank,int rank,MPI_Comm comm);

void print_message_vec(int rank,IphComm * Comms,int size);
void dispatch_all_message_rdma_put(IphComm ** SubComms,int * Num,int Mainrank,int rank);

double Run_comm_pattern(IphComm * Comms,int MsgNum,int rank);
void memory_init(IphComm * Comms,int MsgNum,int my_rank);

void memory_destroy(IphComm * Comms,int MsgNum,int my_rank);
#endif
