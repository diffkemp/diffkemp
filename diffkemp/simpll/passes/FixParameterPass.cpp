//===------ FixParameterPass.cpp - Replacing function parameters -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the FixParameterPass that replaces
// all uses of a specified function parameter with a fixed constant integer
// value throughout the function body.
///
//===----------------------------------------------------------------------===//

#include "FixParameterPass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>

PreservedAnalyses FixParameterPass::run(Function &Fun,
                                        FunctionAnalysisManager & /*fam*/) {
    if (ParamIndex < 0 || static_cast<unsigned>(ParamIndex) >= Fun.arg_size())
        return PreservedAnalyses::all();

    Argument *Param = Fun.getArg(ParamIndex);

    if (!Param || !Param->getType()->isIntegerTy())
        return PreservedAnalyses::all();

    Constant *FixedConstant = ConstantInt::get(
            Param->getType(),
            APInt(Param->getType()->getIntegerBitWidth(), FixedValue));

    Param->replaceAllUsesWith(FixedConstant);

    return PreservedAnalyses();
}
