//===- DifferentialFunctionComparator.h - Comparing functions for equality ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the DifferentialFunctionComparator
/// class that extends the FunctionComparator from the LLVM infrastructure for
/// the specific purposes of SimpLL.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_DIFFERENTIALFUNCTIONCOMPARATOR_H
#define DIFFKEMP_SIMPLL_DIFFERENTIALFUNCTIONCOMPARATOR_H

#include "DebugInfo.h"
#include "ModuleComparator.h"
#include <llvm/Transforms/Utils/FunctionComparator.h>

using namespace llvm;

/// Extension of FunctionComparator from LLVM designed to compare functions in
/// different modules (original FunctionComparator assumes that both functions
/// are in a single module).

class DifferentialFunctionComparator : public FunctionComparator {
  public:
    DifferentialFunctionComparator(const Function *F1,
                                   const Function *F2,
                                   bool controlFlowOnly,
                                   GlobalNumberState *GN,
                                   const DebugInfo *DI,
                                   ModuleComparator *MC)
            : FunctionComparator(F1, F2, GN), DI(DI),
              controlFlowOnly(controlFlowOnly),
              LayoutL(F1->getParent()->getDataLayout()),
              LayoutR(F2->getParent()->getDataLayout()),
              ModComparator(MC) {}

  protected:
    /// Specific comparison of GEP instructions/operators.
    /// Handles situation when there is an offset between matching GEP indices
    /// in the compared modules (when a struct type has different fields).
    int cmpGEPs(const GEPOperator *GEPL,
                const GEPOperator *GEPR) const override;
    /// Specific comparison of attribute lists.
    /// Attributes that do not affect the semantics of functions are removed.
    int cmpAttrs(const AttributeList L, const AttributeList R) const override;
    /// Compare CallInsts using cmpAllocs.
    int cmpOperations(const Instruction *L, const Instruction *R,
                      bool &needToCmpOperands) const override;
    /// Handle comparing of memory allocation function in cases where the size
    /// of the composite type is different.
    int cmpAllocs(const CallInst *CL, const CallInst *CR) const;
    /// Compare two function calls where one has an extra argument.
    /// Such calls are compared as equal if they only differ in the last
    /// argument which is 0 or NULL.
    int cmpCallsWithExtraArg(const CallInst *CL, const CallInst *CR) const;
    /// Compares array types with equivalent element types and all integer types
    /// as equal when comparing the control flow only.
    int cmpTypes(Type *L, Type *R) const override;
    /// Do not compare bitwidth when comparing the control flow only.
    int cmpAPInts(const APInt &L, const APInt &R) const override;
    /// Detect cast instructions and ignore them when comparing the control flow
    /// only. (The rest is the same as in LLVM.)
    int cmpBasicBlocks(const BasicBlock *BBL, const BasicBlock *BBR)
        const override;
    /// Specific comparing of values. Handles values generated from macros
    /// whose value changed and values where at least one of them is a cast.
    int cmpValues(const Value *L, const Value *R) const override;
    /// Specific comarison of memcpy instructions
    int cmpMemset(const CallInst *CL, const CallInst *CR) const;
    /// Comparing of a structure size with a constant
    int cmpStructTypeSizeWithConstant(StructType *Type,
                                      const Value *Const) const;

  private:
    /// Finds macro differences at the locations of the instructions L and R and
    /// adds them to the list in ModuleComparator.
    /// This is used when a difference is suspected to be in a macro in order to
    /// include that difference into the diffkemp output.
    void findMacroDifferences(const Instruction *L, const Instruction *R) const;

    const DebugInfo *DI;
    bool controlFlowOnly;

    const DataLayout &LayoutL, &LayoutR;

    ModuleComparator *ModComparator;
};

#endif //DIFFKEMP_SIMPLL_DIFFERENTIALFUNCTIONCOMPARATOR_H
