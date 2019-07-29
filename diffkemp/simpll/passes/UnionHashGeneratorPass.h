//===- UnionHashGeneratorPass.cpp - Renaming union types based on content -===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the UnionHashGeneratorPass pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_UNIONHASHGENERATORPASS_H
#define DIFFKEMP_SIMPLL_UNIONHASHGENERATORPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

/// This pass renames all anonymous union types to names with hashes
/// corresponding to the union type definitions.
class UnionHashGeneratorPass
        : public PassInfoMixin<UnionHashGeneratorPass> {
  public:
    PreservedAnalyses run(
            Module &Mod,
            llvm::AnalysisManager<llvm::Module> &Main);
};

#endif //DIFFKEMP_SIMPLL_UNIONHASHGENERATORPASS_H
