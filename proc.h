#ifndef IPH_Proc
#define IPH_Proc
#include "IphCommTrace.h"

void read_file(int rank,char *fn,MPI_Comm comm);
extern IphComm *schd;
extern int msgn;
double time_start();
double get_time(double in);

extern IphComm *schd;
extern int msgn;
#endif
