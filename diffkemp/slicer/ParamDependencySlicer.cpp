/*
 * Created by Viktor Malik (vmalik@redhat.com)
 *
 * Published under Apache 2.0 license.
 * See LICENSE for details.
 */

#include "ParamDependencySlicer.h"
#include <iostream>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

char ParamDependencySlicer::ID = 0;

bool ParamDependencySlicer::runOnFunction(Function &Fun) {
    // Clear all sets
    DependentInstrs.clear();
    IncludedInstrs.clear();
    AffectedBasicBlocks.clear();
    IncludedBasicBlocks.clear();
    SuccessorsMap.clear();
    uses_param = false;

#ifdef DEBUG
    errs() << "Function: " << Fun.getName().str() << "\n";
#endif

    // First phase - determine which instructions are dependent on the parameter
    for (auto &BB : Fun) {
        if (AffectedBasicBlocks.find(&BB) != AffectedBasicBlocks.end())
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

                if (CallInstr->getCalledFunction() &&
                    isLlreveIntrinsic(*CallInstr->getCalledFunction()))
                    addToIncluded(&Instr);
            }
            if (auto PhiInstr = dyn_cast<PHINode>(&Instr)) {
                // Phi instructions
                for (auto incomingBB : PhiInstr->blocks()) {
                    if (IncludedBasicBlocks.find(incomingBB) !=
                        IncludedBasicBlocks.end()) {
                        auto *Val =
                                PhiInstr->getIncomingValueForBlock(incomingBB);
                        if (auto *ValInstr = dyn_cast<Instruction>(Val)) {
                            if (DependentInstrs.find(ValInstr) ==
                                DependentInstrs.end())
                                continue;
                        }
                        dependent = true;
                    }
                }
            }

            if (dependent) {
                addToDependent(&Instr);
#ifdef DEBUG
                errs() << "Dependent: ";
                Instr.dump();
#endif
                if (auto BranchInstr = dyn_cast<BranchInst>(&Instr)) {
                    auto affectedBBs = affectedBasicBlocks(BranchInstr);
                    addAllInstrs(affectedBBs);
                }
            }
        }
    }

    // Second phase - determine which additional instructions we need to
    // produce a valid CFG
    if (uses_param) {
        // Recursively add all instruction operands to included
        for (auto &Inst : DependentInstrs) {
            if (isa<PHINode>(Inst)) continue;
            addAllOpsToIncluded(Inst);
        }

        auto &ExitNodeAnalysis = getAnalysis<UnifyFunctionExitNodes>();
        auto RetBB = ExitNodeAnalysis.getReturnBlock();
        for (auto &BB : Fun) {
            auto Term = BB.getTerminator();
            if (DependentInstrs.find(Term) != DependentInstrs.end()) continue;
            if (Term->getNumSuccessors() == 0) continue;

            // If there is just one necessary successor, put it into the
            // successors map
            auto includedSucc = includedSuccessors(*Term, RetBB);
            if (includedSucc.size() <= 1) {
                auto NewSucc = includedSucc.empty()
                               ? Term->getSuccessor(0)
                               : *includedSucc.begin();
                SuccessorsMap.emplace(&BB, NewSucc);
            } else {
                addToIncluded(Term);
                addAllOpsToIncluded(Term);
            }
        }
        IncludedInstrs.insert(DependentInstrs.begin(), DependentInstrs.end());

        // If the return instruction is to be removed, we need to mock it
        if (IncludedInstrs.find(RetBB->getTerminator()) == IncludedInstrs.end())
            mockReturn(RetBB, Fun.getReturnType());
        addToIncluded(RetBB->getTerminator());
    }

    // Third phase - remove unneeded instructions and keep the control flow
    if (uses_param) {
        std::vector<Instruction *> toRemove;
        int b = 0;
        for (auto &BB : Fun) {
            // Create new terminator if the current one is to be removed
            // The new terminator will be used to correctly redirect all
            // incoming edges to the following block
            auto TermInst = BB.getTerminator();
            if (IncludedInstrs.find(TermInst) == IncludedInstrs.end()) {
                for (auto TermSucc : TermInst->successors()) {
                    if (TermSucc != SuccessorsMap.find(&BB)->second)
                        TermSucc->removePredecessor(&BB, true);
                }
                TermInst->eraseFromParent();
                IRBuilder<> builder(&BB);
                auto newTerm = builder.CreateBr(
                        SuccessorsMap.find(&BB)->second);
                IncludedInstrs.insert(newTerm);
            }
            // Collect and clear all instruction that can be removed
            for (auto &Inst : BB) {
                if (IncludedInstrs.find(&Inst) == IncludedInstrs.end() &&
                    !Inst.isTerminator()) {
#ifdef DEBUG
                    errs() << "Clearing ";
                    Inst.dump();
#endif
                    Inst.replaceAllUsesWith(UndefValue::get(Inst.getType()));
                    toRemove.push_back(&Inst);
                }
            }
        }

        // Erase instructions
        for (auto &Inst : toRemove) {
            Inst->eraseFromParent();
        }
        // Erase basic blocks
        std::vector<BasicBlock *> BBToRemove;
        for (auto BB_it = Fun.begin(); BB_it != Fun.end();) {
            BasicBlock *BB = &*BB_it++;
            if (IncludedBasicBlocks.find(BB) == IncludedBasicBlocks.end()) {
                if (BB == &BB->getParent()->getEntryBlock()) {
                    // First block is simply deleted, incoming edges represent
                    // loop-back edges that will be deleted as well and hence
                    // can be removed
                    if (canRemoveFirstBlock(BB)) {
                        for (auto Pred : predecessors(BB)) {
                            auto PredTerm = Pred->getTerminator();
                            unsigned succ_cnt = PredTerm->getNumSuccessors();
                            for (unsigned succ = 0; succ < succ_cnt; ++succ) {
                                if (PredTerm->getSuccessor(succ) == BB) {
                                    PredTerm->setSuccessor(succ, Pred);
                                }
                            }
                        }
                        DeleteDeadBlock(BB);
                    }
                } else {
                    // When removing other than a first block, we need to
                    // redirect incoming edges into the successor (a block that
                    // is not included is guaranteed to have one successor).
                    if (canRemoveBlock(BB)) {
                        bool removed =
                                TryToSimplifyUncondBranchFromEmptyBlock(BB);
                        assert(removed);
                    }
                }
            }
        }

#ifdef DEBUG
        errs() << "Function " << Fun.getName().str() << " after cleanup:\n";
        Fun.dump();
        errs() << "\n";
#endif
    }
    return uses_param;
}

std::vector<const BasicBlock *> ParamDependencySlicer::affectedBasicBlocks(
        BranchInst *Branch) {
    // A condition affects those blocks that are reachable through one branch
    // only: hence it is a difference of union and intersection of sets of
    // blocks reachable from individual branches
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
    std::set_difference(reachableUnion.begin(), reachableUnion.end(),
                        reachableIntersection.begin(),
                        reachableIntersection.end(),
                        std::back_inserter(result));
    return result;
}

void ParamDependencySlicer::addAllInstrs(
        const std::vector<const BasicBlock *> BBs) {
    for (auto BB : BBs) {
        AffectedBasicBlocks.insert(BB);
        IncludedBasicBlocks.insert(BB);
        for (auto &Instr : *BB) {
            DependentInstrs.insert(&Instr);
#ifdef DEBUG
            errs() << "Dependent: ";
            Instr.dump();
#endif
        }
    }
}

bool ParamDependencySlicer::checkDependency(const Use *Op) {
    bool result = false;
    if (auto Global = dyn_cast<GlobalVariable>(Op)) {
        if (Global->getName() == ParamName) {
            uses_param = true;
            result = true;
        }
    } else if (auto OpInst = dyn_cast<Instruction>(Op)) {
        if (DependentInstrs.find(OpInst) != DependentInstrs.end()) {
            result = true;
        }
    }
    return result;
}

bool ParamDependencySlicer::addToDependent(const Instruction *Instr) {
    return addToSet(Instr, DependentInstrs);
}

bool ParamDependencySlicer::addToIncluded(const Instruction *Inst) {
    if (DependentInstrs.find(Inst) == DependentInstrs.end())
        return addToSet(Inst, IncludedInstrs);
    else
        return false;
}

bool ParamDependencySlicer::addToSet(const Instruction *Inst,
                                     std::set<const Instruction *> &set) {

    bool added = false;
    auto add = set.insert(Inst);
    if (add.second) {
        added = true;
        IncludedBasicBlocks.insert(Inst->getParent());
    }
    return added;
}

bool ParamDependencySlicer::addAllOpsToIncluded(
        const Instruction *Inst) {
    bool added = false;
    for (auto &Op : Inst->operands()) {
        if (auto OpInst = dyn_cast<Instruction>(&Op)) {
            if (addToIncluded(OpInst)) {
#ifdef DEBUG
                errs() << "Included: ";
                OpInst->dump();
#endif
                added = true;
                addAllOpsToIncluded(OpInst);
            }
        }
    }
    return added;
}

std::set<BasicBlock *> ParamDependencySlicer::includedSuccessors(
        TerminatorInst &Terminator,
        const BasicBlock *ExitBlock) {

    // If block has multiple successors, choose which must be included
    if (Terminator.getNumSuccessors() == 0)
        return {};
    if (Terminator.getNumSuccessors() == 1)
        return {Terminator.getSuccessor(0)};

    auto TrueSucc = Terminator.getSuccessor(0);
    auto FalseSucc = Terminator.getSuccessor(1);

    // Find all included blocks (except exit block) that are reachable through
    // true edge
    auto reachableTrue = reachableBlocksThroughSucc(&Terminator, TrueSucc);
    intersectWith(reachableTrue, IncludedBasicBlocks);
    reachableTrue.erase(ExitBlock);
    // Find all included blocks (except exit block) that are reachable through
    // false edge
    auto reachableFalse = reachableBlocksThroughSucc(&Terminator, FalseSucc);
    intersectWith(reachableFalse, IncludedBasicBlocks);
    reachableFalse.erase(ExitBlock);

    if (reachableTrue != reachableFalse) {
        // If one successor covers all included blocks reachable from the other
        // successor, choose it
        if (std::includes(reachableTrue.begin(), reachableTrue.end(),
                          reachableFalse.begin(), reachableFalse.end()))
            return {TrueSucc};
        if (std::includes(reachableFalse.begin(), reachableFalse.end(),
                          reachableTrue.begin(), reachableTrue.end()))
            return {FalseSucc};
        // If neither of successors covers all blocks reachable by the other,
        // we have to follow both
        return {TrueSucc, FalseSucc};
    }

    // If sets of included blocks reachable through both successors are same
    // and non-empty, we need to decide which successor to keep.
    // One of them might reach other blocks through loop only and than we need
    // to keep the other one
    // TODO this should use loop analysis
    if (!reachableTrue.empty()) {
        if (!isPotentiallyReachable(TrueSucc, Terminator.getParent())) {
            return {TrueSucc};
        } else if (!isPotentiallyReachable(FalseSucc,
                                           Terminator.getParent())) {
            return {FalseSucc};
        } else {
            return {TrueSucc == ExitBlock ? FalseSucc : TrueSucc};
        }
    } else {
        return {TrueSucc == ExitBlock ? FalseSucc : TrueSucc};
    }
}

void ParamDependencySlicer::mockReturn(BasicBlock *ReturnBB, Type *RetType) {
    IRBuilder<> builder(ReturnBB);
    ReturnBB->getTerminator()->eraseFromParent();

    // TODO support more return types
    Value *returnVal = nullptr;
    if (RetType->isIntegerTy())
        returnVal = ConstantInt::get(RetType, 0);

    auto NewReturn = builder.CreateRet(returnVal);
#ifdef DEBUG
    errs() << "New return: ";
    NewReturn->dump();
#endif
}

void ParamDependencySlicer::getAnalysisUsage(AnalysisUsage &usage) const {
    usage.addRequired<UnifyFunctionExitNodes>();
}

bool ParamDependencySlicer::isLlreveIntrinsic(const Function &f) {
    return f.getName() == "__mark" || f.getName() == "__splitmark" ||
           f.getName() == "__criterion";
}

bool ParamDependencySlicer::canRemoveBlock(const BasicBlock *bb) {
    if (bb->getTerminator()->getNumSuccessors() != 1)
        return false;

    // If a removal of bb would result in a situation that there exists a phi
    // node Phi with two different incoming values for the same incoming
    // block (which is a predecessor Pred of bb), we cannot remove bb
    for (const PHINode &Phi : bb->getTerminator()->getSuccessor(0)->phis()) {
        for (unsigned i = 0; i < Phi.getNumIncomingValues(); ++i) {
            if (Phi.getIncomingBlock(i) == bb)
                continue;
            for (auto Pred : predecessors(bb)) {
                if (Phi.getBasicBlockIndex(Pred) < 0)
                    continue;

                auto PredPhiValue = Phi.getIncomingValueForBlock(Pred);
                if (PredPhiValue != Phi.getIncomingValueForBlock(bb))
                    return false;
            }
        }
    }

    return true;
}

bool ParamDependencySlicer::canRemoveFirstBlock(const BasicBlock *bb) {
    // We cannot remove the first block if it has a successor that is included
    // and has incoming edges (since first block cannot have incoming edges)
    for (const auto &Succ : bb->getTerminator()->successors()) {
        if (IncludedBasicBlocks.find(Succ) != IncludedBasicBlocks.end() &&
            pred_begin(Succ) != pred_end(Succ))
            return false;
    }
    return true;
}

std::set<const BasicBlock *> ParamDependencySlicer::reachableBlocks(
        const BasicBlock *Src, Function &Fun) {
    std::set<const BasicBlock *> result;
    for (auto &BB : Fun) {
        if (Src != &BB &&
            isPotentiallyReachable(Src, &BB, nullptr, nullptr, false))
            result.insert(&BB);
    }
    return result;
}

std::set<const BasicBlock *> ParamDependencySlicer::reachableBlocksThroughSucc(
        TerminatorInst *Terminator, BasicBlock *Succ) {
    // Replace terminator by unconditional branch and find all blocks reachable
    // through the new branch (one that omits all other successors)
    auto NewBranch = BranchInst::Create(Succ, Terminator);
    Terminator->removeFromParent();
    auto reachable = reachableBlocks(NewBranch->getParent(),
                                     *Succ->getParent());

    // Restore original terminator
    Terminator->insertBefore(NewBranch);
    NewBranch->eraseFromParent();

    return reachable;
}

void ParamDependencySlicer::intersectWith(
        std::set<const BasicBlock *> &set,
        const std::set<const BasicBlock *> &other) {
    std::set<const BasicBlock *> tmpSet;
    std::set_intersection(set.begin(), set.end(), other.begin(), other.end(),
                          std::inserter(tmpSet, tmpSet.begin()));
    set = std::move(tmpSet);
}

void ParamDependencySlicer::uniteWith(
        std::set<const BasicBlock *> &set,
        const std::set<const BasicBlock *> &other) {
    std::set<const BasicBlock *> tmpSet;
    std::set_union(set.begin(), set.end(), other.begin(), other.end(),
                   std::inserter(tmpSet, tmpSet.begin()));
    set = std::move(tmpSet);
}
