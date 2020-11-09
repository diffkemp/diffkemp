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
/// tailored to difference pattern comparison.
///
//===----------------------------------------------------------------------===//

#include "PatternFunctionComparator.h"

/// Compare the module function and the difference pattern starting from the
/// given module instruction.
int PatternFunctionComparator::compareFromInst(const Instruction *ModInst) {
    // TODO: Do an actual pattern comparison.
    return 1;
}

/// Compare a module function instruction with a pattern instruction along
/// with their operands.
int PatternFunctionComparator::cmpOperationsWithOperands(
        const Instruction *ModInst, const Instruction *PatInst) const {
    // TODO: Do an actual instruction comparison.
    return 1;
}
