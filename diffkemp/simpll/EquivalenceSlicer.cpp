//===--------- EquivalenceSlicer.cpp - Slicing equal parts of code ---------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tatiana Malecova, t.malecova@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementation of static slicer for removing parts of
/// code that are semantically equal.
///
//===----------------------------------------------------------------------===//

#include "EquivalenceSlicer.h"
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>

void EquivalenceSlicer::slice(DifferentialFunctionComparator *fComp) {
    if (!differingInstsExist(fComp)) {
        return;
    }

    // Synchronisation maps
    DenseMap<const Value *, int> *sn_mapL = fComp->getSnMaps().first;
    DenseMap<const Value *, int> *sn_mapR = fComp->getSnMaps().second;
    // Get entry blocks
    QL.push(&(config.FirstFun->getEntryBlock()));
    QR.push(&(config.SecondFun->getEntryBlock()));

    removeInstFromSnMap(sn_mapL, fComp->DifferingInstructions.first);
    removeInstFromSnMap(sn_mapR, fComp->DifferingInstructions.second);

    while (queuesNotEmpty()) {
        // The slicer considers more differing pairs of instructions, so it will
        // keep slicing until it did not check all basic blocks

        // Back up of differing instruction got from DFC
        setDifferingInstructions(fComp->DifferingInstructions);

        // First phase
        // We are comparing BBs by respecting a control flow, whereas we ignore
        // basic blocks whose only predecessors are differing BBs (in ignored
        // BBs we will try to find next synchronization)
        bool diff = false;
        while (queuesNotEmpty()) {
            bool foundDifference = false;

            auto BBL = getNextBB(&QL);
            auto BBR = getNextBB(&QR);

            auto InstL = BBL->begin();
            auto InstR = BBR->begin();

            while (!analysedAllInstsInBB(BBL, InstL)
                   && !analysedAllInstsInBB(BBR, InstR)) {

                if (fComp->maySkipInstruction(&*InstL)) {
                    InstL++;
                    continue;
                }
                if (fComp->maySkipInstruction(&*InstR)) {
                    InstR++;
                    continue;
                }

                if (isDebugInfo(*InstL)) {
                    InstL++;
                    continue;
                }
                if (isDebugInfo(*InstR)) {
                    InstR++;
                    continue;
                }

                if (!fComp->equal(&*InstL, &*InstR)) {
                    // DFC considers the instructions to be different, this may
                    // be caused by instructions that were not compared yet or
                    // differing instructions were found
                    if (foundDifferingInsts(
                                std::make_pair(
                                        dyn_cast<const Instruction>(&*InstL),
                                        dyn_cast<const Instruction>(&*InstR)),
                                fComp->DifferingInstructions)) {
                        setDifferingInstructions(fComp->DifferingInstructions);
                        // Found differing instructions
                        foundDifference = true;
                        diff = true;
                    } else {
                        if (!fComp->cmpBasicBlocks(BBL, BBR)) {
                            // Basic blocks were compared as equal, so we need
                            // to restore DifferingInstructions to real
                            // differing ones (the pair was changed to last
                            // compared instructions when calling
                            // cmpBasicBlocks)
                            fComp->DifferingInstructions = DifferingInsts;
                            InstL = BBL->getTerminator()->getIterator();
                            InstR = BBR->getTerminator()->getIterator();
                        } else {
                            // New differing instructions are found
                            setDifferingInstructions(
                                    fComp->DifferingInstructions);
                            // Moving iterators to differing instructions
                            foundDifference = true;
                            diff = true;
                            removeInstFromSnMap(
                                    sn_mapL,
                                    fComp->DifferingInstructions.first);
                            removeInstFromSnMap(
                                    sn_mapR,
                                    fComp->DifferingInstructions.second);
                        }
                    }
                }

                if (foundDifference)
                    // We do not need successors of differing BBs
                    break;

                if (InstL->isTerminator() && InstR->isTerminator()) {
                    // It is sufficient to find successors only at the end
                    // of current BBs
                    addSuccesors(BBL, &QL, &pushed_QL);
                    addSuccesors(BBR, &QR, &pushed_QR);
                }

                ++InstL;
                ++InstR;
            }
        }

        if (!diff) {
            break;
        }

        // Second phase
        // Searching for possible next synchronization after found difference
        // in functions
        bool foundSync = false;
        // Temporary sets for actual left instruction
        std::set<BasicBlock *> pushed_QRB = pushed_QR;
        std::set<const Instruction *> IncludedInstrsTemp = CFR.IncludedInstrs;
        std::set<const BasicBlock *> IncludedBasicBlocksTemp =
                CFR.IncludedBasicBlocks;

        // Search for the synchronization starting from differing instructions
        auto InstL = fComp->DifferingInstructions.first->getIterator();
        auto InstR = fComp->DifferingInstructions.second->getIterator();
        int map_size = fComp->getSizeOfMaps();

        auto BBL = const_cast<BasicBlock *>(
                fComp->DifferingInstructions.first->getParent());
        auto BBR = const_cast<BasicBlock *>(InstR->getParent());

        while (!analysedAllInstsInBB(BBL, InstL)) {
            // Iterating through the left BB

            if (fComp->maySkipInstruction(&*InstL)) {
                InstL++;
                continue;
            }
            if (isDebugInfo(*InstL)) {
                InstL++;
                continue;
            }

            // We start from the differing instruction found in second function
            InstR = fComp->DifferingInstructions.second->getIterator();
            BBR = const_cast<BasicBlock *>(InstR->getParent());

            // Temporary sets for potentially unequal instruction - if the
            // synchronization is found, we are keeping the content of these
            // sets
            pushed_QRB = pushed_QR;
            IncludedInstrsTemp = CFR.IncludedInstrs;
            IncludedBasicBlocksTemp = CFR.IncludedBasicBlocks;
            while (!analysedAllInstsInBB(BBR, InstR)) {
                // Iterating trough the right BB
                if (fComp->maySkipInstruction(&*InstR)) {
                    InstR++;
                    continue;
                }

                if (isDebugInfo(*InstR)) {
                    InstR++;
                    continue;
                }

                if (!InstL->isTerminator() && !InstR->isTerminator()) {
                    // Ignoring terminators to avoid false positive
                    // synchronization
                    fComp->valuesMustExist = true;
                    if (!fComp->cmpBasicBlocksFromInstructions(
                                BBL, BBR, *&InstL, *&InstR)) {
                        // Compared BB as equal
                        foundSync = true;
                    } else {
                        // Removing unequal instructions from synchronization
                        // maps
                        fComp->eraseFromMaps(map_size - 1);
                    }
                    fComp->valuesMustExist = false;
                    // Restore differing instructions
                    fComp->DifferingInstructions = DifferingInsts;
                }

                if (foundSync) {
                    break;
                }

                IncludedInstrsTemp.insert(&*InstR);
                IncludedBasicBlocksTemp.insert(BBR);
                if (InstR->isTerminator()) {
                    // Keep finding synchronization
                    addSuccesors(BBR, &QR, &pushed_QRB);

                    if (!QR.empty()) {
                        // Get next right BB
                        BBR = getNextBB(&QR);
                        InstR = BBR->begin();
                        continue;
                    }
                }
                ++InstR;
            }

            if (foundSync) {
                break;
            }

            CFL.addToIncluded(&*InstL);
            CFL.IncludedBasicBlocks.insert(BBL);
            if (InstL->isTerminator()) {
                // Keep finding synchronisation
                addSuccesors(BBL, &QL, &pushed_QL);

                if (!QL.empty()) {
                    // Get next left BB
                    BBL = getNextBB(&QL);
                    InstL = BBL->begin();
                    continue;
                }
            }
            ++InstL;
        }

        CFR.IncludedInstrs = IncludedInstrsTemp;
        CFR.IncludedBasicBlocks = IncludedBasicBlocksTemp;
        pushed_QR = pushed_QRB;
        if (foundSync) {
            if (!sameQueueSizes()) {
                // Instructions left in queue are not synchronized so we
                // need to keep them
                keepAllInstsFromQueue(fComp, &CFL, &QL);
                keepAllInstsFromQueue(fComp, &CFR, &QR);
            }
        }
        addSuccesors(BBL, &QL, &pushed_QL);
        addSuccesors(BBR, &QR, &pushed_QR);
    }

    processPHIs(fComp);

    // To keep valid functions we need to keep all operands of differing
    // instructions
    includeAllDependentOperands(&CFL);
    includeAllDependentOperands(&CFR);

    CFL.addAdditionalInsts(*config.FirstFun);
    CFR.addAdditionalInsts(*config.SecondFun);

    CFL.addDebugInfo(*config.FirstFun);
    CFR.addDebugInfo(*config.SecondFun);

    CFL.clearFunction(*config.FirstFun);
    CFR.clearFunction(*config.SecondFun);
}

void EquivalenceSlicer::setDifferingInstructions(
        std::pair<const Instruction *, const Instruction *>
                DifferingInstructions) {
    DifferingInsts = DifferingInstructions;
}

void EquivalenceSlicer::removeInstFromSnMap(
        DenseMap<const Value *, int> *sn_map, const Instruction *Inst) {
    sn_map->erase(Inst);
}

void EquivalenceSlicer::addSuccesors(BasicBlock *current_BB,
                                     std::queue<BasicBlock *> *Q,
                                     std::set<BasicBlock *> *analysed_BBs) {
    for (auto BB : successors(current_BB)) {
        if (analysed_BBs->find(BB) == analysed_BBs->end()) {
            Q->push(BB);
            analysed_BBs->insert(BB);
        }
    }
}

void EquivalenceSlicer::processPHIs(DifferentialFunctionComparator *DFC) {
    // At the end, we need to compare PHI instructions stored in phisToCompare
    for (auto &PhiPair : DFC->phisToCompare)
        if (DFC->cmpPHIs(PhiPair.first, PhiPair.second)) {
            if (CFL.checkPhiDependency(*PhiPair.first))
                CFL.addToDependent(PhiPair.first);

            if (CFR.checkPhiDependency(*PhiPair.second))
                CFR.addToDependent(PhiPair.second);
        }
}

void EquivalenceSlicer::includeAllDependentOperands(CFGSlicer *CFG) {
    for (auto Inst : CFG->IncludedInstrs) {
        CFG->addAllOpsToIncluded(&*Inst);
    }
}

void EquivalenceSlicer::keepAllInstsFromQueue(
        DifferentialFunctionComparator *DFC,
        CFGSlicer *CFG,
        std::queue<BasicBlock *> *Q) {
    while (!Q->empty()) {
        auto BB = getNextBB(Q);
        for (auto &Inst : BB->getInstList()) {
            if (DFC->maySkipInstruction(&Inst)) {
                continue;
            }
            if (isDebugInfo(*&Inst)) {
                continue;
            }
            CFG->addToIncluded(&Inst);
        }
        CFG->IncludedBasicBlocks.insert(BB);
    }
}

bool EquivalenceSlicer::foundDifferingInsts(
        std::pair<const Instruction *, const Instruction *> Insts,
        std::pair<const Instruction *, const Instruction *>
                DifferingInstructions) {
    return Insts == DifferingInstructions;
}

bool EquivalenceSlicer::analysedAllInstsInBB(BasicBlock *BB,
                                             BasicBlock::const_iterator Inst) {
    return Inst == BB->end();
}

bool EquivalenceSlicer::sameQueueSizes() { return QL.size() == QR.size(); }

bool EquivalenceSlicer::queuesNotEmpty() { return !QL.empty() && !QR.empty(); }

bool EquivalenceSlicer::differingInstsExist(
        DifferentialFunctionComparator *DFC) {
    return DFC->DifferingInstructions.first
           && DFC->DifferingInstructions.second;
}

BasicBlock *EquivalenceSlicer::getNextBB(std::queue<BasicBlock *> *Q) {
    auto BB = Q->front();
    Q->pop();
    return BB;
}
