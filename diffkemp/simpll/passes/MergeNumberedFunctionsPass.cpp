//== MergeNumberedFunctionsPass.h - Merge functions with different numbers ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the MergeNumberedFunctionsPass pass.
///
//===----------------------------------------------------------------------===//

#include "MergeNumberedFunctionsPass.h"

#include "CalledFunctionsAnalysis.h"
#include "FunctionAbstractionsGenerator.h"
#include "Utils.h"
#include <unordered_map>

PreservedAnalyses
        MergeNumberedFunctionsPass::run(Module &Mod,
                                        AnalysisManager<Module> & /*mam*/) {
    // All functions with the same number are grouped together into a vector,
    // its index is the name of the functions without the suffix.
    std::unordered_map<std::string, std::vector<Function *>> GroupingMap;

    // Go over all called functions and put them into the map. Functions without
    // a suffix are included, too, because there may be variants that have it.
    for (Function &Fun : Mod) {
        if (isSimpllAbstraction(&Fun) || Fun.getName().startswith("llvm."))
            // Do not merge LLVM intrinsics and SimpLL abstractions.
            continue;
        std::string originalName = Fun.getName().str();
        std::string strippedName = hasSuffix(originalName)
                                           ? dropSuffix(originalName)
                                           : originalName;
        GroupingMap[strippedName].push_back(&Fun);
    }

    // Go over the map and process the functions.
    for (auto It : GroupingMap) {
        std::vector<Function *> &Vec = It.second;
        if (Vec.size() < 2)
            // There is nothing to be merged.
            continue;

        // Merge the functions.
        auto FirstF = Vec.front();
        for (auto F = Vec.begin() + 1; F != Vec.end(); F++) {
            if (FirstF->getFunctionType() != (*F)->getFunctionType())
                continue;
            (*F)->replaceAllUsesWith(FirstF);
            (*F)->eraseFromParent();
        }

        // If FirstF has a suffix, drop it to ensure that the suffix won't end
        // up anywhere in the output of SimpLL.
        auto name = FirstF->getName().str();
        name = hasSuffix(name) ? dropSuffix(name) : name;
        FirstF->setName(name);
    }

    return PreservedAnalyses();
}
