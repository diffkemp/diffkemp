//===---------- ReduceFunctionMetadataPassTest.cpp - Unit tests ------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the ReduceFunctionMetadataPass pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/ReduceFunctionMetadataPass.h>

/// Create a function with additional metadata, run the pass and check whether
/// it has been removed.
TEST(ReduceFunctionMetadataPassTest, Base) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    Function *Fun =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::InternalLinkage,
                             "test",
                             Mod);
    Fun->setSection("customsec");

    // Run the pass and check the results,
    FunctionPassManager fpm(false);
    FunctionAnalysisManager fam(false);
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(ReduceFunctionMetadataPass{});
    fpm.run(*Fun, fam);

    ASSERT_EQ(Fun->getLinkage(), GlobalValue::ExternalLinkage);
    ASSERT_FALSE(Fun->hasSection());
}
