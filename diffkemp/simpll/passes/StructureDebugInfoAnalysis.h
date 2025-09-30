//===--- StructureSizeAnalysis.h - extraction of struct type debug info ---===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the StructureDebugInfoAnalysis pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_STRUCTUREDEBUGINFOANALYSIS_H
#define DIFFKEMP_SIMPLL_STRUCTUREDEBUGINFOANALYSIS_H

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/PassManager.h>

#include <set>
#include <unordered_map>

using namespace llvm;

class StructureDebugInfoAnalysis
        : public AnalysisInfoMixin<StructureDebugInfoAnalysis> {
  public:
    using Result = std::unordered_map<std::string, DICompositeType *>;

    /// Collects all debug info nodes (DICompositeType) belonging to structure
    /// types and saves them into a map from the type names to the nodes.
    Result run(Module &Mod,
               AnalysisManager<Module, Function *> &mam,
               Function *Main);

  private:
    friend AnalysisInfoMixin<StructureDebugInfoAnalysis>;
    static AnalysisKey Key;
};

#endif // DIFFKEMP_SIMPLL_STRUCTUREDEBUGINFOANALYSIS_H
