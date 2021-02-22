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
#include "Config.h"
#include "Utils.h"
#include <llvm/IR/CallSite.h>

/// Compare the module function and the difference pattern from the starting
/// module instruction. Uses the original LLVM FunctionComparator
/// implementation, extended to support comparisons starting from specific
/// instructions. Because of that, code reffering to the comparison of whole
/// functions has also been removed.
/// Note: Parts of this function have been adapted from FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information
/// in the appropriate llvm-lib subdirectory for more details.
int PatternFunctionComparator::compare() {
    // Clear the previous results.
    beginCompare();

    // We do a CFG-ordered walk since the actual ordering of the blocks in the
    // linked list is immaterial. Our walk starts at the containing blocks of
    // the starting instructions, then takes each block from each terminator in
    // order. Instructions from the first pair of blocks that are before the
    // starting instructions will get ignored. As an artifact, this also means
    // that unreachable blocks are ignored.
    SmallVector<const BasicBlock *, 8> FnLBBs, FnRBBs;
    SmallPtrSet<const BasicBlock *, 32> VisitedBBs; // in terms of F1.

    // Get the starting basic blocks.
    FnLBBs.push_back(StartInst->getParent());
    if (IsNewSide) {
        FnRBBs.push_back(ParentPattern->NewStartPosition->getParent());
    } else {
        FnRBBs.push_back(ParentPattern->OldStartPosition->getParent());
    }

    // Run the pattern comparison.
    VisitedBBs.insert(FnLBBs[0]);
    while (!FnLBBs.empty()) {
        const BasicBlock *BBL = FnLBBs.pop_back_val();
        const BasicBlock *BBR = FnRBBs.pop_back_val();

        if (int Res = cmpValues(BBL, BBR))
            return Res;

        if (int Res = cmpBasicBlocks(BBL, BBR))
            return Res;

#if LLVM_VERSION_MAJOR < 8
        const TerminatorInst *TermL = BBL->getTerminator();
        const TerminatorInst *TermR = BBR->getTerminator();
#else
        const Instruction *TermL = BBL->getTerminator();
        const Instruction *TermR = BBR->getTerminator();
#endif
        // Do not descend to successors if the pattern terminating instruction
        // is not a direct part of the pattern.
        if (ParentPattern->MetadataMap[TermR].PatternEnd) {
            continue;
        }

        // Queue all successor basic blocks.
        assert(TermL->getNumSuccessors() == TermR->getNumSuccessors());
        for (unsigned i = 0, e = TermL->getNumSuccessors(); i != e; ++i) {

            if (!VisitedBBs.insert(TermL->getSuccessor(i)).second)
                continue;

            FnLBBs.push_back(TermL->getSuccessor(i));
            FnRBBs.push_back(TermR->getSuccessor(i));
        }
    }

    return 0;
}

/// Set the starting module instruction.
void PatternFunctionComparator::setStartInstruction(
        const Instruction *StartModInst) {
    StartInst = StartModInst;
}

/// Clear all result structures to prepare for a new comparison.
void PatternFunctionComparator::beginCompare() {
    FunctionComparator::beginCompare();
    InstMatchMap.clear();
}

/// Compare a module function instruction with a pattern instruction along
/// with their operands.
/// Note: Parts of this function have been adapted from FunctionComparator.
/// Therefore, LLVM licensing also applies here. See the LICENSE information
/// in the appropriate llvm-lib subdirectory for more details.
int PatternFunctionComparator::cmpOperationsWithOperands(
        const Instruction *L, const Instruction *R) const {
    bool needToCmpOperands = true;
    if (int Res = cmpOperations(&*L, &*R, needToCmpOperands))
        return Res;
    if (needToCmpOperands) {
        assert(L->getNumOperands() == R->getNumOperands());

        for (unsigned i = 0, e = L->getNumOperands(); i != e; ++i) {
            Value *OpL = L->getOperand(i);
            Value *OpR = R->getOperand(i);

            if (int Res = cmpValues(OpL, OpR))
                return Res;
            assert(cmpTypes(OpL->getType(), OpR->getType()) == 0);
        }
    }
    // Map the instructions to each other.
    InstMatchMap[R] = L;
    return 0;
}

/// Compare a module function basic block with a pattern basic block.
int PatternFunctionComparator::cmpBasicBlocks(const BasicBlock *BBL,
                                              const BasicBlock *BBR) const {
    BasicBlock::const_iterator InstL = BBL->begin(), InstLE = BBL->end();
    BasicBlock::const_iterator InstR = BBR->begin(), InstRE = BBR->end();

    // When comparing the first pair, jump to start instructions.
    if (InstMatchMap.empty()) {
        jumpToStartInst(InstL, StartInst);
        jumpToStartInst(InstR,
                        IsNewSide ? ParentPattern->NewStartPosition
                                  : ParentPattern->OldStartPosition);
    }

    while (InstL != InstLE && InstR != InstRE) {
        // Check whether the compared pattern ends at this instruction.
        if (ParentPattern->MetadataMap[&*InstR].PatternEnd) {
            return 0;
        }

        // Compare current instructions with operands.
        if (int Res = cmpOperationsWithOperands(&*InstL, &*InstR)) {
            return Res;
        }

        ++InstL;
        ++InstR;
    }

    if (InstL != InstLE && InstR == InstRE)
        return 1;
    if (InstL == InstLE && InstR != InstRE)
        return -1;

    return 0;
}

/// Compare global values by their names, because their indexes are not
/// expected to be the same.
int PatternFunctionComparator::cmpGlobalValues(GlobalValue *L,
                                               GlobalValue *R) const {
    if (L->hasName() && R->hasName()) {
        // Both values are named, compare them by names
        auto NameL = L->getName();
        auto NameR = R->getName();

        // Remove number suffixes
        if (hasSuffix(NameL))
            NameL = NameL.substr(0, NameL.find_last_of("."));
        if (hasSuffix(NameR))
            NameR = NameR.substr(0, NameR.find_last_of("."));

        if (NameL == NameR)
            return 0;
        return 1;
    }
    return L != R;
}

/// Position the basic block instruction iterator forward to the given
/// starting instruction.
void PatternFunctionComparator::jumpToStartInst(
        BasicBlock::const_iterator &BBIterator,
        const Instruction *Start) const {
    while (&*BBIterator != Start) {
        ++BBIterator;
    }
}
