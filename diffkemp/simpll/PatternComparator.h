//===------------- PatternComparator.h - Code pattern finder --------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the LLVM code pattern finder and
/// comparison manager.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
#define DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H

#include "PatternFunctionComparator.h"
#include "PatternSet.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Compares difference patterns against functions, possibly eliminating reports
/// of prior semantic differences.
class PatternComparator {
  public:
    PatternComparator(const PatternSet *Patterns,
                      const Function *NewFun,
                      const Function *OldFun);

    /// Tries to match a difference pattern starting with instructions that may
    /// be matched to the given instruction pair. Returns true if a valid match
    /// is found.
    bool matchPattern(const Instruction *NewInst, const Instruction *OldInst);

  private:
    /// Map of pattern function comparators associated with the current set of
    /// patterns and the currently compared functions.
    std::unordered_map<const Pattern *,
                       std::pair<std::unique_ptr<PatternFunctionComparator>,
                                 std::unique_ptr<PatternFunctionComparator>>>
            PatFunComparators;
};

#endif // DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
