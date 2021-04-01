#!/bin/bash
#SBATCH --job-name=keerthana_jacobi
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=20
#SBATCH --partition=regular1
#SBATCH --error=jacobi.err
#SBATCH --output=jacobi.out
#SBATCH --time=04:00:00


module load intel/18.0.3.222
module load openmpi/2.1.3

cd /home/kchandra/ParallelProgramming/Jacobi/mpi_code
 
N=1200
it=10
rpeek=102
cpeek=393

for procs in 2 4 8 16 20
 do
    echo ${procs}
    (mpirun -np ${procs} ./mpi_jacobi_blocking.x ${N} ${it} ${rpeek} ${cpeek})> ../output_blocking/N1200_1node/Blocking_${procs}.txt
 done
