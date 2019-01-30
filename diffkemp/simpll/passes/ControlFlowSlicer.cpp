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
#include "Utils.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <set>

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

/// Check if a function has indirect call (call to a value).
bool hasIndirectCall(const Function &Fun) {
    for (auto &BB : Fun) {
        for (auto &Inst : BB) {
            if (auto Call = dyn_cast<CallInst>(&Inst)) {
                if (Call->getCalledFunction())
                    continue;
                // For indirect call, check if the called value is ever used
                // (apart from debug instructions and the call itself).
                // If not, do not return true.
                const Value *called = Call->getCalledValue();
                for (auto &Use : called->uses()) {
                    if (Use.getUser() == Call)
                        continue;
                    if (auto UserCall = dyn_cast<CallInst>(Use.getUser())) {
                        if (UserCall->getCalledFunction()
                                && UserCall->getCalledFunction()->isIntrinsic())
                            continue;
                    }
                    return true;
                }
            }
        }
    }
    return false;
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
                if (Function && !hasSideEffect(*Function)) {
                    errs() << "Removing " << Function->getName() << "\n";
                    keep = false;
                }
            }
            else {
                // Instructions having functions as parameters are included only
                // if it is possible that the functions is sometimes called.
                // This at least requires that Fun contains an indirect call.
                for (auto &Op : Instr.operands()) {
                    if (isa<Function>(Op) && hasIndirectCall(Fun)) {
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

    return PreservedAnalyses::none();
}
