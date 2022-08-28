#!/bin/bash

source ../common.sh

compile () {
  echo "Compile driver ----> matmul_driver_${1}"
  echo "Compile kernel ----> matmul_kernel_${1}"
  
  # Compile driver. 
  clang -O3 -emit-llvm -S -I $LIB_INCLUDE_PATH matmul_driver_${1}.c
  llc matmul_driver_${1}.ll

  # Fire tpp compiler (with xsmm conversion).
  standalone-opt matmul_kernel_${1}.mlir -map-linalg-to-tpp -pre-bufferization -one-shot-bufferize="bufferize-function-boundaries allow-return-allocs function-boundary-type-conversion=identity-layout-map" -canonicalize -drop-equivalent-buffer-results -finalizing-bufferize

  standalone-opt matmul_kernel_${1}.mlir -map-linalg-to-tpp -pre-bufferization -one-shot-bufferize="bufferize-function-boundaries allow-return-allocs function-boundary-type-conversion=identity-layout-map" -canonicalize -drop-equivalent-buffer-results -finalizing-bufferize -convert-linalg-to-tpp="enable-tiling" -convert-tpp-to-xsmm -loop-invariant-code-motion -convert-xsmm-to-func -convert-linalg-to-loops -arith-expand -convert-vector-to-scf -convert-scf-to-cf -convert-vector-to-llvm -convert-func-to-llvm -convert-memref-to-llvm -canonicalize -reconcile-unrealized-casts | mlir-translate -mlir-to-llvmir -o matmul_kernel_${1}.ll
  llc matmul_kernel_${1}.ll

  # Merge them.
  unamestr=$(uname)
  if [[ "$unamestr" == 'Darwin' ]]; then
    export DYLD_LIBRARY_PATH=$LIB_PATH:$DYLD_LIBRARY_PATH
  else
    export LD_LIBRARY_PATH=$LIB_PATH:$LD_LIBRARY_PATH
  fi

  clang -O3 matmul_driver_${1}.s matmul_kernel_${1}.s -L$LIB_PATH -lstandalone_c_runner_utils -o matmul_driver_${1}

  rm *.s
  rm *.ll
}

execute () {
  # Execute and check result.
  ./matmul_driver_${1} > matmul_driver_${1}.log 2>&1
  #rm matmul_driver_${1}

 if cat matmul_driver_${1}.log | grep "Result is correct" &> /dev/null ; then
    printf "${GREEN} OK ${NC} \n"
  else
    printf "${RED} Oh NO ${NC} \n";
    exit 1
  fi 
  
  #rm matmul_driver_${1}.log
}


# ----- matmul M = 12 N = 6 K = 9
compile "12x6x9"
execute "12x6x9"

# ----- matmul M = 64 N = 48 and K = 96
compile "64x48x96"
execute "64x48x96"

# ----- matmul M = 48 N = 64 and K = 96
compile "48x64x96"
execute "48x64x96"

# ----- matmul M = 64 N = 64 and K = 64
compile "64x64x64"
execute "64x64x64"
