//===- XsmmOps.h - Xsmm dialect ops -----------------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef XSMM_STANDALONE_OPS_H
#define XSMM_STANDALONE_OPS_H

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

namespace mlir {
namespace xsmm {
enum class BinaryKind : uint64_t;
class BinaryKindAttr;
enum class TernaryKind : uint64_t;
class TernaryKindAttr;
enum class UnaryKind : uint64_t;
class UnaryKindAttr;
enum class UnaryFlags : uint64_t;
class UnaryFlagsAttr;
enum class BinaryFlags : uint64_t;
class BinaryFlagsAttr;
enum class DataType : uint64_t;
class DataTypeAttr;
} // namespace xsmm
} // namespace mlir

#define GET_OP_CLASSES
#include "Standalone/Dialect/Xsmm/XsmmOps.h.inc"

#endif // XSMM_STANDALONE_OPS_H
