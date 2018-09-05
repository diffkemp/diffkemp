//===------ UnifyMemcpyPass.h - Removing arguments of print functions -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the UnifyMemcpyPass that changes all
/// variants of memcpy function to use the llvm.memcpy intrinsic.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_UNIFYMEMCPYPASS_H
#define DIFFKEMP_SIMPLL_UNIFYMEMCPYPASS_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

class UnifyMemcpyPass : public PassInfoMixin<UnifyMemcpyPass> {
  public:
    PreservedAnalyses run(Function &Fun, FunctionAnalysisManager &fam);
};

#endif //DIFFKEMP_SIMPLL_UNIFYMEMCPYPASS_H
