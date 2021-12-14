//===----- StructureSizeAnalysis.cpp - analysis of struct type sizes ------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the StructureSizeAnalysis pass.
///
//===----------------------------------------------------------------------===//

#include "StructureSizeAnalysis.h"
#include "llvm/IR/TypeFinder.h"

AnalysisKey StructureSizeAnalysis::Key;

StructureSizeAnalysis::Result StructureSizeAnalysis::run(
        Module &Mod,
        AnalysisManager<Module, Function *> & /*mam*/,
        Function * /*Main*/) {
    TypeFinder Types;
    Result Res;
    Types.run(Mod, true);

    for (auto *Ty : Types) {
        if (StructType *STy = dyn_cast<StructType>(Ty)) {
            if (!STy->isSized())
                continue;
            uint64_t STySize = Mod.getDataLayout().getTypeAllocSize(STy);
            auto Elem = Res.find(STySize);
            if (Elem != Res.end()) {
                Elem->second.insert(STy->getStructName().str());
            } else {
                std::set<std::string> Set;
                Set.insert(STy->getStructName().str());
                Res[STySize] = Set;
            }
        }
    }

    return Res;
}
