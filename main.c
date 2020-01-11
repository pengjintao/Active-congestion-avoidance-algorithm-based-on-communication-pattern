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
#include "proc.h"
#include "IphCommTrace.h"
#include "Iph_Ib_comm.h"
#include "Iph_psm2_comm.h"
#include "rc-pingpong.h"

//Nax9Fr72cray
int main(int argc, char *argv[])
{
	/**** 初始化MPI环境 ****/
	int num_procs, my_id; //进程总数、进程ID
	MPI_Status status;	//status对象(Status)
	MPI_Request handle;   //MPI请求(request)
	int ierr;			  //MPI返回值
	double inittime, recvtime, totaltime;
	int i = 0, j = 0;
	double factor = 0.0;
	int proc_per_node = 1;
	int local_id = 0;
	MPI_Comm IPH_comm,comm2;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
	//printf("num_procs = %d\n",num_procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_id);

	/* 从程序运行参数中获得二维进程矩阵边长和buffer的大小 */

	//通信组的分割
	
	if(proc_per_node > 1)
	{
		MPI_Group worldGroup;						 //创建一个进程组
		MPI_Comm_group(MPI_COMM_WORLD, &worldGroup); //将该进程组与MPI_COMM_WORLD通信域进行绑定
		MPI_Group group, group2;
		int *vec = (int *)malloc(sizeof(int) * (num_procs / proc_per_node));
		int *vec2 = (int *)malloc(sizeof(int) * (num_procs - (int)num_procs / proc_per_node));
		for (i = 0; i < (num_procs / proc_per_node); i++)
		{
			vec[i] = proc_per_node * i;
			for (j = 1; j < proc_per_node; j++)
			{
				vec2[(proc_per_node - 1) * i + j - 1] = vec[i] + j;
			}
		}
		// if (my_id == 0)
		// {
		// 	for (i = 0; i < (num_procs / proc_per_node); i++)
		// 	{
		// 		printf("%d ", vec[i]);
		// 	}
		// 	puts("");
		// 	for (i = 0; i < (num_procs - (int)num_procs / proc_per_node); i++)
		// 	{
		// 		printf("%d ", vec2[i]);
		// 	}
		// }
		MPI_Group_incl(worldGroup,(num_procs - (int)num_procs / proc_per_node),&vec2[0],&group2);//设置进程组group2中的进程为group2P
		MPI_Group_incl(worldGroup,(num_procs / proc_per_node),&vec[0],&group);//设置进程组group1中的进程为group1P

		MPI_Comm_create(MPI_COMM_WORLD,group,&IPH_comm);//将进程组group1与通信域comm1进行绑定
		MPI_Comm_create(MPI_COMM_WORLD,group2,&comm2);//将进程组group2与通信域comm2进行绑定

		MPI_Group_translate_ranks(worldGroup,1,&my_id,group,&local_id);
	}else{
		local_id = my_id;
		IPH_comm = MPI_COMM_WORLD;
	}
	if(local_id != MPI_UNDEFINED)
	{
		int nprocs;
		MPI_Comm_size(IPH_comm,&nprocs);
		#define RUN_FILE 1
		//#define RUN_PINGPONG 1
		//printf("%d\n",local_id);
		//读取通信模式文件
#ifdef RUN_FILE
		{
			char *file_name = argv[1];
			factor = atoi(argv[2]) / 10000000.0;
			read_file(local_id, file_name, MPI_COMM_WORLD);
			/*首先为消息分配内存*/
			memory_init(schd, msgn, my_id);
			//printf("%d\n",local_id);
			//iph_main(argv,local_id,nprocs,MPI_COMM_WORLD);
			//int pingpong_size = 1<<24;
			//ping_pong(my_id,pingpong_size);
			//ibv_pingpong_main(local_id,nprocs,IPH_comm);
			iph_psm2_main(argv,local_id,nprocs,IPH_comm);
		}
#endif
#ifdef RUN_PINGPONG
		{
			int pingpong_size = 1<<atoi(argv[1]);
			ping_pong(my_id,pingpong_size);
		}
#endif
	}
	//puts("total barrier");
	MPI_Barrier(MPI_COMM_WORLD);
	// ///////////////////////////////////
	MPI_Finalize();
	return 0;
}
