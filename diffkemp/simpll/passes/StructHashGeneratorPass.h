//===- StructHashGeneratorPass.h - Renaming struct types based on content -===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the StructHashGeneratorPass pass.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_STRUCTHASHGENERATORPASS_H
#define DIFFKEMP_SIMPLL_STRUCTHASHGENERATORPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

/// This pass renames all anonymous struct and union types to names with hashes
/// corresponding to the their definitions.
/// This does not work on more complicated anonymous structures that use another
/// anonymous structure as a member when there is a name change in the inner
/// one and the outer one gets replaced first.
class StructHashGeneratorPass : public PassInfoMixin<StructHashGeneratorPass> {
  public:
    PreservedAnalyses run(Module &Mod,
                          llvm::AnalysisManager<llvm::Module> &Main);
};

#endif // DIFFKEMP_SIMPLL_STRUCTHASHGENERATORPASS_H
