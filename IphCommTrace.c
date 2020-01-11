#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <malloc.h>
#include <string.h>
#include "IphCommTrace.h"

void iph_comm_copy(IphComm * sour,IphComm * tar)
{
    if(sour == tar)
        return ;
    tar->source = sour->source;
    tar->target = sour->target;
    tar->size   = sour->size;
    tar->round  = sour->round;
    tar->bufS   = sour->bufS;
    tar->bufS   = sour->bufR;
	tar->delay	= sour->delay;
}

void Iph_swap(IphComm *a,IphComm *b)
{
    IphComm c;
    iph_comm_copy(a,&c);
    iph_comm_copy(b,a);
    iph_comm_copy(&c,b);
}

//此函数将消息分派给各个子进程
void dispatch_all_message(IphComm ** SubComms,int * Num,int Mainrank,int rank,MPI_Comm comm)
{
    MPI_Bcast((void *)Num,1,MPI_INT,Mainrank,comm);

    if((*SubComms) == 0){
        (*SubComms) = (IphComm *)malloc((*Num)*sizeof(IphComm));
    }else{
        if(rank != Mainrank)
        {
            free(*SubComms);
            (*SubComms) = (IphComm *)malloc((*Num)*sizeof(IphComm));
        }
    }
    MPI_Bcast((void *)(*SubComms),(*Num)*sizeof(IphComm),MPI_CHAR,
                Mainrank,comm);
    int i = 0;
    int count = 0;
    //printf("%d Num = %d\n",rank,*Num);
    while(i < *Num)
    {
       // printf("rank = %d,target = %d i = %d,Num = %d\n",rank,(*SubComms)[i].target,i,*Num);
        if(rank == (*SubComms)[i].source || rank == (*SubComms)[i].target){
            iph_comm_copy(&(*SubComms)[i],&(*SubComms)[count]);
            count++;
        }
        i++;
    }
    *Num = count;
}

void dispatch_all_message_rdma_put(IphComm ** SubComms,int * Num,int Mainrank,int rank)
{
    MPI_Bcast((void *)Num,1,MPI_INT,Mainrank,MPI_COMM_WORLD);

    if((*SubComms) == 0){
        (*SubComms) = (IphComm *)malloc((*Num)*sizeof(IphComm));
    }else{
        if(rank != Mainrank)
        {
            free(*SubComms);
            (*SubComms) = (IphComm *)malloc((*Num)*sizeof(IphComm));

        }
    }
    // MPI_Scatter((void *)AllComms,(*Num)*sizeof(IphComm),MPI_CHAR,
    //             (void *)(*SubComms),(*Num)*sizeof(IphComm),MPI_CHAR,
    //             Mainrank,MPI_COMM_WORLD);
    MPI_Bcast((void *)(*SubComms),(*Num)*sizeof(IphComm),MPI_CHAR,
                Mainrank,MPI_COMM_WORLD);
    int i = 0;
    int count = 0;
    //printf("%d Num = %d\n",rank,*Num);
    while(i < *Num)
    {
       // printf("rank = %d,target = %d i = %d,Num = %d\n",rank,(*SubComms)[i].target,i,*Num);
        if(rank == (*SubComms)[i].source){
            iph_comm_copy(&(*SubComms)[i],&(*SubComms)[count]);
            count++;
        }
        i++;
    }
    *Num = count;

}


void print_message_vec(int rank,IphComm * Comms,int size)
{
    if(Comms == 0 && size != 0){
        puts("error print msg");
    }
    printf("msgNum = %d \n",size);
    int i = 0;
    for(i = 0;i<size;i++)
    {
        printf("(myid = %d,daly = %d, round = %d,source %d,target %d) size= %d\n",rank,Comms[i].delay,Comms[i].round,Comms[i].source,Comms[i].target,Comms[i].size);
    }
}


void memory_init(IphComm * Comms,int MsgNum,int my_rank)
{
    int i = 0;
    for(i = 0;i<MsgNum;i++)
    {
        if(my_rank == Comms[i].source)
        {
            Comms[i].bufS = (char *)memalign(sysconf(_SC_PAGESIZE),sizeof(char)*Comms[i].size);
            memset(Comms[i].bufS,'a',Comms[i].size);
        }
        if(my_rank == Comms[i].target)
        {
             Comms[i].bufR =(char *)memalign(sysconf(_SC_PAGESIZE),sizeof(char)*Comms[i].size); 
            memset(Comms[i].bufR,'b',Comms[i].size);
        }
    }
}
void memory_destroy(IphComm * Comms,int MsgNum,int my_rank)
{
    int i = 0;
    for(i = 0;i<MsgNum;i++)
    {
        if(Comms[i].bufS  != 0)
        {
            free(Comms[i].bufS);
        }
        if(Comms[i].bufR != 0)
        {
            free(Comms[i].bufR);
        }
    }
    free(Comms);
}
double Run_comm_pattern(IphComm * Comms,int MsgNum,int rank)
{
    MPI_Request *req_send = (MPI_Request *)malloc(sizeof(MPI_Request)*MsgNum);
    MPI_Status  *status_send=(MPI_Status  *)malloc(sizeof(MPI_Status )*MsgNum);
    int ReqSI = 0;
    MPI_Request *req_recv = (MPI_Request *)malloc(sizeof(MPI_Request)*MsgNum);
    MPI_Status  *status_recv=(MPI_Status  *)malloc(sizeof(MPI_Status )*MsgNum);
    int ReqRI = 0;

    int loopN = 400;
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    double time_start = MPI_Wtime();
    for(loopN = 0;loopN<1;loopN++)
    {
        ReqSI = 0;
        ReqRI = 0;
        int i = 0;
        //i = 0;
        for(i=0;i<MsgNum;i++)
        {
            if(rank == Comms[i].source){
            // / MPI_Isend()
            MPI_Isend(Comms[i].bufS,Comms[i].size,MPI_CHAR,
                        Comms[i].target,0,MPI_COMM_WORLD,&req_send[ReqSI++]);
            }
            if(rank == Comms[i].target){
                MPI_Irecv(Comms[i].bufR,Comms[i].size,MPI_CHAR,
                        Comms[i].source,0,MPI_COMM_WORLD,&req_recv[ReqRI++]);
            }
        }
        MPI_Waitall(ReqSI,req_send,status_send);
        MPI_Waitall(ReqRI,req_recv,status_recv);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    double time_end = MPI_Wtime();

    free(req_send);
    free(status_send);
    free(status_recv);
    free(req_recv);
    return (time_end - time_start)/loopN;
}

