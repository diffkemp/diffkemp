// SimplifyKernelFunctionCallsPass.cpp - Simplifying kernel-specific functions//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the SimplifyKernelFunctionCallsPass.
/// Applied transformations:
/// 1. Remove all arguments of calls to printing functions (printk, _dev_info,
///    dev_warn, dev_err, sprintf).
/// 2. Remove the second argument of all calls to warn_slowpath_null. This
///    argument is a line number.
/// 3. Remove the second argument of inline assemblies containing the
///    __bug_table string. The argument is a line number.
///
//===----------------------------------------------------------------------===//

#include "SimplifyKernelFunctionCallsPass.h"
#include "Utils.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <set>

/// Replace an argument of a call instruction by 0.
/// Checks if the argument is of integer type.
void replaceArgByZero(CallInst *Call, unsigned index) {
    auto OldArg = dyn_cast<ConstantInt>(Call->getArgOperand(index));
    if (OldArg && OldArg->getType()->isIntegerTy()) {
        Call->setArgOperand(index,
                            ConstantInt::get(OldArg->getType(),
                                             APInt(OldArg->getBitWidth(), 0)));
    }
}

/// Replace an argument of a call instruction by 0.
/// Checks if the argument is of integer type.
void replaceArgByNull(CallInst *Call, unsigned index) {
    auto OldArg = Call->getArgOperand(index);
    if (OldArg && OldArg->getType()->isPointerTy()) {
        Call->setArgOperand(index,
                            ConstantPointerNull::get(
                                    dyn_cast<PointerType>(OldArg->getType())));
    }
}

PreservedAnalyses SimplifyKernelFunctionCallsPass::run(
        Function &Fun, FunctionAnalysisManager & /*fam*/) {
    std::vector<Instruction *> toRemove;
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            auto CallInstr = dyn_cast<CallInst>(&Instr);
            if (!CallInstr)
                continue;
            auto CalledFun = CallInstr->getCalledFunction();

            if (!CalledFun) {
                if (auto Asm = dyn_cast<InlineAsm>(getCallee(CallInstr))) {
                    std::string asm_str = Asm->getAsmString();
                    // For inline asm containing __bug_table:
                    //  - replace the first argument byt null (is a file name)
                    //  - replace the second argument by 0 (is a line number)
                    if (asm_str.find("__bug_table") != std::string::npos) {
                        replaceArgByNull(CallInstr, 0);
                        replaceArgByZero(CallInstr, 1);
                    }
                    // For inline asm containing .discard.(un)reachable:
                    //  - replace the first argument by 0 (it is a counter)
                    if (asm_str.find(".discard.reachable") != std::string::npos
                        || asm_str.find(".discard.unreachable")
                                   != std::string::npos) {
                        if (CallInstr->arg_size() > 0)
                            replaceArgByZero(CallInstr, 0);
                    }
                }
                continue;
            }

            // Remove arguments of printing functions
            if (CalledFun->getName() == "printk") {
                // Functions with 1 mandatory argument
                auto OpType = dyn_cast<PointerType>(
                        CallInstr->getOperand(0)->getType());
                // An additional void pointer is added to the operand list
                // so the instruction can be compared as equal even when the
                // other one is one of the functions listed in the other if
                auto newCall =
                        CallInst::Create(CalledFun,
                                         {ConstantPointerNull::get(OpType),
                                          ConstantPointerNull::get(OpType)},
                                         "",
                                         &Instr);
                copyCallInstProperties(CallInstr, newCall);
                CallInstr->replaceAllUsesWith(newCall);
                toRemove.push_back(&Instr);
            } else if (isKernelPrintFunction(CalledFun->getName().str())) {
                // Functions with 2 mandatory arguments
                auto Op0Type = dyn_cast<PointerType>(
                        CallInstr->getOperand(0)->getType());
                auto Op1Type = dyn_cast<PointerType>(
                        CallInstr->getOperand(1)->getType());
                auto newCall =
                        CallInst::Create(CalledFun,
                                         {ConstantPointerNull::get(Op0Type),
                                          ConstantPointerNull::get(Op1Type)},
                                         "",
                                         &Instr);
                copyCallInstProperties(CallInstr, newCall);
                CallInstr->replaceAllUsesWith(newCall);
                toRemove.push_back(&Instr);
            }

            // Replace the second argument of a call to warn_slowpath_null
            // by 0 (it is a line number).
            if (isKernelWarnFunction(CalledFun->getName().str())) {
                replaceArgByNull(CallInstr, 0);
                replaceArgByZero(CallInstr, 1);
            }
        }
    }
    for (auto i : toRemove)
        i->eraseFromParent();
    toRemove.clear();

    return PreservedAnalyses();
}

/// Returns true when the argument is a name of a kernel print function.
bool isKernelPrintFunction(const std::string &name) {
    const static std::set<std::string> List = {
            "_dev_info", "dev_warn", "dev_err", "sprintf", "printk"};

    return List.find(name) != List.end();
}

/// Returns true when the argument is a name of a kernel warning function.
bool isKernelWarnFunction(const std::string &name) {
    const static std::set<std::string> List = {"warn_slowpath_null",
                                               "warn_slowpath_fmt",
                                               "__might_sleep",
                                               "__might_fault",
                                               "acpi_ut_predefined_warning"};

    return List.find(name) != List.end();
}

/// Returns true when the argument is a name of a kernel-specific function.
bool isKernelSimplifiedFunction(const std::string &name) {
    return isKernelPrintFunction(name) || isKernelWarnFunction(name);
}
