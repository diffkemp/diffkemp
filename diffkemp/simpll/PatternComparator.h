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
/// comparator.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
#define DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H

#include "PatternFunctionComparator.h"
#include "PatternSet.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Representation of an actively matched difference pattern pair.
struct ActivePattern {
    /// Current comparison position for the new part of the pattern.
    mutable const Instruction *NewPosition;
    /// Current comparison position for the old part of the pattern.
    mutable const Instruction *OldPosition;

    ActivePattern(const Pattern *Parent)
            : NewPosition(Parent->NewStartPosition),
              OldPosition(Parent->OldStartPosition) {}
};

/// Compares difference patterns against functions, possibly eliminating reports
/// of prior semantic differences.
class PatternComparator {
  public:
    PatternComparator(const PatternSet *Patterns,
                      const Function *NewFun,
                      const Function *OldFun);

    /// Tries to match the given instruction pair to the starting instructions
    /// of one of the patterns. Returns true if a valid match is found.
    bool matchPatternStart(const Instruction *NewInst,
                           const Instruction *OldInst);

    /// Tries to match the given instruction pair to one of the active patterns.
    /// Returns true if a valid match is found.
    bool matchActivePattern(const Instruction *NewInst,
                            const Instruction *OldInst);

  private:
    /// Parent set of difference patterns.
    const PatternSet *Patterns;
    /// Map of loaded and active difference patterns.
    std::unordered_map<const Pattern *, std::vector<ActivePattern>>
            ActivePatterns;
    /// Map of pattern function comparators associated with the current set of
    /// patterns and the currently compared functions.
    std::unordered_map<const Pattern *,
                       std::pair<std::unique_ptr<PatternFunctionComparator>,
                                 std::unique_ptr<PatternFunctionComparator>>>
            PatFunComparators;
};

#endif // DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
