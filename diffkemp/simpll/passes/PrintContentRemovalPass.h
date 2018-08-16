//===-- PrintContentRemovalPass.h - Removing arguments of print functions -===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the PrintContentRemovalPass that
/// removes arguments of all printing functions.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PRINTCONTENTREMOVALPASS_H
#define DIFFKEMP_SIMPLL_PRINTCONTENTREMOVALPASS_H

#include <llvm/ADT/StringSet.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

class PrintContentRemovalPass
        : public PassInfoMixin<PrintContentRemovalPass> {
  public:
    PreservedAnalyses run(Function &Fun, FunctionAnalysisManager &fam);
};

#endif //DIFFKEMP_SIMPLL_PRINTCONTENTREMOVALPASS_H