/* Ex4 - MPI SOLVING LAPLACE EQUATION USING JACOBI METHOD
 * - Parallel Programming 
 * - Prof. Ivan Girotto
 * - Exercise submitted by Keerthana C J 
 *
 * The program is a parallelization of the serial code in the ../code directory
 * which is used to solve the 2D Laplace equation using Jacobi Method.
 *
 * The problem involves boundary exchange between processors
 * Both Blocking & Non-Blocking communication are tested and their scaling is 
 * performed. 
 */
#include <mpi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>


/*** function declarations ***/

// save matrix to file
void save_gnuplot( FILE* file, double *M, size_t dim, size_t start, size_t end);

// evolve Jacobi
void evolve( double * matrix, double *matrix_new, size_t row_start, size_t row_end, size_t dimension );

// print the matrix
void print ( double *matrix, size_t loc_dimension, size_t dimension);

// exchange boundaries - non blocking communication
void exchange_borders_nonblocking (double *matrix, size_t loc_dimension, size_t dimension, int prev, int next, MPI_Datatype MPI_ARRAYROW, MPI_Request **request);

// exchange boundaries - blocking communication
void exchange_borders_blocking (double *matrix, size_t loc_dimension, size_t dimension, int prev, int next, MPI_Datatype MPI_ARRAYROW);

// return the elapsed time
double seconds( void );

/*** end function declaration ***/



int main(int argc, char* argv[]){
			
  //MPI Initialize
  int rank, np, prev, next;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Status status[2];
  MPI_Request* request;
  request = (MPI_Request *)malloc(2*sizeof(MPI_Request));


  // timing variables
  double t_start, t_end, t_start_comm, t_end_comm, t_start_comp, t_end_comp, t_comm = 0, t_comp = 0, increment, increment_offset;

  // indexes for loops
  size_t i, j, it;
  
  // initialize matrix
  double *matrix, *matrix_new, *tmp_matrix;

  size_t dimension = 0, iterations = 0, row_peek = 0, col_peek = 0, loc_dimension = 0, res = 0, offset = 0;
  size_t byte_dimension = 0;

  // check on input parameters
  if(rank == 0 && argc != 5) {
    fprintf(stderr,"\nwrong number of arguments. Usage: ./a.out dim it n m\n");
    MPI_Abort(MPI_COMM_WORLD,1);
    exit(-1);
  }

  dimension = atoi(argv[1]);
  loc_dimension = dimension/np;
  res = dimension%np;
  if(rank < res) loc_dimension++;
  iterations = atoi(argv[2]);
  row_peek = atoi(argv[3]);
  col_peek = atoi(argv[4]);

  if(rank == 0){
    printf("matrix size = %zu\n", dimension);
    printf("number of iterations = %zu\n", iterations);
    printf("element for checking = Mat[%zu,%zu]\n",row_peek, col_peek);

   if((row_peek > dimension) || (col_peek > dimension)){
     fprintf(stderr, "Cannot Peek a matrix element outside of the matrix dimension\n");
     fprintf(stderr, "Arguments n and m must be smaller than %zu\n", dimension);
     return 1;
   }
  }

  //allocate local matrices
  byte_dimension = sizeof(double) * ( dimension + 2 ) * ( loc_dimension + 2 );
  matrix = ( double* )malloc( byte_dimension );
  matrix_new = ( double* )malloc( byte_dimension );

  memset( matrix, 0, byte_dimension );
  memset( matrix_new, 0, byte_dimension );

  //fill initial values in the whole domain and boundaries
  for( i = 0; i <= loc_dimension+1; ++i )
    for( j = 1; j <= dimension; ++j )
      matrix[ ( i * ( dimension + 2 ) ) + j ] = 0.5;
	      
  // set up boundary conditions 
  increment = 100.0 / ( dimension+1 );
  if(rank >= res) offset = res;
  
  // left boundary condition in the domain
  for( i=0; i < loc_dimension+1; ++i ){
    matrix[ i * ( dimension + 2 ) ] = i * increment + rank * loc_dimension * increment + offset * increment;
    matrix_new[ i * ( dimension + 2 ) ] = i * increment + rank * loc_dimension * increment + offset * increment;
  }

  // bottom boundary condition in the bottom boundary
  if(rank == np-1){
   for( i=0; i<= dimension; ++i){
     matrix[(loc_dimension+1) * (dimension + 2) + i] = (dimension + 1 - i) * increment;
     matrix_new[(loc_dimension+1) * (dimension + 2) + i] = (dimension + 1 - i) * increment;
   }
  }
  //left boundary condition in the bottom boundary
  else{
   matrix[(loc_dimension + 1) * (dimension + 2)] = increment + (rank+1) * loc_dimension * increment + offset * increment;
   matrix_new[(loc_dimension + 1) * (dimension + 2)] = increment + (rank+1) * loc_dimension * increment + offset * increment;
  }

  //top boundary condition in the top boundary
  if(rank == 0){
   for( i=0; i<=dimension+1; ++i){
     matrix[i] = 0.0;
     matrix_new[i] = 0.0; 
   }
  }
  //left boundary condition in the top boundary
  else{
   matrix[0] = loc_dimension * increment + (rank-1) * loc_dimension * increment + offset * increment;
   matrix_new[0] = loc_dimension * increment + (rank-1) * loc_dimension * increment + offset * increment;
  }


  prev = (rank - 1);
  next = (rank + 1);

  if(rank == 0) prev = MPI_PROC_NULL;
  if(rank == np-1) next = MPI_PROC_NULL;
  
  MPI_Datatype MPI_ARRAYROW;
  MPI_Type_contiguous(dimension+2, MPI_DOUBLE, &MPI_ARRAYROW);
  MPI_Type_commit(&MPI_ARRAYROW);

  // start algorithm
  t_start = seconds();
  for( it = 0; it < iterations; ++it ){
    
   //Exchange borders	  
#ifdef BLOCKING	  //Blocking Communication
    t_start_comm = seconds();
   // printf("Inside Blocking\n");
    exchange_borders_blocking(matrix, loc_dimension, dimension, prev, next, MPI_ARRAYROW);
    t_end_comm = seconds();   
#else	//Non-blocking Communication
    t_start_comm = seconds();
    exchange_borders_nonblocking(matrix, loc_dimension, dimension, prev, next, MPI_ARRAYROW, &request);
#endif
    
    //evolve interior
    t_start_comp = seconds();
    evolve( matrix, matrix_new, 2, loc_dimension, dimension );
    t_end_comp = seconds();

#ifndef BLOCKING    
    MPI_Waitall(2, request, status);
    t_end_comm = seconds();
#endif

    //evolve borders
    evolve( matrix, matrix_new, loc_dimension, loc_dimension+1, dimension);
    evolve( matrix, matrix_new, 1, 2, dimension);

    // swap the pointers
    tmp_matrix = matrix;
    matrix = matrix_new;
    matrix_new = tmp_matrix;

    t_comm = t_comm + t_end_comm - t_start_comm;
    t_comp = t_comp + t_end_comp - t_start_comp;
  }
  t_end = seconds();
  

   printf( "\n total elapsed time on processor %d = %f seconds\n", rank, t_end - t_start );
   printf( "\n communication elapsed time on processor %d = %f seconds\n",rank, t_comm );
   printf("\n computation time for the evolving the bulk by processor %d = %f seconds\n",rank, t_comp);
   printf("\n");
  
  
  //peek into row and coloumn
  size_t glob_dimension_start = rank * loc_dimension + 1  +  offset; // to understand which processor has the point we need falls into
  size_t glob_dimension_end = glob_dimension_start + loc_dimension - 1;
  if(row_peek >= glob_dimension_start && row_peek <= glob_dimension_end){
    size_t loc_dimension_row_peek = row_peek - (rank * loc_dimension) - offset;
    printf( "\nmatrix[%zu,%zu] = %f\n", row_peek, col_peek, matrix[ ( loc_dimension_row_peek + 1 ) * ( dimension + 2 ) + ( col_peek + 1 ) ] );
  }
 

 
  //dump into files
  if(rank != 0 && rank != np-1) MPI_Send(matrix+(dimension+2), loc_dimension, MPI_ARRAYROW, 0, 300, MPI_COMM_WORLD);
  if(rank == np-1) MPI_Send(matrix+(dimension+2), loc_dimension+1, MPI_ARRAYROW, 0, 300, MPI_COMM_WORLD);
  if(rank == 0){
     FILE* file;
     file = fopen( "solution_mpi.dat", "w" );
     save_gnuplot(file, matrix, dimension, 0, loc_dimension);
     for(i=1; i<np; ++i){
       if(i == res)loc_dimension--;
       if(i == np-1)loc_dimension++;
       MPI_Recv(matrix+(dimension+2), loc_dimension, MPI_ARRAYROW, i, 300, MPI_COMM_WORLD,MPI_STATUS_IGNORE);
       save_gnuplot(file, matrix, dimension, 1, loc_dimension);
     }
     fclose(file);
  }

  //if(rank == 0) print(matrix, loc_dimension, dimension);
  
  free( matrix );
  free( matrix_new );

  MPI_Finalize();

  return 0;
}

void exchange_borders_nonblocking (double *matrix, size_t loc_dimension, size_t dimension, int prev, int next, MPI_Datatype MPI_ARRAYROW, MPI_Request **request){
   
   MPI_Isend(matrix + (dimension+2), 1, MPI_ARRAYROW, prev, 100, MPI_COMM_WORLD, *request);
   MPI_Isend(matrix + loc_dimension*(dimension+2), 1, MPI_ARRAYROW, next, 200, MPI_COMM_WORLD, *request+1);
   MPI_Irecv(matrix, 1, MPI_ARRAYROW, prev, 200, MPI_COMM_WORLD, *request);
   MPI_Irecv(matrix + (loc_dimension+1)*(dimension+2), 1, MPI_ARRAYROW, next, 100, MPI_COMM_WORLD, *request+1);

}


void exchange_borders_blocking (double *matrix, size_t loc_dimension, size_t dimension, int prev, int next, MPI_Datatype MPI_ARRAYROW){
  MPI_Sendrecv(matrix + (dimension+2), 1, MPI_ARRAYROW, prev, 100, matrix + (loc_dimension+1)*(dimension+2), 1, MPI_ARRAYROW, next, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  MPI_Sendrecv(matrix + loc_dimension*(dimension+2), 1, MPI_ARRAYROW, next, 200, matrix, 1, MPI_ARRAYROW, prev, 200, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}


void evolve( double * matrix, double *matrix_new, size_t row_start, size_t row_end, size_t dimension){
  
  size_t i , j;

  //This will be a row dominant program.
  for( i = row_start ; i < row_end; ++i )
    for( j = 1; j <= dimension; ++j )
      matrix_new[ ( i * ( dimension + 2 ) ) + j ] = ( 0.25 ) * 
	( matrix[ ( ( i - 1 ) * ( dimension + 2 ) ) + j ] + 
	  matrix[ ( i * ( dimension + 2 ) ) + ( j + 1 ) ] + 	  
	  matrix[ ( ( i + 1 ) * ( dimension + 2 ) ) + j ] + 
	  matrix[ ( i * ( dimension + 2 ) ) + ( j - 1 ) ] ); 
}


void save_gnuplot( FILE* file, double *M, size_t dimension, size_t row_start, size_t row_end){
  
  size_t i , j;
  const double h = 0.1;

  for( i = row_start; i <= row_end; ++i )
    for( j = 0; j < dimension + 2; ++j )
      fprintf(file, "%f\t%f\t%f\n", h * j, -h * i, M[ ( i * ( dimension + 2 ) ) + j ] );
}


void print(double* matrix, size_t loc_dimension, size_t dimension){
  size_t i, j;
  for( i=0; i<loc_dimension+2; ++i){
   for( j=0; j<dimension+2; ++j){
    printf("%f\t", matrix[i*(dimension+2) + j]);
   }
   printf("\n");
  }
}

// A Simple timer for measuring the walltime
double seconds(){

    struct timeval tmp;
    double sec;
    gettimeofday( &tmp, (struct timezone *)0 );
    sec = tmp.tv_sec + ((double)tmp.tv_usec)/1000000.0;
    return sec;
}

