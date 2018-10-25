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
#include <llvm/Transforms/Utils/FunctionComparator.h>

using namespace llvm;

/// Extension of FunctionComparator from LLVM designed to compare functions in
/// different modules (original FunctionComparator assumes that both functions
/// are in a single module).

class DifferentialFunctionComparator : public FunctionComparator {
  public:
    DifferentialFunctionComparator(const Function *F1,
                                   const Function *F2,
                                   GlobalNumberState *GN,
                                   const DebugInfo *DI)
            : FunctionComparator(F1, F2, GN), DI(DI),
              LayoutL(F1->getParent()->getDataLayout()),
              LayoutR(F2->getParent()->getDataLayout()) { }

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
    int cmpAllocs(const CallInst* CL, const CallInst* CR,
                 bool &needToCmpOperands) const;

  private:
    const DebugInfo *DI;

    const DataLayout &LayoutL, &LayoutR;
};

#endif //DIFFKEMP_SIMPLL_DIFFERENTIALFUNCTIONCOMPARATOR_H
