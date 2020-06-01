#!/bin/bash

compile="mpicc -o project_mpi_reduce project_mpi_reduce.c -lm"
compare="diff PM/initial.bin PM/output.bin"
delete="rm ./PM/*.bin | rm ./PM/*.txt"

eval $delete > ./PM/del.txt
eval $compile

X=$2
Y=$3

task=$1
exec1="time mpiexec -n $task ./project_mpi_reduce $task $X $Y"  
eval $exec1 >  ./PM/out.txt
eval $compare

exit 0
