#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/time.h>
#include <unistd.h>
#include "proc.h"
#include "IphCommTrace.h"


IphComm *schd;
int msgn = 0;
static struct timeval end; 

void read_file(int rank,char *fn,MPI_Comm comm)
{
	if(rank == 0)
	{
        FILE *fp;
        //if((fp=fopen("./schdl1.txt","r"))==NULL)
        if((fp=fopen(fn,"r"))==NULL)
        {
    		printf("filecannotopen\n");
        }
    	fscanf(fp,"%d",&msgn);
    	schd =(IphComm * )malloc(sizeof(IphComm) * msgn); 
    	int c = 0;
    	for(c = 0;c<msgn;c++)
    	{
			fscanf(fp,"%d",&schd[c].source);
			fscanf(fp,"%d",&schd[c].target);
			fscanf(fp,"%d",&schd[c].size);
			fscanf(fp,"%d",&schd[c].round);
			fscanf(fp,"%d",&schd[c].delay);
			schd[c].time_end = 0.0;
	//		schd[c].delay = 1.5*schd[c].size;
    	}
    	fclose(fp);
		dispatch_all_message(&schd,&msgn,0,rank,comm);
	}else{
		dispatch_all_message(&schd,&msgn,0,rank,comm);
	}
	//print_message_vec(rank,schd,msgn);
}

inline double time_start()
{
	struct timeval start;
	gettimeofday( &start, NULL );
	return start.tv_sec*1000000.0+start.tv_usec;
}
double get_time(double in)
{
    gettimeofday( &end, NULL );
    return end.tv_sec*1000000.0+end.tv_usec - in; 
}
