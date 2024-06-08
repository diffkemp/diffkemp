//==-- SimplifyKernelGlobalsPass.cpp - Simplifying kernel-specific globals -==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the SimplifyKernelGlobalsPass.
/// The following transformations are done:
/// 1. For some globals, multiple variables of the same name having different
///    suffices are merged into one.
///    Supported globals are:
///    - containing ".__warned" created by WARN_ON* macros
///    - containing ".descriptor" created by netdev_dbg
/// 2. Same applies for functions.
///    Supported functions are:
///    - __compiletime_assert_<NUMBER>()
/// 3. Remove global variables containing the kernel symbol table.
///
//===----------------------------------------------------------------------===//

#include "SimplifyKernelGlobalsPass.h"
#include "Utils.h"

#include <llvm/IR/Constants.h>

/// Check if a global variable with the given name is supported to be merged in
/// case multiple instances of the same variable with different suffices exist.
bool canMergeGlobalWithName(const std::string Name) {
    return Name.find(".__warned") != std::string::npos
           || Name.find(".descriptor") != std::string::npos;
}

PreservedAnalyses
        SimplifyKernelGlobalsPass::run(Module &Mod,
                                       ModuleAnalysisManager & /*mam*/) {
    std::vector<GlobalVariable *> kSymstoDelete;

    for (auto &Glob : Mod.globals()) {
        // Set kernel symbol to be removed
        if (Glob.hasName() && hasPrefix(Glob.getName(), "__ksym")
            && isa<GlobalVariable>(Glob)) {
            kSymstoDelete.push_back(&Glob);
        }
    }

    // Remove kernel symbols from llvm.used
    if (GlobalVariable *GUsed = Mod.getGlobalVariable("llvm.used")) {
        // Create a new initializer without the kernel symbols
        ConstantArray *Used = dyn_cast<ConstantArray>(GUsed->getInitializer());
        std::vector<Constant *> newValues;

        for (Value *C : Used->operands()) {
            // The element may be a bitcast.
            StringRef Name;
            if (ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
                Name = CE->getOperand(0)->getName();
            } else {
                Name = C->getName();
            }

            // Check whether the element is a kernel symbol
            if (!hasPrefix(Name, "__ksym"))
                newValues.push_back(dyn_cast<Constant>(C));
        }

        if (!newValues.empty()) {
            // Create the new type and initialized
            ArrayType *NewType = ArrayType::get(
                    Used->getType()->getArrayElementType(), newValues.size());
            Constant *Used_New = ConstantArray::get(NewType, newValues);
            // The initializer type has changed, therefore the whole global
            // variable has to be replaced.
            GlobalVariable *GUsed_New = new GlobalVariable(Mod,
                                                           NewType,
                                                           GUsed->isConstant(),
                                                           GUsed->getLinkage(),
                                                           Used_New);
            GUsed->eraseFromParent();
            GUsed_New->setName("llvm.used");
        } else {
            GUsed->eraseFromParent();
        }
    }

    // Remove kernel symbol
    for (GlobalVariable *V : kSymstoDelete) {
        Constant *Initializer = V->getInitializer();
        bool isStruct = isa<ConstantStruct>(Initializer);

        // Remove the global variable itself.
        V->replaceAllUsesWith(Constant::getNullValue(V->getType()));
        V->eraseFromParent();

        // Remove its initializer if it was a struct.
        if (isStruct && Initializer) {
            Initializer->destroyConstant();
        }
    }

    for (auto &Fun : Mod) {
        std::string Name = Fun.getName().str();
        if (Name.find("__compiletime_assert") != std::string::npos
            && Name != "__compiletime_assert") {
            std::string OrigName = "__compiletime_assert";
            auto *FunOrig = Mod.getFunction(OrigName);
            if (FunOrig)
                Fun.replaceAllUsesWith(FunOrig);
            else
                Fun.setName(OrigName);
        }
    }
    return PreservedAnalyses();
}
