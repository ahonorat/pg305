#!/bin/bash
#
#SBATCH --job-name=test_mpi
#SBATCH --output=res_mpi.txt

#SBATCH -N 2
## 1 //2 outputs an error but works anyway

#SBATCH --ntasks=1
# //SBATCH --ntasks-per-node=1

#SBATCH --cpus-per-task=24
## 24

#SBATCH --time=0-00:11:00
#SBATCH --mem-per-cpu=128
#SBATCH --partition=info


n=1 ## not used
p=7 ## number of slaves + master
t=5 ## number of threads (not accounting the com thread)
a=abcdefghijklmnopqrstuvwxyz
r=6
m=passwd
c=./pg305/slave

#srun #compile OpenMPI with --with-pmi=/path/to/includesOfPMI option?
#srun mpiexec --oversubscribe --mca btl self,sm,tcp --np ${n} ./pg305/master -p ${p} -t ${t} -a ${a} -r ${r} -m ${m} -c ${c}
mpirun --oversubscribe --mca btl self,sm,tcp ./pg305/master -p ${p} -t ${t} -a ${a} -r ${r} -m ${m} -c ${c}
