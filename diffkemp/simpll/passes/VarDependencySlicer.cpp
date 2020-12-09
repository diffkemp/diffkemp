//=== VarDependencySlicer.cpp - Slicing program w.r.t. value of a variable ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the VarDependencySlicer pass.
/// The pass slices the program w.r.t. to a value of a global variable.
///
//===----------------------------------------------------------------------===//

#include "VarDependencySlicer.h"
#include "DebugInfo.h"
#include <Config.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Operator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>

PreservedAnalyses VarDependencySlicer::run(Function &Fun,
                                           FunctionAnalysisManager &fam,
                                           GlobalVariable *Var) {
    if (Fun.isDeclaration())
        return PreservedAnalyses::all();

    Variable = Var;
    // Clear all sets
    DependentInstrs.clear();
    IncludedInstrs.clear();
    AffectedBasicBlocks.clear();
    IncludedBasicBlocks.clear();
    IncludedParams.clear();

    DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE,
                    dbgs() << "Function: " << Fun.getName().str() << "\n");

    // First phase - determine which instructions are dependent on the parameter
    for (auto &BB : Fun) {
        if (isAffected(&BB))
            continue;
        for (auto &Instr : BB) {
            bool dependent = false;
            for (auto &Op : Instr.operands()) {
                if (checkDependency(&Op))
                    dependent = true;
            }
            if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                // Call instructions
                for (auto &Arg : CallInstr->arg_operands()) {
                    if (checkDependency(&Arg))
                        dependent = true;
                }
            }
            if (auto PhiInstr = dyn_cast<PHINode>(&Instr)) {
                // Phi instructions
                if (checkPhiDependency(*PhiInstr))
                    dependent = true;
            }

            if (dependent) {
                addToDependent(&Instr);
                DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE, {
                    dbgs() << "Dependent: ";
                    Instr.print(dbgs());
                });
                if (auto BranchInstr = dyn_cast<BranchInst>(&Instr)) {
                    auto affectedBBs = affectedBasicBlocks(BranchInstr);
                    addAllInstrs(affectedBBs);
                }
                if (auto StoreInstr = dyn_cast<StoreInst>(&Instr)) {
                    auto Ptr = StoreInstr->getPointerOperand();
                    if (auto PtrInstr = dyn_cast<Instruction>(Ptr)) {
                        addToDependent(PtrInstr);
                    }
                }
            }
        }
    }

    // Second phase - determine which additional instructions we need to
    // produce a valid CFG
    DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE, dbgs() << "Second phase\n");
    // Recursively add all instruction operands to included
    for (auto &Inst : DependentInstrs) {
        if (isa<PHINode>(Inst))
            continue;
        addAllOpsToIncluded(Inst);
    }

    UnifyFunctionExitNodes unifyExitPass;
    unifyExitPass.runOnFunction(Fun);
    RetBB = unifyExitPass.getReturnBlock();

    for (auto &BB : Fun) {
        auto Term = dyn_cast<BranchInst>(BB.getTerminator());
        if (!Term)
            continue;
        if (isDependent(Term))
            continue;
        if (Term->getNumSuccessors() == 0)
            continue;

        // If there is just one necessary successor remove all others
        auto includedSucc = includedSuccessors(*Term, RetBB);
        if (includedSucc.size() <= 1) {
            auto NewSucc = includedSucc.empty() ? Term->getSuccessor(0)
                                                : *includedSucc.begin();

            // Notify successors about removing some branches
            for (auto TermSucc : successors(&BB)) {
                if (TermSucc != NewSucc)
                    TermSucc->removePredecessor(&BB, true);
            }
            // Create and insert new branch
            auto NewTerm = BranchInst::Create(NewSucc, Term);
            Term->eraseFromParent();
            IncludedInstrs.insert(NewTerm);
        } else {
            addToIncluded(Term);
            addAllOpsToIncluded(Term);
        }
    }
    IncludedInstrs.insert(DependentInstrs.begin(), DependentInstrs.end());

    // Add needed instructions coming to Phis to included
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto Phi = dyn_cast<PHINode>(&Instr)) {
                if (!isIncluded(Phi))
                    continue;
                for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
                    if (auto incomingInstr = dyn_cast<Instruction>(
                                Phi->getIncomingValue(i))) {
                        addToIncluded(incomingInstr);
                        addAllOpsToIncluded(incomingInstr);
                    }
                }
            }
        }
    }

    // Third phase - add useful debug info
    for (auto &BB : Fun) {
        for (auto &Inst : BB) {
            if (isIncludedDebugInfo(Inst)) {
                addToIncluded(&Inst);
            }
        }
    }

    // Fourth phase - remove unneeded instructions and keep the control flow
    std::vector<Instruction *> toRemove;
    int b = 0;
    for (auto &BB : Fun) {
        // Collect and clear all instruction that can be removed
        for (auto &Inst : BB) {
            if (!isIncluded(&Inst) && !Inst.isTerminator()) {
                DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE,
                                { dbgs() << "Clearing " << Inst << "\n"; });
                Inst.replaceAllUsesWith(UndefValue::get(Inst.getType()));
                toRemove.push_back(&Inst);
            }
        }
    }

    // Erase instructions
    for (auto &Inst : toRemove) {
        Inst->eraseFromParent();
    }

    // If the return instruction is not included, we can transform the function
    // to return void
    if (RetBB && !RetBB->empty() && !isIncluded(RetBB->getTerminator())
        && !Fun.getReturnType()->isVoidTy()) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE,
                        dbgs() << "Changing return type of " << Fun.getName()
                               << " to void.\n");
        changeToVoid(Fun);
    }

    // Clear BBs except first that have no incoming edges
    for (auto BB_it = ++Fun.begin(); BB_it != Fun.end();) {
        BasicBlock *BB = &*BB_it++;
        if (!isIncluded(BB) && pred_begin(BB) == pred_end(BB)) {
            DeleteDeadBlock(BB);
        }
    }
    // Erase basic blocks
    std::vector<BasicBlock *> BBToRemove;
    for (auto BB_it = ++Fun.begin(); BB_it != Fun.end();) {
        // First, erase all blocks that can be erased, except the entry one
        BasicBlock *BB = &*BB_it++;
        if (!isIncluded(BB)) {
            // When removing other than a first block, we need to
            // redirect incoming edges into the successor (a block that
            // is not included is guaranteed to have one successor).
            if (canRemoveBlock(BB)) {
                bool removed = TryToSimplifyUncondBranchFromEmptyBlock(BB);
                assert(removed || BB->getSingleSuccessor() == BB);
            }
        }
    }
    if (!isIncluded(&Fun.getEntryBlock())
        && canRemoveFirstBlock(&Fun.getEntryBlock())) {
        // Erase entry block if possible
        DeleteDeadBlock(&Fun.getEntryBlock());
    }

    // Remove unreachable BBs
    // @TODO There is a pass in LLVM for this but it fails. It might be fixed
    //       in a newer version of LLVM.
    deleteUnreachableBlocks(Fun);

    DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE, {
        dbgs() << "Function " << Fun.getName().str() << " after cleanup:\n";
        Fun.print(dbgs());
        dbgs() << "\n";
    });
    return PreservedAnalyses::none();
}

/// Check if a value is dependent on the value of the global variable.
/// A value is dependent on a variable if it is the variable itself or if it is
/// a dependent value.
bool VarDependencySlicer::checkDependency(const Use *Op) {
    bool result = false;
    if (dyn_cast<GlobalVariable>(Op) == Variable) {
        result = true;
    } else if (auto OpInst = dyn_cast<Instruction>(Op)) {
        if (isDependent(OpInst)) {
            result = true;
        }
    } else if (auto OpOperator = dyn_cast<Operator>(Op)) {
        for (auto &InnerOp : OpOperator->operands())
            if (checkDependency(&InnerOp))
                result = true;
    }
    return result;
}

/// Calculate the set of basic blocks affected by a conditional branch
/// A condition affects those blocks that are reachable through one branch
/// only: hence it is a difference of union and intersection of sets of
/// blocks reachable from individual branches.
std::vector<const BasicBlock *>
        VarDependencySlicer::affectedBasicBlocks(BranchInst *Branch) {
    std::set<const BasicBlock *> reachableUnion;
    std::set<const BasicBlock *> reachableIntersection;
    bool first = true;
    if (Branch->isConditional()) {
        for (auto Succ : Branch->successors()) {
            auto reachable = reachableBlocksThroughSucc(Branch, Succ);

            // Compute union with blocks reachable from other branches
            uniteWith(reachableUnion, reachable);

            // Compute intersection with blocks reachable from other branches
            if (first) {
                reachableIntersection = std::move(reachable);
                first = false;
            } else {
                intersectWith(reachableIntersection, reachable);
            }
        }
    }
    std::vector<const BasicBlock *> result;
    std::set_difference(reachableUnion.begin(),
                        reachableUnion.end(),
                        reachableIntersection.begin(),
                        reachableIntersection.end(),
                        std::back_inserter(result));
    return result;
}

/// Add all instructions of a basic block to included.
void VarDependencySlicer::addAllInstrs(
        const std::vector<const BasicBlock *> BBs) {
    for (auto BB : BBs) {
        AffectedBasicBlocks.insert(BB);
        IncludedBasicBlocks.insert(BB);
        for (auto &Instr : *BB) {
            DependentInstrs.insert(&Instr);
            DEBUG_WITH_TYPE(DEBUG_SIMPLL_VERBOSE, {
                dbgs() << "Dependent: ";
                Instr.print(dbgs());
            });
        }
    }
}

/// Set union. Result is stored in the first set.
void VarDependencySlicer::uniteWith(std::set<const BasicBlock *> &set,
                                    const std::set<const BasicBlock *> &other) {
    std::set<const BasicBlock *> tmpSet;
    std::set_union(set.begin(),
                   set.end(),
                   other.begin(),
                   other.end(),
                   std::inserter(tmpSet, tmpSet.begin()));
    set = std::move(tmpSet);
}

// Check if a basic block is affected by the value of the global variable.
bool VarDependencySlicer::isAffected(const BasicBlock *BB) {
    return AffectedBasicBlocks.find(BB) != AffectedBasicBlocks.end();
}

/// Recursively calculate the set of all blocks reachable from BB.
/// \param BB Block to search from
/// \param Reachable Set of reachale blocks (new found blocks will be added).
/// \param Visited Set of visited blocks.
void calculateReachableBlocksRecursive(BasicBlock *BB,
                                       std::set<BasicBlock *> &Reachable,
                                       std::set<BasicBlock *> &Visited) {
    if (Visited.find(BB) != Visited.end())
        return;
    Visited.insert(BB);

    for (auto Succ : successors(BB)) {
        Reachable.insert(Succ);
        calculateReachableBlocksRecursive(Succ, Reachable, Visited);
    }
}
