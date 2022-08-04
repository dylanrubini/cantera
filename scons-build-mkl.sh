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

scons clean
# scons build -j 18 env_vars=all CXX="icpx" CC="icx" python_package="full" matlab_toolbox="n" f90_interface="n" logging="debug" system_eigen="n" system_fmt="n" system_yamlcpp="n" system_sundials="n" debug="no" boost_inc_dir="/home/orie3565/anaconda3/envs/ct-build/include" blas_lapack_libs="mkl_intel_lp64,mkl_intel_thread,mkl_core,iomp5,m,dl", blas_lapack_dir="/home/orie3565/anaconda3/envs/ct-build/lib" 
scons -j 18 build env_vars=all CXX="icpx" CC="icx" python_package="full" matlab_toolbox="n" f90_interface="n" logging="debug" system_eigen="n" system_fmt="n" system_yamlcpp="n" system_sundials="n" debug="no" boost_inc_dir="/home/orie3565/anaconda3/envs/ct-build/include" blas_lapack_libs="mkl_rt,dl", blas_lapack_dir="/home/orie3565/anaconda3/envs/ct-build/lib" 

# scons -j 18 test
# sudo -s 
# source /datapart1/spack/share/spack/setup-env.sh 
# scons -j 18 install
