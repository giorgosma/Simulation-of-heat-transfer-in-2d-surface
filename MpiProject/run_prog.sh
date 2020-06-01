#!/bin/bash

init='export PATH="/home/giorgos/mpich-install/bin:$PATH"'
eval $init

#compile="mpicc -o project_vol4 project_vol4.c -lm"
compile="mpicc -o project_mpi project_mpi.c -lm"
#compile="mpicc -o project_mpi project_mpi.c -lm"
compare="diff PM/initial.bin PM/output.bin"
delete="rm ./PM/*.bin | rm ./PM/*.txt"

eval $delete > ./PM/del.txt
eval $compile
    
#declare -A array_of_tasks
array_of_tasks=( 1 4 9 16 25 36 49 64 81 100 121 128 144 160 )
size=14
X=$2
Y=$3

#for i in "${array_of_tasks[@]}" 
#do
    task=$1
    #task=$i
    #echo -n $i
    #exec1="time mpiexec -n $task ./project_vol4 $task"
    exec1="time mpiexec -n $task ./project_mpi $task $X $Y"
    #exec1="time mpiexec -n $task ./project_mpi $task"        
    eval $exec1 >  ./PM/out.txt
    eval $compare

#done

exit 0
