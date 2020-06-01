#!/bin/bash

init='export PATH="/home/giorgos/mpich-install/bin:$PATH"'
eval $init

compile="mpicc -o project_mpi_reduce_omp project_mpi_reduce_omp.c -lm -fopenmp -g3"
compare="diff PM/initial.bin PM/output.bin"
delete="rm ./PM/*.bin | rm ./PM/*.txt "

eval $delete > ./PM/del.txt
eval $compile

#declare -A array_of_tasks
array_of_tasks=( 1 4 9 16 25 36 49 64 81 100 121 128 144 160 )
size=14

#for i in "${array_of_tasks[@]}" 
#do
    tasks=$1
    threads=$2
    X=$3
    Y=$4
    #task=$i
    #echo -n $i
    exec1="time mpirun -n $tasks -genv OMP_NUM_THREADS=$threads -genv I_MPI_PIN_DOMAIN=omp ./project_mpi_reduce_omp $tasks $threads $X $Y"
    #eval $exec1
    eval $exec1 >  ./PM/out.txt
    eval $compare

#done

exit 0
