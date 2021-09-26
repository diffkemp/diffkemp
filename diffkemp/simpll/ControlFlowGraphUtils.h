//===------- ControlFlowGraphUtils.h - Utility functions for slicing -------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tatiana Malecova, t.malecova@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains declaration of utility functions and sets for slicing a
/// function.
///
//===----------------------------------------------------------------------===//
#ifndef DIFFKEMP_CONTROLFLOWGRAPHUTILS_H
#define DIFFKEMP_CONTROLFLOWGRAPHUTILS_H

#include "Config.h"
#include "DebugInfo.h"
#include "DifferentialFunctionComparator.h"
#include "Result.h"
#include "ResultsCache.h"
#include "SourceCodeUtils.h"
#include "Utils.h"
#include <llvm/IR/Module.h>
#include <queue>
#include <set>

using namespace llvm;

class CFGSlicer {
  public:
    // Instructions directly dependent on the parameter/differing instructions
    std::set<const Instruction *> DependentInstrs = {};
    // Instructions that must be included
    std::set<const Instruction *> IncludedInstrs = {};
    // Basic blocks that must be included
    std::set<const BasicBlock *> IncludedBasicBlocks = {};
    // Function parameters to be included
    std::set<const Argument *> IncludedParams = {};

    // Return block
    BasicBlock *RetBB = nullptr;

    bool hasDependentPHI(BasicBlock *BB, BasicBlock *incomingBB);
    bool isAllocOrLoad(const Instruction *Inst);

    // Functions for slicing and producing valid CFG
    void addAdditionalInsts(Function &Fun);
    void addDebugInfo(Function &Fun);
    void clearFunction(Function &Fun);

    // Functions for adding to sets
    bool addToSet(const Instruction *Inst, std::set<const Instruction *> &set);
    bool addToDependent(const Instruction *Instr);
    bool addToIncluded(const Instruction *Inst);
    bool addAllOpsToIncluded(const Instruction *Inst);
    bool addStoresToIncluded(const Instruction *Alloca, const Instruction *Use);

    // Functions for searching sets
    bool isDependent(const Instruction *Instr);
    bool isIncluded(const Instruction *Instr);
    bool isIncluded(const BasicBlock *BB);
    bool isIncluded(const Argument *Param);

    // Computing affected and included basic blocks
    std::set<BasicBlock *> includedSuccessors(BranchInst &Terminator,
                                              const BasicBlock *ExitBlock);

    bool checkPhiDependency(const PHINode &Phi);

    // Computing reachable blocks
    std::set<const BasicBlock *> reachableBlocks(const BasicBlock *Src,
                                                 Function &Fun);
    std::set<const BasicBlock *>
            reachableBlocksThroughSucc(Instruction *Terminator,
                                       BasicBlock *Succ);

    // Set operations
    void intersectWith(std::set<const BasicBlock *> &set,
                       const std::set<const BasicBlock *> &other);

    bool canRemoveBlock(const BasicBlock *bb);
    bool canRemoveFirstBlock(const BasicBlock *bb);

    bool isIncludedDebugInfo(const Instruction &Inst);

    /// Delete all blocks that are not reachable from the entry block
    /// \param Fun
    void deleteUnreachableBlocks(Function &Fun);

    /// Change return type of the function to void if possible.
    void changeToVoid(Function &Fun);
};

#endif // DIFFKEMP_CONTROLFLOWGRAPHUTILS_H
