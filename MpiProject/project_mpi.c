
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

//#define NXPROB      320                 /* x dimension of problem grid */
//#define NYPROB      256                 /* y dimension of problem grid */
#define STEPS       100                /* number of time steps */

struct Parms {
    float cx;
    float cy;
} parms = {0.1, 0.1};

typedef struct Row_Column {
    int x;
    int y;
    int data_size;  // SIZE OF BLOCK
    
    int up;
    int down;
    int left;
    int right;

    MPI_Request Isend_up;
    MPI_Request Irecv_up;
    MPI_Request Isend_down;
    MPI_Request Irecv_down;
    MPI_Request Isend_left;
    MPI_Request Irecv_left;
    MPI_Request Isend_right;
    MPI_Request Irecv_right;
} Row_Column;

int main (int argc, char *argv[]) {
    void inidat(), prtdat(), update();
    void prtdat_bin();

    int NXPROB = atoi(argv[2]);
    int NYPROB = atoi(argv[3]);


    float  u[2][NXPROB][NYPROB];        /* array for grid */
    int	taskid,                         /* this task's unique id */
        numtasks,                       /* number of tasks */
        i,ix,iy,iz,it;                  /* loop variables */
    MPI_Status status;
    int rest = 0;
    int rest_status = 0;
    double startTime, endTime;

    MPI_Datatype up_down_rows;          // MPI DATA TYPE FOR ROWS
    MPI_Datatype left_right_columns;    // MPI DATA TYPE FOR COLS
    
    int tasks = atoi(argv[1]);          // NUM OF TAKSKS

    // PERFECT DIVIDE OF DATA SIZE WITH NUMBER OF TASKS
    while((NXPROB*NYPROB)%tasks != 0) {
		tasks--;
	}
	int sqrt_tasks = sqrt(tasks);           // SQRT ^ 2 = TASKS, NUMBER OF TASKS FOR ROWS  
	int sqrt_tasks2 = tasks / sqrt_tasks;   // SQRT2 = TASKS / SQRT
	rest = tasks % sqrt_tasks;              // EXTRA, BASED ON COTRONIS EXAMPLE

    // MAKE PERFECT SQRT NUMBERS FOR SQRT TASKS(TASK ROWS) AND SQRT TASKS 2(TASK COLUMNS) OF TASKS FOR THIS DATA FILE 
	while (rest != 0) {
		rest_status++;
		if (sqrt_tasks*2 > tasks)           // SQRT TASKS GREATER THAN HALF TASKS
			sqrt_tasks--;
		else                                // SQRT TASKS SMALLER THAN HALF TASKS
			sqrt_tasks++;
		sqrt_tasks2 = tasks / sqrt_tasks;
		rest = tasks % sqrt_tasks;
	}

    // NUM_X (SIZE OF ROW DATA FOR EVERY TASK)
    // NUM_Y (SIZE OF COLUMN DATA FOR EVERY TASK)
	int num_x, num_y;
	if (NXPROB % sqrt_tasks == 0 && NYPROB % sqrt_tasks2 == 0) {
		num_x = NXPROB / sqrt_tasks;
		num_y = NYPROB / sqrt_tasks2;
	}
    // NUM_X (SIZE OF COLUMN DATA FOR EVERY TASK)
    // NUM_Y (SIZE OF ROW DATA FOR EVERY TASK)
 	else if (NXPROB % sqrt_tasks2 == 0 && NYPROB % sqrt_tasks == 0) {
		num_x = NXPROB / sqrt_tasks2;
		num_y = NYPROB / sqrt_tasks;
	}
    // NEVER HAPPEN
	else
		printf("PROBLEM in x OR y\n");

    // ARRAY STRUCT ABOUT INFO FOR EVERY TASK'S DATA BLOCK
    Row_Column *row_column = malloc(tasks*sizeof(Row_Column));
	int j, i_x, i_y;
	i = 0;
    // INIT STARTING ROW AND CLOUMN THESIS WHERE THE TASK WILL READ FROM DATA FILE
    FILE *fp = fopen( "PM/table.txt" , "w" );
	for (i_x = 0; i_x < NXPROB; i_x += num_x) {
		for (i_y = 0; i_y < NYPROB; i_y += num_y) {
            row_column[i].x = i_x;
            row_column[i].y = i_y;
            row_column[i].data_size = num_x*num_y;
            fprintf(fp, "%d\t", i);
			i++;
		}
        fprintf(fp, "\n");
	}
    fclose(fp);

    // U1 AND U2, ARRAY BUFFERS FOR READING DATA BLOCKS, U2 KEEP THE UPDATED DATA AND IN THE END U2 = U1
    float *u1 = NULL, *u2 = NULL, *array_row_pointer = NULL, *tmp = NULL;
    u1 = (float *)calloc((num_x+2) * (num_y+2), sizeof(float));
    u2 = (float *)calloc((num_x+2) * (num_y+2), sizeof(float));
    tmp = u1;
    
    // STARTING MPI
    MPI_Init(&argc,&argv);
    MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&taskid);
	
    // TASK MASTER MAKES THE DATA FILE IN .BIN (PARALLEL I/O FORMAT) AND .TXT
	if (taskid == 0) {         
	    //printf("- tasks: %d \n- sqrt_tasks: %d, sqrt_tasks2: %d \n- num_x: %d, num_y: %d\n", tasks, sqrt_tasks, sqrt_tasks2, num_x, num_y);
        //if (rest_status)
        //    printf("- rest: %d\n", rest_status);  
        inidat(NXPROB, NYPROB, u);
        prtdat_bin(NXPROB, NYPROB, u, "PM/initial.bin");  
        prtdat(NXPROB, NYPROB, u, "PM/initial.txt");
    }
    MPI_Barrier(MPI_COMM_WORLD);    // WAIT UNTIL FILE HAS BEEN CREATED

    // DATATYPE FOR ROWS
    MPI_Type_contiguous(num_y, MPI_FLOAT, &up_down_rows);
    MPI_Type_commit(&up_down_rows);

    // DATATYPE FOR COLUMNS
    MPI_Type_vector(num_x, 1, num_y+2, MPI_FLOAT, &left_right_columns);
    MPI_Type_commit(&left_right_columns);

    // PARALLEL READING FROM DATA FILE AND INITING ARRAY'S STRUCT NEIGHBORS
    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, "PM/initial.bin", MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);
    if (tasks - 1 < taskid){            // EXTRA TASKS WHICH DO NOTHING
        //printf("Im task %d\n", taskid);
    }
    else{
        for (i = 1 ; i <= num_x ; i++) {
            // GO FILE DESCRIPTOR TO DATA FILE'S ROW THESIS
            MPI_File_seek(fh, ((row_column[taskid].x + i-1) * NYPROB + row_column[taskid].y)*sizeof(float), MPI_SEEK_SET);
            // POINTER INSIDE ROW FOR READING NUMBER OF COLUMN DATA FOR EACH ROW
            array_row_pointer = &(u1[(num_y+2) * i + 1]);
            MPI_File_read(fh, array_row_pointer, num_y, MPI_FLOAT, &status);
        }
        // INIT NEIGHBORS TO NULL
        row_column[taskid].up = MPI_PROC_NULL;
        row_column[taskid].down = MPI_PROC_NULL;
        row_column[taskid].left = MPI_PROC_NULL;
        row_column[taskid].right = MPI_PROC_NULL;
        
        // FIND THE colls sqrt
        int temp_sqrt = 0;
        if (NXPROB % sqrt_tasks == 0 && NYPROB % sqrt_tasks2 == 0){
            temp_sqrt = sqrt_tasks2;
        }
        else if (NXPROB % sqrt_tasks2 == 0 && NYPROB % sqrt_tasks == 0){
            temp_sqrt = sqrt_tasks;
        }
        // FIND AND INIT THE REAL NEIGHBORS
        if (row_column[taskid].x != 0)
            row_column[taskid].up = taskid - temp_sqrt;
        if (row_column[taskid].x + num_x != NXPROB && taskid + temp_sqrt < tasks)    
            row_column[taskid].down = taskid + temp_sqrt;
        if (row_column[taskid].y != 0)
            row_column[taskid].left = taskid - 1;
        if (row_column[taskid].y + num_y != NYPROB && taskid + 1 < tasks)
            row_column[taskid].right = taskid + 1;
        //printf("\n-TASK: %d\n\t-up: %d\nleft: %d \t\tright: %d\n\tdown: %d\n", taskid, row_column[taskid].up, row_column[taskid].left, row_column[taskid].right, row_column[taskid].down);
    }
    MPI_File_close(&fh);
        
    // STARTING COUNTING TIME
    MPI_Barrier(MPI_COMM_WORLD);
    startTime = MPI_Wtime();
    if (tasks - 1 < taskid) {}    // EXTRA TASKS THAT DON'T DO NOTHING
    else {
        for (it = 1; it <= STEPS; it++) {
            // DO Isend X 4
                MPI_Isend(&(u1[(num_y+2) * 1 + 1]), 1, up_down_rows, row_column[taskid].up, 0, MPI_COMM_WORLD, &(row_column[taskid].Isend_up));
                    
                MPI_Isend(&(u1[(num_y+2) * (num_x + 0) + 1]), 1, up_down_rows, row_column[taskid].down, 0, MPI_COMM_WORLD, &(row_column[taskid].Isend_down));
                    
                MPI_Isend(&(u1[(num_y+2) * 1 + 1]), 1, left_right_columns, row_column[taskid].left, 0, MPI_COMM_WORLD, &(row_column[taskid].Isend_left));
                    
                MPI_Isend(&(u1[(num_y+2) * 1 + (num_y + 0)]), 1, left_right_columns, row_column[taskid].right, 0, MPI_COMM_WORLD, &(row_column[taskid].Isend_right));

            // DO Irecv X 4
                MPI_Irecv(&(u1[(num_y+2) * 0 + 1]), 1, up_down_rows, row_column[taskid].up, 0, MPI_COMM_WORLD, &row_column[taskid].Irecv_up);
                
                MPI_Irecv(&(u1[(num_y+2) * (num_x + 1) + 1]), 1, up_down_rows, row_column[taskid].down, 0, MPI_COMM_WORLD, &(row_column[taskid].Irecv_down));
                
                MPI_Irecv(&(u1[(num_y+2) * 1 + 0]), 1, left_right_columns, row_column[taskid].left, 0, MPI_COMM_WORLD, &(row_column[taskid].Irecv_left));
                
                MPI_Irecv(&(u1[(num_y+2) * 1 + (num_y + 1)]), 1, left_right_columns, row_column[taskid].right, 0, MPI_COMM_WORLD, &(row_column[taskid].Irecv_right));


            // UPDATE INTERNALS OF ARRAY
                update(2, num_x-1, 2, num_y-1, num_y+2, u1, u2);

            // WAIT FOR RECEIVERS X 4
                MPI_Wait(&(row_column[taskid].Irecv_up), &status);

                MPI_Wait(&(row_column[taskid].Irecv_down), &status);

                MPI_Wait(&(row_column[taskid].Irecv_left), &status);

                MPI_Wait(&(row_column[taskid].Irecv_right), &status);

            // UPDATE EXTERNALS OF ARRAY( NORTH, SOUTH, WEST, EAST)
                update(1, 1, 1, num_y, num_y+2, u1, u2);

                update(num_x, num_x, 1, num_y, num_y+2, u1, u2);

                update(2, num_x-1, 1, 1, num_y+2, u1, u2);

                update(2, num_x-1, num_y, num_y, num_y+2, u1, u2);

            // OLD ARRAY(U1) = NEW ARRAY(U2)
            u1 = u2;

            // WAIT FOR SENDERS X 4
                MPI_Wait(&(row_column[taskid].Isend_up), &status);

                MPI_Wait(&(row_column[taskid].Isend_down), &status);

                MPI_Wait(&(row_column[taskid].Isend_left), &status);

                MPI_Wait(&(row_column[taskid].Isend_right), &status);

        }
    }
    // STOP CONTING TIME
    endTime = MPI_Wtime();

    double counting_time = endTime-startTime;
    double counting_time_avg = counting_time;
    double counting_time_max = counting_time;
    double counting_time_min = counting_time;
    if (taskid == 0) {          // MASTER TASK TAKES ALL TIMES AND MAKE AVG, MIN AND MAX TIME
        double temp_time = 0;
        int i;
        for(i = 1; i < tasks; i++) {    // FOR EVERY TASK RECEIV ITS TIME
            MPI_Recv(&temp_time, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, &status);
            counting_time_avg += temp_time;
            if (temp_time > counting_time_max)
                counting_time_max = temp_time;
            if (temp_time < counting_time_min)
                counting_time_min = temp_time;
        }
        counting_time_avg = counting_time_avg / tasks;
        printf("--TASKS: %d\tCOUNTING AVG: %f\tCOUNTING MAX: %f\tCOUNTING MIN: %f\n", tasks, counting_time_avg, counting_time_max, counting_time_min);
    }
    else
    {
        if (tasks - 1 >= taskid)    // IF TASK HAS DATA BLOCK SEND ITS TIME TO MASTER TASK
            MPI_Send(&counting_time, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    }
    //printf("-TASK: %d\tTIME: %f\n", taskid, endTime-startTime);
    
    // PARALLEL WRITING TO DATA FILE
    MPI_File_open(MPI_COMM_WORLD, "PM/output.bin", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);
    if (tasks - 1 < taskid){}   // EXTRA TASKS, HAVE NOTHING TO WRITE
    else{
        for (i = 1 ; i <= num_x ; i++) {
            // GO FILE DESCRIPTOR TO DATA FILE'S ROW THESIS
            MPI_File_seek(fh, ((row_column[taskid].x + i-1) * NYPROB + row_column[taskid].y)*sizeof(float), MPI_SEEK_SET);
            // POINTER INSIDE ROW FOR WRITING NUMBER OF COLUMNS OF EACH ROW
            array_row_pointer = &(u1[(num_y+2) * i + 1]);
            MPI_File_write(fh, array_row_pointer, num_y, MPI_FLOAT, MPI_STATUS_IGNORE);
        }
    }
    MPI_File_close(&fh);

    MPI_Finalize();
    free(row_column);
    free(u2);
    free(tmp);
    exit(0);

}


/**************************************************************************
 *  subroutine update
 ****************************************************************************/
// CHANGED UPDATE WHICH PASSES NUMBER OF num_x, num_y OF DATA BLOCK SIZES
inline void update(int start, int end, int y_st, int y_end, int numy, float *u1, float *u2) {
    int ix, iy;
    for (ix = start; ix <= end; ix++) {
        for (iy = y_st; iy <= y_end; iy++) {
            *(u2+ix*numy+iy) = *(u1+ix*numy+iy)  +
                            parms.cx * (*(u1+(ix+1)*numy+iy) +
                            *(u1+(ix-1)*numy+iy) -
                            2.0 * *(u1+ix*numy+iy)) +
                            parms.cy * (*(u1+ix*numy+iy+1) +
                            *(u1+ix*numy+iy-1) -
                            2.0 * *(u1+ix*numy+iy));
        }
    }
}
/*****************************************************************************
 *  subroutine inidat
 *****************************************************************************/
inline void inidat(int nx, int ny, float *u) {
    int ix, iy;

    for (ix = 0; ix <= nx-1; ix++){
        for (iy = 0; iy <= ny-1; iy++){
            *(u+ix*ny+iy) = (float)(ix * (nx - ix - 1) * iy * (ny - iy - 1));
        }
    }
}
/**************************************************************************
* subroutine prtdat
**************************************************************************/
inline void prtdat(int nx, int ny, float *u1, char *fnam) {
    int ix, iy;
    FILE *fp;
    fp = fopen(fnam, "w");
    for (ix = 0; ix <= nx-1; ix++) {
        for (iy = 0; iy <= ny-1; iy++) {
            fprintf(fp, "%6.1f", *(u1+ix*ny+iy));
            if (iy != ny-1)
                fprintf(fp, " ");
            else
                fprintf(fp, "\n");
        }
    }
    fclose(fp);
}
// WRITING DATA IN A .BIN FILE, REQUIRED FOR PARALLEL I/O 
inline void prtdat_bin(int nx, int ny, float *u1, char *fnam) {
    int ix, iy;

    int fd = open(fnam, O_RDWR | O_CREAT, 0755);
    for (ix = 0; ix <= nx-1; ix++) {
        for (iy = 0; iy <= ny-1; iy++) {
            write(fd, (u1+ix*ny+iy), sizeof(float));
        }
    }
    close(fd);
}

