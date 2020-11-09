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
/// tailored to difference pattern comparison.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNFUNCTIONCOMPARATOR_H
#define DIFFKEMP_SIMPLL_PATTERNFUNCTIONCOMPARATOR_H

#include "FunctionComparator.h"
#include "PatternSet.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Extension of LLVM FunctionComparator which compares a difference pattern
/// against its corresponding module function. Compared functions are expected
/// to lie in different modules.
class PatternFunctionComparator : protected FunctionComparator {
  public:
    PatternFunctionComparator(const Function *ModFun,
                              const Function *PatFun,
                              const PatternSet *Patterns)
            : FunctionComparator(ModFun, PatFun, nullptr), Patterns(Patterns){};

    /// Compare the module function and the difference pattern starting from the
    /// given module instruction.
    int compareFromInst(const Instruction *ModInst);

  protected:
    /// Compare a module function instruction with a pattern instruction along
    /// with their operands.
    int cmpOperationsWithOperands(const Instruction *ModInst,
                                  const Instruction *PatInst) const;

  private:
    /// Associated set of difference patterns.
    const PatternSet *Patterns;
};

#endif // DIFFKEMP_SIMPLL_PATTERNFUNCTIONCOMPARATOR_H
