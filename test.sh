#!/bin/bash
#PBS -N test
#PBS -l nodes=8:ppn=1
#PBS -j oe
#PBS -l walltime=0:7:0
#PBS -q debug 
cd $PBS_O_WORKDIR
NP='cat $PBS_NODEFILE|wc -l'
#source  /public/software/profile.d/mpi_openmpi-gnu-3.1.3.sh
source /public/software/profile.d/mpi_intelmpi-2017.4.239.sh
#source /public/home/ch190467/Soft/mpi_intelmpi-2018.1.163.sh
DIR=`pwd`
procn=8
loopn=10
delay=0
# for((loop=0;loop<$loopn;loop++))
# do
#  mpirun  -N 1 -n $procn ./pjt_trace_comm ./schdl.txt 0
#  wait
# done
mpirun -n 8 -ppn 1  -print-rank-map ./pjt_trace_comm ./schdl.txt 0
echo "---------------------------origin---------------------------"
for((loop=0;loop<$loopn;loop++))
do
    mpirun   -n $procn -ppn 1 ./pjt_trace_comm ./origin.txt $delay
done
echo "---------------------------after---------------------------"
for((loop=0;loop<$loopn;loop++))
do
    mpirun  -n $procn -ppn 1 ./pjt_trace_comm ./after.txt $delay
done

# echo "---------------------------origin---------------------------"
# for((loop=0;loop<$loopn;loop++))
# do
#     mpirun  -N 1 -n $procn ./pjt_trace_comm ./origin.txt $delay
# done
# echo "---------------------------after---------------------------"
# for((loop=0;loop<$loopn;loop++))
# do
#     mpirun  -N 1 -n $procn ./pjt_trace_comm ./after.txt $delay
# done



# echo "---------------------------after1---------------------------"
# for((loop=0;loop<$loopn;loop++))
# do
#     mpirun  -N 1 -n $procn ./pjt_trace_comm ./after1.txt $delay
# done


# for((size=0;size<28;size++))
# do
#     python $DIR/1-1-gentrace.py $size $num
#     wait
#     #mpirun  -n 4 -ppn 1 -IB ./pjt_trace_comm ./schdl.txt 0 
#     mpirun   -n 4 -N 1 ./pjt_trace_comm ./schdl.txt 0 
#     wait
# done
