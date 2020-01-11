#!/bin/bash
#PBS -N 2d-stencil
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
x=4
y=4
procn=$((x*y))
traceN=Stencil_Gen
procPerNode=1
loopN=10
mpirun -n $procn -ppn 1  -print-rank-map ./pjt_trace_comm ./schdl.txt 0
for((size=0;size<=24;size++))
do
        python $DIR/main.py $traceN $x $y $size
        wait
        echo "----------traceN=$traceN x=$x y=$y size=$size---------"
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
# mpirun  -N 1 -n $procn  ./pjt_trace_comm ./schdl.txt 0
# for((size=24;size<=24;size++))
# do
#         python $DIR/main.py $traceN $x $y $size
#         wait
#         echo "----------traceN=$traceN x=$x y=$y size=$size---------"
#         echo "----------before---------"
#         for((loop=0;loop<$loopN;loop++))
#         do
#                 mpirun   -N 1 -n $procn  ./pjt_trace_comm ./origin.txt 0
#                 wait
#         done
#         echo "----------after---------"
#         for((loop=0;loop<$loopN;loop++))
#         do
#                 mpirun   -N 1 -n $procn ./pjt_trace_comm ./after.txt 0
#                 wait
#         done
# done
# echo "----------after1---------"
# for((factor=0;factor<20;factor+=5))
# do
#     echo "---------factor=$factor-----------"
#     for((loop=0;loop<$loopN;loop++))
#     do
#             mpirun   -N 1 -n $procn ./pjt_trace_comm ./after1.txt $factor
#             wait
#     done
# done 
# for((size=20;size<25;size++))
# do
#     echo "-------------------------size = 1<<$size -------------------------------------"
#     python main.py $x $y $size
#     wait
#     for((loop=0;loop<10;loop++))
#     do
#         #mpirun  -n 4 -ppn 1 -IB ./pjt_trace_comm ./schdl.txt 0 
#         mpirun -N 1 -n $procn   ./pjt_trace_comm ./2d-stencil-origin.txt 0 
#         wait
#     done
#     echo "--------------------------------------------"
#     for((factor=0;factor<=100;factor+10))
#     do
#     echo "-----------factor = $factor------------------"
#         for((loop=0;loop<10;loop++))
#         do
#             #mpirun  -n 4 -ppn 1 -IB ./pjt_trace_comm ./schdl.txt 0 
#             mpirun -N 1 -n $procn   ./pjt_trace_comm ./2d-stencil-after.txt $factor 
#             wait
#         done
#     done
# done
# for((size=0;size<28;size++))
# do
#     python $DIR/1-1-gentrace.py $size $num
#     wait
#     #mpirun  -n 4 -ppn 1 -IB ./pjt_trace_comm ./schdl.txt 0 
#     mpirun   -n 4 -N 1 ./pjt_trace_comm ./schdl.txt 0 
#     wait
# done