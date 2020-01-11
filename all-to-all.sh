#!/bin/bash
#PBS -N all-to-all
#PBS -l nodes=16:ppn=1
#PBS -j oe
#PBS -l walltime=01:59:0
#PBS -q debug

cd $PBS_O_WORKDIR
NP='cat $PBS_NODEFILE|wc -l'
#source  /public/software/profile.d/mpi_openmpi-gnu-3.1.3.sh
source /public/software/profile.d/mpi_intelmpi-2017.4.239.sh
#source /public/home/ch190467/Soft/mpi_intelmpi-2018.1.163.sh
DIR=`pwd`
num=1
traceN=all_to_all_Gen
procn=16
procPerNode=1

#python $DIR/main.py $traceN $procn $size
mpirun -n $procn -ppn 1  -print-rank-map ./pjt_trace_comm ./schdl.txt 0
python $DIR/main.py $traceN $procn $size
#wait
loopN=10
for((size=0;size<=24;size++))
do
        echo "----------traceN=$traceN size=$size---------"
        echo "----------before---------"
        for((loop=0;loop<$loopN;loop++))
        do
                mpirun   -n $procn -ppn 1  ./pjt_trace_comm ./origin.txt 0
                wait
        done

        echo "----------after---------"
        for((loop=0;loop<$loopN;loop++))
        do
                mpirun   -n $procn -ppn 1 ./pjt_trace_comm ./after.txt 0
                wait
        done
done
# for((size=0;size<28;size++))
# do
#     python $DIR/1-1-gentrace.py $size $num
#     wait
#     #mpirun  -n 4 -ppn 1 -IB ./pjt_trace_comm ./schdl.txt 0
#     mpirun   -n 4 -N 1 ./pjt_trace_comm ./schdl.txt 0
#     wait
# done