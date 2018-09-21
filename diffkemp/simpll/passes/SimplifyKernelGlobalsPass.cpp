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
///
//===----------------------------------------------------------------------===//

#include "SimplifyKernelGlobalsPass.h"
#include "Utils.h"

/// Check if a global variable with the given name is supported to be merged in
/// case multiple instances of the same variable with different suffices exist.
bool canMergeGlobalWithName(const std::string Name) {
    return Name.find(".__warned") != std::string::npos
            || Name.find(".descriptor") != std::string::npos;
}

PreservedAnalyses SimplifyKernelGlobalsPass::run(Module &Mod,
                                                 ModuleAnalysisManager &mam) {
    for (auto &Glob : Mod.globals()) {
        std::string Name = Glob.getName();
        if (canMergeGlobalWithName(Name) && hasSuffix(Name)) {
            std::string OrigName = dropSuffix(Name);
            auto *GlobalOrig = Mod.getNamedGlobal(OrigName);
            if (GlobalOrig)
                Glob.replaceAllUsesWith(GlobalOrig);
            else
                Glob.setName(OrigName);
        }
    }
    return PreservedAnalyses();
}
