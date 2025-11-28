//===----- ValuePatternComparator.h - Code pattern instruction matcher ----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the value pattern matcher. The
/// value pattern matcher is a comparator extension of the LLVM
/// FunctionComparator tailored to value pattern comparison.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_VALUEPATTERNCOMPARATOR_H
#define DIFFKEMP_SIMPLL_VALUEPATTERNCOMPARATOR_H

#include "CustomPatternSet.h"
#include "FunctionComparator.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Extension of LLVM FunctionComparator which compares a pattern value against
/// a given module value. Compared values are expected to lie in different
/// functions from different modules. Only one side of a value pattern can
/// be compared at once. Therefore, it is expected that the instances of
/// ValuePatternComparator will be used in pairs (one for each side of the
/// compared pattern).
class ValuePatternComparator : protected FunctionComparator {
  public:
    /// The module value that should be compared against the pattern value.
    mutable const Value *ComparedValue;

    ValuePatternComparator(const Function *ModFun,
                           const Function *PatFun,
                           const ValuePattern *ParentPattern)
            : FunctionComparator(ModFun, PatFun, nullptr),
              IsLeftSide(PatFun == ParentPattern->PatternL),
              ParentPattern(ParentPattern) {};

    /// Compare the given module value with the pattern value.
    int compare() override;

  protected:
    /// Compare a module value with a pattern value without using serial
    /// numbers.
    int cmpValues(const Value *ModVal, const Value *PatVal) const override;

  private:
    /// Whether the comparator has been created for the left pattern side.
    const bool IsLeftSide;
    /// The pattern which should be used during comparison.
    const ValuePattern *ParentPattern;
};

#endif // DIFFKEMP_SIMPLL_VALUEPATTERNCOMPARATOR_H
