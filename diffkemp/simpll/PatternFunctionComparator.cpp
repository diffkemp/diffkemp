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

/// Save the local match result into the combined result pool. If an instruction
/// matches matches multiple patterns, the first match takes precedence.
void PatternFunctionComparator::saveResult(
        InstructionMap *CombinedInstMatches) const {
    CombinedInstMatches->insert(InstMatches.begin(), InstMatches.end());
}

/// Clear all result structures to prepare for a new comparison.
void PatternFunctionComparator::beginCompare() {
    FunctionComparator::beginCompare();
    InstMatches.clear();
}

/// Compare a module function instruction with a pattern instruction along
/// with their operands.
int PatternFunctionComparator::cmpOperationsWithOperands(
        const Instruction *L, const Instruction *R) const {
    // TODO: Do an actual instruction comparison.
    return 1;
}

/// Compare a module function basic block with a pattern basic block.
int PatternFunctionComparator::cmpBasicBlocks(const BasicBlock *BBL,
                                              const BasicBlock *BBR) const {
    BasicBlock::const_iterator InstL = BBL->begin(), InstLE = BBL->end();
    BasicBlock::const_iterator InstR = BBR->begin(), InstRE = BBR->end();

    // When comparing the first pair, jump to start instructions.
    if (InstMatches.empty()) {
        jumpToStartInst(InstL, StartInst);
        jumpToStartInst(InstR,
                        IsNewSide ? ParentPattern->NewStartPosition
                                  : ParentPattern->OldStartPosition);
    }

    // TODO: Do an actual basic block comparison.
    return 1;
}

/// Compare a module function values with a pattern value.
int PatternFunctionComparator::cmpValues(const Value *L, const Value *R) const {
    // TODO: Do an actual value comparison.
    return 1;
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
