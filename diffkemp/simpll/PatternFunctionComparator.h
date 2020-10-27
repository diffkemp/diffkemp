//===--- PatternFunctionComparator.h - Code pattern instruction matcher ---===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the LLVM code pattern matcher. The
/// pattern matcher is a comparator extension of the LLVM FunctionComparator
/// tailored to difference pattern instruction comparison.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNFUNCTIONCOMPARATOR_H
#define DIFFKEMP_SIMPLL_PATTERNFUNCTIONCOMPARATOR_H

#include "FunctionComparator.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Extension of LLVM FunctionComparator which compares a difference pattern
/// against its corresponding module function. Compared functions are expected
/// to lie in different modules and the comparison is performed one instruction
/// at a time to allow incremental pattern position changes. Therefore, it is
/// expected that only the cmpOperationsWithOperands method will be used during
/// the comparison.
class PatternFunctionComparator : protected FunctionComparator {
  public:
    PatternFunctionComparator(const Function *ModFun, const Function *PatFun)
            : FunctionComparator(ModFun, PatFun, nullptr) {}

    /// Compare a module and a pattern instruction along with their operands.
    int cmpOperationsWithOperands(const Instruction *ModInst,
                                  const Instruction *PatInst) const;
};

#endif // DIFFKEMP_SIMPLL_PATTERNFUNCTIONCOMPARATOR_H
