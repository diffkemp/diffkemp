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
/// 1. If there are multiple globals containing "__warned" of a same name, just
///    having different suffices, they are all merged into one. These are
///    created by WARN_ON* macros.
///
//===----------------------------------------------------------------------===//

#include "SimplifyKernelGlobalsPass.h"
#include "Utils.h"

PreservedAnalyses SimplifyKernelGlobalsPass::run(Module &Mod,
                                                 ModuleAnalysisManager &mam) {
    for (auto &Glob : Mod.globals()) {
        std::string Name = Glob.getName();
        if (Name.find(".__warned") != std::string::npos && hasSuffix(Name)) {
            std::string OrigName = dropSuffix(Name);
            auto *GlobalOrig = Mod.getNamedGlobal(OrigName);
            if (!GlobalOrig) {
                Mod.getOrInsertGlobal(OrigName, Glob.getType());
                GlobalOrig = Mod.getNamedGlobal(OrigName);
                GlobalOrig->setLinkage(Glob.getLinkage());
                GlobalOrig->setSection(Glob.getSection());
                GlobalOrig->setAlignment(Glob.getAlignment());
                GlobalOrig->setInitializer(GlobalOrig->getInitializer());
            }

            Glob.replaceAllUsesWith(GlobalOrig);
        }
    }
    return PreservedAnalyses();
}
