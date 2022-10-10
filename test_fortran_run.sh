#!/bin/sh

conda activate ct-build
# conda activate ct-build-parallel-mkl

module purge

# set-up/source spack environment variables
unset LD_LIBRARY_PATH

# set-up/source spack environment variables
source /datapart1/spack/share/spack/setup-env.sh 

spack clean

spack unload --all
spack load intel-oneapi-compilers@2021.2.0%gcc@11.2.0/dc7udtm

ifort -O3 -pthread test_fortran.F90 -o -I/home/orie3565/anaconda3/envs/ct-build/include/cantera -pthread -Wl,-rpath,/home/orie3565/anaconda3/envs/ct-build/lib \
-Wl,-rpath,/home/orie3565/anaconda3/envs/ct-build/lib \
-L/home/orie3565/anaconda3/envs/ct-build/lib -L/home/orie3565/anaconda3/envs/ct-build/lib \
-Lbuild/lib -lcantera -lmkl_rt -ldl -lcantera_fortran -lstdc++