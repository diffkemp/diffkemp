//===-- PatternFunctionComparator.cpp - Code pattern instruction matcher --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the LLVM code pattern matcher. The
/// pattern matcher is a comparator extension of the LLVM FunctionComparator
/// tailored to difference pattern instruction comparison. Compared to the LLVM
/// FunctionComparator, the comparison is performed lazily, one instruction at
/// a time.
///
//===----------------------------------------------------------------------===//

#include "PatternFunctionComparator.h"

/// Compare a module and a pattern instruction along with their operands.
int PatternFunctionComparator::cmpOperationsWithOperands(
        const Instruction *ModInst, const Instruction *PatInst) const {
    // TODO: Do an actual comparison.
    return true;
}
