//===------ StructureSizeAnalysis.h - analysis of struct type sizes -------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the StructureSizeAnalysis pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_STRUCTURESIZEANALYSIS_H
#define DIFFKEMP_SIMPLL_STRUCTURESIZEANALYSIS_H

#include <llvm/IR/PassManager.h>
#include <set>
#include <map>

using namespace llvm;

class StructureSizeAnalysis
        : public AnalysisInfoMixin<StructureSizeAnalysis> {
  public:
    using Result = std::map<int, std::set<std::string>>;

    /// Collects all structure type sizes and creates a map from the size
    /// numbers to a set of structure names.
    Result run(Module &Mod,
               AnalysisManager<Module, Function *> &mam,
               Function *Main);

  private:
    friend AnalysisInfoMixin<StructureSizeAnalysis>;
    static AnalysisKey Key;
};

#endif //DIFFKEMP_SIMPLL_STRUCTURESIZEANALYSIS_H
