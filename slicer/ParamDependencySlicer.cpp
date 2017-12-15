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

using namespace llvm;

char ParamDependencySlicer::ID = 0;

bool ParamDependencySlicer::runOnFunction(Function &Fun) {
    // Clear all sets
    DependentInstrs.clear();
    IncludedInstrs.clear();
    AffectedBasicBlocks.clear();
    IncludedBasicBlocks.clear();
    SuccessorsMap.clear();

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
                    auto affectedBBs = affectedBasicBlocks(*BranchInstr, Fun);
                    addAllInstrs(affectedBBs);
                }
            }
        }
    }

    // Second phase - determine which additional instructions we need to
    // produce a valid CFG
    if (!DependentInstrs.empty()) {
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
    if (!DependentInstrs.empty()) {
        std::vector<Instruction *> toRemove;
        int b = 0;
        for (auto &BB : Fun) {
            // First, solve the terminator instruction
            if (IncludedBasicBlocks.find(&BB) != IncludedBasicBlocks.end()) {
                auto TermInst = BB.getTerminator();
                if (IncludedInstrs.find(TermInst) == IncludedInstrs.end()) {
                    TermInst->eraseFromParent();
                    IRBuilder<> builder(&BB);
                    auto newTerm = builder.CreateBr(
                            SuccessorsMap.find(&BB)->second);
                    IncludedInstrs.insert(newTerm);
                }
            } else {
                // If the block is to be skipped, redirect all incoming edges to
                // successors of the block
                std::vector<BasicBlock *> predecessors;
                for (auto Pred : llvm::predecessors(&BB))
                    predecessors.push_back(Pred);

                for (auto Pred : predecessors) {
                    if (IncludedBasicBlocks.find(Pred) ==
                        IncludedBasicBlocks.end())
                        continue;

                    auto Term = Pred->getTerminator();
                    unsigned numSucc = Term->getNumSuccessors();
                    for (unsigned i = 0; i < numSucc; ++i) {
                        if (Term->getSuccessor(i) == &BB) {
                            Term->setSuccessor(i,
                                               SuccessorsMap.find(&BB)->second);
                        }
                    }
                }

                if (!predecessors.empty()) {
                    // TODO support multiple or none predecessors
                    BB.replaceSuccessorsPhiUsesWith(*predecessors.begin());
                }
            }

            // Collect and clear all instruction that can be removed
            for (auto &Inst : BB) {
                if (IncludedInstrs.find(&Inst) == IncludedInstrs.end()) {
#ifdef DEBUG
                    errs() << "Clearing ";
                    Inst.dump();
#endif
                    for (unsigned i = 0; i < Inst.getNumOperands(); ++i) {
                        clearOperand(Inst, i);
                    }
                    if (auto CallInstr = dyn_cast<CallInst>(
                            &Inst)) {
                        for (unsigned i = 0;
                             i < CallInstr->getNumArgOperands(); ++i) {
                            clearArgOperand(*CallInstr, i);
                        }
                    }
                    toRemove.push_back(&Inst);
                } else if (auto PhiInstr = dyn_cast<PHINode>(
                        &Inst)) {
                    std::vector<BasicBlock *> blocksToRemove;
                    for (auto &incomingBB : PhiInstr->blocks()) {
                        if (IncludedBasicBlocks.find(incomingBB) ==
                            IncludedBasicBlocks.end()) {
                            blocksToRemove.push_back(incomingBB);
                        }
                    }
                    for (auto blockToRemove: blocksToRemove) {
                        PhiInstr->removeIncomingValue(blockToRemove);
                    }
                }
            }

        }

        // Erase instructions
        for (auto &Inst : toRemove) {
            Inst->eraseFromParent();
        }
        // Erase basic blocks
        std::vector<BasicBlock *> BBToRemove;
        for (auto &BB : Fun) {
            if (IncludedBasicBlocks.find(&BB) == IncludedBasicBlocks.end())
                BBToRemove.push_back(&BB);
        }
        for (auto BB : BBToRemove)
            BB->removeFromParent();

#ifdef DEBUG
        std::cerr << "Function " << Fun.getName().str() << " after cleanup:"
                  << std::endl;
        Fun.dump();
        std::cerr << std::endl;
#endif
    }
    return !DependentInstrs.empty();
}

std::vector<const BasicBlock *>
ParamDependencySlicer::affectedBasicBlocks(const BranchInst &Branch,
                                           const Function &Fun) {
    // A condition affects those blocks that are reachable through one branch
    // only
    std::vector<const BasicBlock *> result;
    if (Branch.isConditional()) {
        auto succTrue = Branch.getSuccessor(0);
        auto succFalse = Branch.getSuccessor(1);
        if (succTrue->getParent() != &Fun || succFalse->getParent() != &Fun)
            return result;
        for (auto &otherBB : Fun)
            if (isPotentiallyReachable(succTrue, &otherBB) !=
                isPotentiallyReachable(succFalse, &otherBB))
                result.push_back(&otherBB);
    }
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
        const TerminatorInst &Terminator,
        const BasicBlock *ExitBlock) {

    // If block has multiple successors, choose which must be included
    if (Terminator.getNumSuccessors() == 0)
        return {};
    if (Terminator.getNumSuccessors() == 1)
        return {Terminator.getSuccessor(0)};

    std::set<BasicBlock *> result;
    auto TrueSucc = Terminator.getSuccessor(0);
    auto FalseSucc = Terminator.getSuccessor(1);

    bool bothReachable = false;
    for (auto &InclBB : IncludedBasicBlocks) {
        if (InclBB == ExitBlock) continue;

        bool TrueReach = isPotentiallyReachable(TrueSucc, InclBB);
        bool FalseReach = isPotentiallyReachable(FalseSucc, InclBB);
        if (TrueReach != FalseReach) {
            if (TrueReach && !FalseReach)
                result.insert(TrueSucc);
            else
                result.insert(FalseSucc);
        } else if (TrueReach)
            bothReachable = true;
    }

    // If both are reachable, one of them might be reachable through loop only
    // and we need to keep the other one
    // TODO this should use loop analysis
    if (result.empty() && bothReachable) {
        if (!isPotentiallyReachable(TrueSucc, Terminator.getParent())) {
            result.insert(TrueSucc);
        } else if (!isPotentiallyReachable(FalseSucc,
                                           Terminator.getParent())) {
            result.insert(FalseSucc);
        } else {
            result.insert(TrueSucc);
        }
    }

    return result;
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

void ParamDependencySlicer::clearOperand(
        Instruction &Inst, unsigned index) {
    if (index < Inst.getNumOperands()) {
        auto Op = Inst.getOperand(index);
        if (Op && isa<Instruction>(Op))
            Inst.setOperand(index, nullptr);
    }
}

void ParamDependencySlicer::clearArgOperand(
        CallInst &Inst, unsigned index) {
    if (index < Inst.getNumArgOperands()) {
        auto Op = Inst.getArgOperand(index);
        if (Op && isa<Instruction>(Op))
            Inst.setArgOperand(index, nullptr);
    }
}

void ParamDependencySlicer::getAnalysisUsage(AnalysisUsage &usage) const {
    usage.addRequired<UnifyFunctionExitNodes>();
}

bool ParamDependencySlicer::isLlreveIntrinsic(const Function &f) {
    return f.getName() == "__mark" || f.getName() == "__splitmark" ||
           f.getName() == "__criterion";
}
