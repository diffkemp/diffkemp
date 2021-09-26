//===---------- EquivalenceSlicer.h - Slicing equal parts of code ----------==//
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
#ifndef DIFFKEMP_EQUIVALENCESLICER_H
#define DIFFKEMP_EQUIVALENCESLICER_H

#include "Config.h"
#include "ControlFlowGraphUtils.h"
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

class EquivalenceSlicer : public CFGSlicer {
    const Config &config;
    CFGSlicer CFL;
    CFGSlicer CFR;
    // Queues for comparing basic blocks of functions and sets for remembering
    // processed basic blocks to avoid infinite loops
    std::queue<BasicBlock *> QL;
    std::queue<BasicBlock *> QR;
    std::set<BasicBlock *> pushed_QL;
    std::set<BasicBlock *> pushed_QR;
    std::pair<const Instruction *, const Instruction *> DifferingInsts;

  public:
    EquivalenceSlicer(const Config &config) : config(config) {}
    /// Removes semantically equal parts of compared functions and uses
    /// DifferentialFunctionComparator to get the first found difference
    void slice(DifferentialFunctionComparator *fComp);
    void setDifferingInstructions(
            std::pair<const Instruction *, const Instruction *>
                    DifferingInstructions);
    void removeInstFromSnMap(DenseMap<const Value *, int> *sn_map,
                             const Instruction *Inst);
    void addSuccesors(BasicBlock *current_BB,
                      std::queue<BasicBlock *> *Q,
                      std::set<BasicBlock *> *analysed_BBs);
    void processPHIs(DifferentialFunctionComparator *DFC);
    void includeAllDependentOperands(CFGSlicer *CF);
    void keepAllInstsFromQueue(DifferentialFunctionComparator *DFC,
                               CFGSlicer *CFG,
                               std::queue<BasicBlock *> *Q);

    bool differingInstsExist(DifferentialFunctionComparator *DFC);
    bool foundDifferingInsts(
            std::pair<const Instruction *, const Instruction *> Insts,
            std::pair<const Instruction *, const Instruction *>
                    DifferingInstructions);
    bool analysedAllInstsInBB(BasicBlock *BB, BasicBlock::const_iterator Inst);
    bool queuesNotEmpty();
    bool sameQueueSizes();

    BasicBlock *getNextBB(std::queue<BasicBlock *> *Q);
};

#endif // DIFFKEMP_EQUIVALENCESLICER_H
