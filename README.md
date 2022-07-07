# An out-of-tree dialect template for MLIR

This repository contains a template for an out-of-tree [MLIR](https://mlir.llvm.org/) dialect as well as a
standalone `opt`-like tool to operate on that dialect.

## How to build LLVM

```
git clone https://github.com/llvm/llvm-project.git
mkdir llvm-project/build
cd llvm-project/build
cmake -G Ninja ../llvm \
   -DLLVM_ENABLE_PROJECTS=mlir \
   -DLLVM_BUILD_EXAMPLES=ON \
   -DLLVM_TARGETS_TO_BUILD="X86;NVPTX;AMDGPU" \
   -DCMAKE_BUILD_TYPE=Release \
   -DLLVM_ENABLE_ASSERTIONS=ON \
```

LLVM commit to use: `cc2a614796cb311e65ee658200b58b14c53089a8`

## How to build

This setup assumes that you have built LLVM and MLIR in `$BUILD_DIR` and installed them to `$PREFIX`. To build and launch the tests, run
```sh
mkdir build && cd build
cmake -G Ninja .. -DMLIR_DIR=$PREFIX/lib/cmake/mlir -DLLVM_EXTERNAL_LIT=$BUILD_DIR/bin/llvm-lit
cmake --build . --target check-standalone-opt
```
To build the documentation from the TableGen description of the dialect
operations, run
```sh
cmake --build . --target mlir-doc
```
**Note**: Make sure to pass `-DLLVM_INSTALL_UTILS=ON` when building LLVM with
CMake so that it installs `FileCheck` to the chosen installation prefix.

## License

This dialect template is made available under the Apache License 2.0 with LLVM Exceptions. See the `LICENSE.txt` file for more details.

## Problems

1. **Expect some tests to fail**

2. **LIBXSMM compilation fails**

To fix it: go in `tpp-sandbox/build/_deps/xsmm-src/include/libxsmm_config.h`
and comment out `include "libxsmm_version.h"` line 12

then you are good to go.

## Note:

- check: getSubMap
- check: getSliceMap
- https://libxsmm.readthedocs.io/en/latest/libxsmm_aux/#meta-image-file-io
- Nice link for conv: https://d2l.ai/chapter_convolutional-neural-networks/padding-and-strides.html

```
#trait1 = {
  indexing_maps = [
    affine_map<(i, j) -> (i, j)>,  // input a
    affine_map<(i, j) -> (i, j)>   // input b
    affine_map<(i, j) -> (i, j)>   // output c
  ],
  iterator_types = ["parallel", "parallel"],
}

tpp.add ins(%arg0: tensor<2x2xf32>, %arg1: tensor<2x2xf32>) out(tensor<2x2xf32) #trait1
```

How to match something like:

```
func.func @add_d(%arga: tensor<32xf32, #DV>, %argb: f32, %argx: tensor<32xf32>) -> tensor<32xf32> {
  %0 = linalg.generic #trait1
     ins(%arga: tensor<32xf32, #DV>)
    outs(%argx: tensor<32xf32>) {
      ^bb(%a: f32, %x: f32):
        %0 = arith.addf %a, %argb : f32
        linalg.yield %0 : f32
  } -> tensor<32xf32>
  return %0 : tensor<32xf32>
}
```

- in IREE: Codegen/Common/ConvertToDestinationPassingStylePass.cpp
