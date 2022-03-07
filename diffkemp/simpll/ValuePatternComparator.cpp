//===---- ValuePatternComparator.cpp - Code pattern instruction matcher ---===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the value pattern matcher. The
/// value pattern matcher is a comparator extension of the LLVM
/// FunctionComparator tailored to value pattern comparison.
///
//===----------------------------------------------------------------------===//

#include "ValuePatternComparator.h"
#include "Config.h"

/// Compare the given module value with the pattern value.
int ValuePatternComparator::compare() {
    auto PatternValue =
            IsLeftSide ? ParentPattern->ValueL : ParentPattern->ValueR;

    // Compare load from a global variable by name.
    auto Load = dyn_cast<LoadInst>(ComparedValue);
    if (Load && isa<GlobalVariable>(PatternValue)) {
        return !namesMatch(Load->getOperand(0)->getName(),
                           PatternValue->getName(),
                           IsLeftSide);
    }

    // Compare all other values using the default implementation.
    return cmpValues(ComparedValue, PatternValue);
}

/// Compare a module value with a pattern value without using serial numbers.
/// Note: Parts of this function have been adapted from FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information
/// in the appropriate llvm-lib subdirectory for more details.
int ValuePatternComparator::cmpValues(const Value *ModVal,
                                      const Value *PatVal) const {
    // Catch self-reference case. Right side is the pattern side.
    if (ModVal == FnL) {
        if (PatVal == FnR)
            return 0;
        return -1;
    }
    if (PatVal == FnR) {
        if (ModVal == FnL)
            return 0;
        return 1;
    }

    const Constant *ModConst = dyn_cast<Constant>(ModVal);
    const Constant *PatConst = dyn_cast<Constant>(PatVal);
    if (ModConst && PatConst) {
        if (ModVal == PatVal)
            return 0;
        return cmpConstants(ModConst, PatConst);
    }

    if (ModConst)
        return 1;
    if (PatConst)
        return -1;

    const InlineAsm *ModInlineAsm = dyn_cast<InlineAsm>(ModVal);
    const InlineAsm *PatInlineAsm = dyn_cast<InlineAsm>(PatVal);

    if (ModInlineAsm && PatInlineAsm)
        return cmpInlineAsm(ModInlineAsm, PatInlineAsm);
    if (ModInlineAsm)
        return 1;
    if (PatInlineAsm)
        return -1;

    // Because only a single pair of values gets compared, general values cannot
    // be considered as equal.
    return 1;
}
