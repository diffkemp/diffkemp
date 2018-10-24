//====- ControlFlowSlicer.cpp - Slicing out non-control-flow dependencies-====//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the ControlFlowSlicer pass.
/// The pass keeps only branches, function calls and all instructions that these
/// depend on.
///
//===----------------------------------------------------------------------===//

#include "ControlFlowSlicer.h"
#include <llvm/IR/Instructions.h>
#include <set>
#include <llvm/IR/Constants.h>

/// Add all instruction operands of an instruction to the set of dependent
/// instructions. If any operand is added, the function is called recursively.
void addAllOperands(const Instruction &Instr,
                    std::set<const Instruction *> &Dependent) {
    for (const auto &Op : Instr.operands()) {
        if (const auto *OpInstr = dyn_cast<Instruction>(Op)) {
            auto added = Dependent.insert(OpInstr);
            if (added.second) {
                addAllOperands(*OpInstr, Dependent);
            }
        }
    }
}

/// Keep only function calls, branches, instructions having functions as
/// parameters, and all instructions depending on these.
PreservedAnalyses ControlFlowSlicer::run(Function &Fun,
                                         FunctionAnalysisManager &fam) {
    std::set<const Instruction *> Dependent;
    for (const auto &BB : Fun) {
        for (const auto &Instr : BB) {
            bool keep = false;
            if (Instr.isTerminator()) {
                // Terminators
                keep = true;
            }
            else if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                // Call instruction except calls to intrinsics
                keep = true;
                auto Function = CallInstr->getCalledFunction();
                if (Function && Function->isIntrinsic())
                    keep = false;
            }
            else {
                // Instructions having functions as parameters
                for (auto &Op : Instr.operands()) {
                    if (isa<Function>(Op)) {
                        keep = true;
                        break;
                    }
                }
            }

            if (keep) {
                Dependent.insert(&Instr);
                addAllOperands(Instr, Dependent);
            }
        }
    }

    std::vector <Instruction *> ToRemove;
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (Dependent.find(&Instr) == Dependent.end()) {
                Instr.replaceAllUsesWith(UndefValue::get(Instr.getType()));
                ToRemove.push_back(&Instr);
            }
        }
    }
    for (auto &Instr : ToRemove)
        Instr->eraseFromParent();

    if (Fun.getName() == "fcoe_ctlr_vn_disc")
        Fun.dump();

    return PreservedAnalyses::none();
}
