//===- VarDependencySlicer.h - Slicing program w.r.t. value of a variable -===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the VarDependencySlicer pass.
/// The pass slices the program w.r.t. to a value of a global variable.
///
//===----------------------------------------------------------------------===//

#ifndef PROJECT_VARDEPENDENCYSLICER_H
#define PROJECT_VARDEPENDENCYSLICER_H

#include "ControlFlowGraphUtils.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PassManager.h>
#include <set>

using namespace llvm;

/// A pass slicing a program w.r.t. to a value of a global variable.
/// Only the instructions whose value or execution is dependent on the value of
/// the variable are kept, the rest is removed.
/// Also, additional instructions that are needed to produce a valid LLVM module
/// are kept.
class VarDependencySlicer : public PassInfoMixin<VarDependencySlicer>,
                            public CFGSlicer {
  public:
    PreservedAnalyses run(Function &Fun,
                          FunctionAnalysisManager &fam,
                          GlobalVariable *Var);

  private:
    GlobalVariable *Variable = nullptr;
    // Basics blocks whose execution is dependent on the parameter
    std::set<const BasicBlock *> AffectedBasicBlocks = {};

    // Functions for adding to sets
    void addAllInstrs(const std::vector<const BasicBlock *> BBs);

    // Functions for searching sets
    inline bool isAffected(const BasicBlock *BB);

    // Computing affected and included basic blocks
    std::vector<const BasicBlock *> affectedBasicBlocks(BranchInst *Branch);

    bool checkDependency(const Use *Op);
    void uniteWith(std::set<const BasicBlock *> &set,
                   const std::set<const BasicBlock *> &other);
};

#endif // PROJECT_VARDEPENDENCYSLICER_H
