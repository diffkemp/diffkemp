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

    /// Individual matched instructions, combined for both all matched patterns.
    InstructionSet AllInstMatches;
    /// Matched pairs of mapped pattern instructions. Instructions from the old
    /// module side are used as keys.
    mutable InstructionMap InstMappings;

    /// Tries to match a difference pattern starting with instructions that may
    /// be matched to the given instruction pair. Returns true if a valid match
    /// is found.
    bool matchPattern(const Instruction *InstL, const Instruction *InstR);

  private:
    /// Pair of corresponding pattern function comparators.
    using PatternFunctionComparatorPair =
            std::pair<std::unique_ptr<PatternFunctionComparator>,
                      std::unique_ptr<PatternFunctionComparator>>;

    /// Map of pattern function comparators associated with the current set of
    /// patterns and the currently compared functions.
    std::unordered_map<const Pattern *, PatternFunctionComparatorPair>
            PatFunComparators;

    /// Create the resulting instruction mapping and add all matched
    /// instructions into the combined instruction set.
    void processPatternMatch(
            const Pattern *Pat,
            const PatternFunctionComparatorPair *PatComparators);
};

#endif // DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
