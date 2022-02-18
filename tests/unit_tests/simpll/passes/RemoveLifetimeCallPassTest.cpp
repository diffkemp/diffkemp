//===---------- RemoveLifetimeCallPass.cpp - Unit tests ------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the RemoveLifetimeCallPass pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/RemoveLifetimeCallsPass.h>

/// Create a function with LLVM lifetime function calls, run the pass on them
/// and check if it was removed.
TEST(RemoveLifetimeCallPassTest, Base) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    Function *Fun =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "test",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Fun);
    auto Ptr = CastInst::Create(CastInst::IntToPtr,
                                ConstantInt::get(Type::getInt64Ty(Ctx), 0),
                                PointerType::get(Type::getInt8Ty(Ctx), 0),
                                "",
                                BB);
    IRBuilder<> Builder(BB);
    Builder.CreateLifetimeStart(Ptr,
                                ConstantInt::get(Type::getInt64Ty(Ctx), 1));
    Builder.CreateLifetimeEnd(Ptr, ConstantInt::get(Type::getInt64Ty(Ctx), 1));
    Builder.CreateRetVoid();

    // Run the pass and check the results,
    ModulePassManager mpm;
    ModuleAnalysisManager mam;
    PassBuilder pb;
    pb.registerModuleAnalyses(mam);
    mpm.addPass(RemoveLifetimeCallsPass{});
    mpm.run(*Mod, mam);

    ASSERT_EQ(BB->getInstList().size(), 2);
    ASSERT_TRUE(isa<IntToPtrInst>(&*BB->begin()));
    ASSERT_TRUE(isa<ReturnInst>(&*BB->begin()->getNextNode()));
}
