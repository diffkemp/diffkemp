//===--- FieldAccessUtils.h - functions for working with field accesses ---===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementations of functions for working with field
/// access operations.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_FIELDACCESSUTILS_H
#define DIFFKEMP_SIMPLL_FIELDACCESSUTILS_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <vector>

using namespace llvm;

/// Finds the beginning of a field access operation from an arbitrary
/// instruction in it.
const GetElementPtrInst *getFieldAccessStart(const Value *Val);

/// Check if the given instruction is a memory access (i.e. a GEP or a pointer
/// bitcast) to the given pointer. If so, return true and set the size of the
/// offset that the instruction adds to the pointer, otherwise return false.
bool isConstantMemoryAccessToPtr(const Instruction *Inst,
                                 const Value *Ptr,
                                 int &Offset);

/// Returns true if NextInst is a part of the same field access operation as
/// Inst and follows it in the operation.
/// This is similar to isConstantMemoryAccessToPtr with the difference being
/// that value semantic difference of the instructions is not required.
bool isFollowingFieldAccessInstruction(const Instruction *Inst,
                                       const Instruction *NextInst);

/// Extracts source types for all GEPs in a field access operation.
std::vector<Type *> getFieldAccessSourceTypes(const GetElementPtrInst *FA);

#endif // DIFFKEMP_SIMPLL_FIELDACCESSUTILS_H
