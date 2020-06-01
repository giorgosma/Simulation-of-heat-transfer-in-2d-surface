#!/bin/bash
init="module load mpiP openmpi"
compile="mpicc -O3 project_mpi_pers_com.c -o project_mpi_pers_com.x -lm"
#compare="diff PM/initial.bin PM/output.bin"
delete="rm ./PM/*.bin | rm ./PM/*.txt"

eval $delete > ./PM/del.txt
#eval $init
#eval $compile

#X=$2
#Y=$3
#task=$1
    
#exec1="time mpiexec -n $task ./project_mpi $task $X $Y"       
#eval $exec1 >  ./PM/out.txt
#eval $compare


