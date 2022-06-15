//===- TppEnforcePreconditions.cpp -------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Standalone/Dialect/Tpp/TppOps.h"
#include "Standalone/TppPasses.h"
#include "Standalone/TppUtils.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Linalg/Transforms/Transforms.h"
#include "mlir/Dialect/Tensor/Utils/Utils.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

using namespace mlir;
using namespace mlir::tpp;

#define GEN_PASS_CLASSES
#include "Standalone/TppPasses.h.inc"

#define DEBUG_TYPE "enforce-tpp-preconditions"
#define DBGS() (llvm::dbgs() << "[" DEBUG_TYPE << "]: ")

namespace {

// Ensure the SIMD dimension to be multiple of 16, and the parallel dimension
// multiple of 6.
//
// Example (SIMD dimension):
// %0 = tensor.pad (%C) : tensor<3x3xf32> to tensor<3xSIMDxf32>
// %1 = tensor.pad (%B) : tensor<3x3xf32> to tensor<3xSIMDxf32>
// %2 = linalg.generic(%C, %A, %B) {library_call = tpp.matmul}
// %3 = tensor.extract tensor<3xSIMDxf32> to tensor<3x3xf32>
//
struct PadSIMDDimensionForGemm : public OpRewritePattern<linalg::GenericOp> {
  using OpRewritePattern<linalg::GenericOp>::OpRewritePattern;

  // POD for GEMM operands
  struct GemmOperands {
    Value A = nullptr;
    Value B = nullptr;
    Value C = nullptr;
    GemmOperands() = delete;
    GemmOperands(Value A, Value B, Value C) : A(A), B(B), C(C){};
  };

  // Pad SIMD-dimension using a multiple of 16.
  void padSIMDDimension(PatternRewriter &rewriter, GemmOperands &operands,
                        int64_t simdDim, Location loc) const {
    // no work to do, exit.
    if (simdDim % 16 == 0)
      return;

    // compute the closest multiple of 16 and pad the
    // simd dimension accordingly.
    int64_t paddedSimd = 16 * std::ceil((float)simdDim / 16.0);
    ArrayRef<int64_t> shapeB =
        operands.B.getType().cast<ShapedType>().getShape();
    ArrayRef<int64_t> shapeC =
        operands.C.getType().cast<ShapedType>().getShape();
    SmallVector<int64_t> newShapeC = {shapeC[0], paddedSimd};
    SmallVector<int64_t> newShapeB = {shapeB[0], paddedSimd};
    RankedTensorType newRankedC = RankedTensorType::get(
        newShapeC, operands.C.getType().cast<ShapedType>().getElementType());
    RankedTensorType newRankedB = RankedTensorType::get(
        newShapeB, operands.B.getType().cast<ShapedType>().getElementType());
    Value padZero = rewriter.create<arith::ConstantOp>(
        loc, operands.C.getType().cast<ShapedType>().getElementType(),
        rewriter.getZeroAttr(
            operands.C.getType().cast<ShapedType>().getElementType()));
    Value paddedC = tensor::createPadHighOp(newRankedC, operands.C, padZero,
                                            /*nofold*/ false, loc, rewriter);
    Value paddedB = tensor::createPadHighOp(newRankedB, operands.B, padZero,
                                            /*nofold*/ false, loc, rewriter);

    // update operands.
    operands.C = paddedC;
    operands.B = paddedB;
  }

  // Pad Parallel-dimension using a multiple of 6.
  void padParallelDimension(PatternRewriter &rewriter, GemmOperands &operands,
                            int64_t parallelDim, Location loc) const {
    // no work to do, exit.
    if (parallelDim % 6 == 0)
      return;

    // compute the closes multiple of 6 and pad the parallel dimension
    // accordingly.
    int64_t paddedParallel = 6 * std::ceil((float)parallelDim / 6.0);
    ArrayRef<int64_t> shapeA =
        operands.A.getType().cast<ShapedType>().getShape();
    ArrayRef<int64_t> shapeC =
        operands.C.getType().cast<ShapedType>().getShape();
    SmallVector<int64_t> newShapeC = {paddedParallel, shapeC[1]};
    SmallVector<int64_t> newShapeA = {paddedParallel, shapeA[1]};
    RankedTensorType newRankedC = RankedTensorType::get(
        newShapeC, operands.C.getType().cast<ShapedType>().getElementType());
    RankedTensorType newRankedA = RankedTensorType::get(
        newShapeA, operands.A.getType().cast<ShapedType>().getElementType());
    Value padZero = rewriter.create<arith::ConstantOp>(
        loc, operands.C.getType().cast<ShapedType>().getElementType(),
        rewriter.getZeroAttr(
            operands.C.getType().cast<ShapedType>().getElementType()));
    Value paddedC = tensor::createPadHighOp(newRankedC, operands.C, padZero,
                                            /*nofold*/ false, loc, rewriter);
    Value paddedA = tensor::createPadHighOp(newRankedA, operands.A, padZero,
                                            /*nofold*/ false, loc, rewriter);

    // update operands.
    operands.C = paddedC;
    operands.A = paddedA;
  }

  LogicalResult padDimensions(linalg::GenericOp linalgOp,
                              PatternRewriter &rewriter) const {
    Location loc = linalgOp.getLoc();
    GemmOperands operands(linalgOp->getOperand(0), linalgOp->getOperand(1),
                          linalgOp->getOperand(2));

    if ((!operands.C.getType().isa<ShapedType>()) ||
        (!operands.B.getType().isa<ShapedType>()) ||
        (!operands.A.getType().isa<ShapedType>()))
      return failure();

    ArrayRef<int64_t> shapeC =
        operands.C.getType().cast<ShapedType>().getShape();
    ArrayRef<int64_t> shapeB =
        operands.B.getType().cast<ShapedType>().getShape();
    ArrayRef<int64_t> shapeA =
        operands.A.getType().cast<ShapedType>().getShape();

    assert(shapeC.size() == 2 && "expect 2d gemm");
    assert(shapeB.size() == 2 && "expect 2d gemm");
    assert(shapeA.size() == 2 && "expect 2d gemm");

    assert(shapeC[1] == shapeB[1] && "expect equal");
    assert(shapeC[0] == shapeA[0] && "expect equal");
    assert(shapeA[1] == shapeB[0] && "expect equal");

    int64_t simdDim = shapeC[1];
    int64_t parallelDim = shapeC[0];
    // no work to do, exit.
    if ((simdDim % 16 == 0) && (parallelDim % 6 == 0))
      return failure();

    padSIMDDimension(rewriter, operands, simdDim, loc);
    padParallelDimension(rewriter, operands, parallelDim, loc);

    linalg::GenericOp replacementOp = rewriter.create<linalg::GenericOp>(
        loc, operands.C.getType(), ValueRange{operands.A, operands.B},
        ValueRange{operands.C}, linalgOp.getIndexingMaps(),
        llvm::to_vector<4>(
            linalgOp.iterator_types().template getAsValueRange<StringAttr>()),
        /*docs*/ "", /*library_call*/ "tpp.matmul");
    rewriter.inlineRegionBefore(linalgOp.region(), replacementOp.region(),
                                replacementOp.region().begin());

    // create tensor.extract for C.
    unsigned rank = shapeC.size();
    SmallVector<OpFoldResult, 4> offsets, sizes, strides;
    offsets.reserve(rank);
    sizes.reserve(rank);
    strides.reserve(rank);
    for (unsigned r = 0; r < rank; r++) {
      offsets.push_back(rewriter.getIndexAttr(0));
      strides.push_back(rewriter.getIndexAttr(1));
      sizes.push_back(rewriter.getIndexAttr(shapeC[r]));
    }
    Value extract = rewriter.create<tensor::ExtractSliceOp>(
        loc, replacementOp->getResult(0), offsets, sizes, strides);

    rewriter.replaceOp(linalgOp, extract);
    return success();
  }

  LogicalResult matchAndRewrite(linalg::GenericOp linalgOp,
                                PatternRewriter &rewriter) const override {
    if (!linalgOp.hasTensorSemantics() || !hasStaticShape(linalgOp) ||
        !hasTppMark(linalgOp))
      return failure();
    std::string libraryCall = linalgOp.getLibraryCallName();
    if (libraryCall.compare("tpp.matmul") != 0)
      return failure();
    return padDimensions(linalgOp, rewriter);
  }
};

// Fold chain of static high pad operations.
//
// %0 = tensor.pad %arg2 low[%c0, %c0] high[%c0, %c13] {
//  ^bb0(%arg3: index, %arg4: index):
//    tensor.yield %cst : f32
//  } : tensor<3x3xf32> to tensor<3x16xf32>
//
// %2 = tensor.pad %0 low[%c0, %c0] high[%c3, %c0] {
//  ^bb0(%arg3: index, %arg4: index):
//    tensor.yield %cst : f32
//  } : tensor<3x16xf32> to tensor<6x16xf32>
//
// into
//
// %1 = tensor.pad %arg2 low[%c0, %c0] high[%c3, %c13] {
// ^bb0(%arg3: index, %arg4: index):
//   tensor.yield %cst : f32
// } : tensor<3x3xf32> to tensor<6x16xf32>
struct FoldChainOfStaticPaddings : public OpRewritePattern<tensor::PadOp> {
  using OpRewritePattern<tensor::PadOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(tensor::PadOp consumer,
                                PatternRewriter &rewriter) const override {
    if (!consumer.hasZeroLowPad() || consumer.nofold() ||
        !consumer.getConstantPaddingValue() || consumer.hasZeroHighPad())
      return failure();
    tensor::PadOp producer = consumer.source().getDefiningOp<tensor::PadOp>();
    if (!producer || !producer.hasZeroLowPad() || producer.nofold() ||
        !producer.getConstantPaddingValue() || producer.hasZeroHighPad())
      return failure();
    if (producer.getConstantPaddingValue() !=
        consumer.getConstantPaddingValue())
      return failure();

    Value paddingVal = producer.getConstantPaddingValue();
    tensor::PadOp newPadOp = tensor::createPadHighOp(
        consumer.getResultType(), producer.source(), paddingVal,
        /*nofold*/ false, consumer.getLoc(), rewriter);
    rewriter.replaceOp(consumer, newPadOp.getResult());
    return success();
  }
};

/*
%5 = linalg.init_tensor [132, 256] : tensor<132x256xf32>
%6 = linalg.fill ins(%cst_0 : f32) outs(%5 : tensor<132x256xf32>) ->
tensor<132x256xf32> %7 = tensor.insert_slice %arg0 into %6[%c0, %c0] [128, 256]
[1, 1] : tensor<128x256xf32> into tensor<132x256xf32> use(%7)

Try to rewrite as:

%5 = linalg.init_tensor [132, 256] : tensor<132x256xf32>
%6 = tensor.insert_slice %arg0 into %5[%c0, %c0] [128, 256] [1, 1] :
tensor<128x256xf32> into tensor<132x256xf32> %7 = scf.for ( i = 128 to 132 ) {
  %8 = tensor.insert %paddingValue into %6[i] : tensor<132x256xf32>
  scf.yield %8 : tensor<132x256xf32>
}
use(%7)

*/
struct GenericHighPadOpPattern : public linalg::GeneralizePadOpPattern {
  GenericHighPadOpPattern(MLIRContext *context, PatternBenefit benefit = 1)
      : linalg::GeneralizePadOpPattern(context, trySimplifyCopy, benefit) {}

  // Try avoiding inserting 'InsertSlideOp' which lowers to another
  // copy operation.
  static LogicalResult trySimplifyCopy(PatternRewriter &rewriter,
                                       tensor::PadOp padOp, Value dest) {
    // We consider only high padding.
    if (!padOp.hasZeroLowPad() || padOp.hasZeroHighPad())
      return failure();

    // Bail out if the pad value is non-constant and the source
    // or result shapes are dynamic.
    RankedTensorType sourceType = padOp.getSourceType();
    RankedTensorType resultType = padOp.getResultType();
    if (!sourceType.hasStaticShape() || !resultType.hasStaticShape())
      return failure();
    auto padValue = padOp.getConstantPaddingValue();
    if (!padValue)
      return failure();

    return failure();
  }
};

void populateTppEnforcePatterns(RewritePatternSet &patterns) {
  patterns.add<PadSIMDDimensionForGemm,
               FoldChainOfStaticPaddings /*, GenericHighPadOpPattern*/>(
      patterns.getContext());
}

struct EnforcePreconditionsToTpp
    : EnforcePreconditionsToTppBase<EnforcePreconditionsToTpp> {
  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    populateTppEnforcePatterns(patterns);
    (void)applyPatternsAndFoldGreedily(getOperation(), std::move(patterns));
    return;
  }
};

} // end namespace

std::unique_ptr<OperationPass<func::FuncOp>>
mlir::tpp::createTppEnforcePreconditions() {
  return std::make_unique<EnforcePreconditionsToTpp>();
}
