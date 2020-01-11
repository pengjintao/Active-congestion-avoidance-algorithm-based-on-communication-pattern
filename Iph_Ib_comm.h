#ifndef Iph_Ib_comm
#define Iph_Ib_comm
#include <mpi.h>
#include <infiniband/verbs.h>
#include <stdlib.h>
#include <stdint.h>
#include <rdma/rdma_cma.h>

int iph_main(char * argv[],int rank,int nprocs, MPI_Comm IPH_COMM_WORLD);

int pp_get_port_info(struct ibv_context *context, int port,
		     struct ibv_port_attr *attr);
void wire_gid_to_gid(const char *wgid, union ibv_gid *gid);
void gid_to_wire_gid(const union ibv_gid *gid, char wgid[]);

enum {
	PINGPONG_RECV_WRID = 1,
	PINGPONG_SEND_WRID = 2,
	MAX_QP             = 256,
};

struct config_t
{
	const char *dev_name; /* IB device name */
	char *server_name;	/* server host name */
	int ib_port;		  /* local IB port to work with */
	int gid_idx;		  /* gid index to use */
};

#endif