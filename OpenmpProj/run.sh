#!/bin/bash

compile="mpicc -o project_mpi_reduce_omp project_mpi_reduce_omp.c -lm -fopenmp -g3"
compare="diff PM/initial.bin PM/output.bin"
delete="rm ./PM/*.bin | rm ./PM/*.txt "

eval $delete > ./PM/del.txt
eval $compile

tasks=$1
threads=$2

exec1="time mpirun -n $tasks -genv OMP_NUM_THREADS=$threads -genv I_MPI_PIN_DOMAIN=omp ./project_mpi_reduce_omp $tasks $threads $3 $4"

eval $exec1 >  ./PM/out.txt
eval $compare

exit 0
