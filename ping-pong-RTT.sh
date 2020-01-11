#!/bin/bash
#PBS -N rtt
#PBS -l nodes=2:ppn=1
#PBS -j oe
#PBS -l walltime=0:10:0
#PBS -q debug 
cd $PBS_O_WORKDIR
NP='cat $PBS_NODEFILE|wc -l'
source /public/software/profile.d/mpi_openmpi-gnu-3.1.3.sh
#source /public/software/profile.d/mpi_intelmpi-2017.4.239.sh
DIR=`pwd`
num=1
mpirun -n 2  ./pjt_trace_comm 1
for((loop=0;loop<10;loop++))
do
    echo "---------------loop $loop----------------------"
    for((size=0;size<28;size++))
    do
        mpirun -n 2  ./pjt_trace_comm $size
        wait
    done
done