//===---------- MergeNumberedFunctionsPassTest.cpp - Unit tests ------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the MergeNumberedFunctionsPass pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/MergeNumberedFunctionsPass.h>

/// Utility function to create a simple function declaration.
/// Note: the tested pass doesn't compare functions by body, so this is
/// sufficient here.
static Function *createFunction(Module *Mod, std::string Name, Type *ReturnTy) {
    return Function::Create(FunctionType::get(ReturnTy, {}, false),
                            GlobalValue::ExternalLinkage,
                            Name,
                            Mod);
}

/// Creates two functions groups with different names, each function in
/// the group having a different number suffix and some of them having
/// a different type.
/// MergeNumberedFunctionsPass is then run on the module and the results
/// are checked.
TEST(MergeNumberedFunctionsPassTest, Base) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    createFunction(Mod, "group1", Type::getVoidTy(Ctx));
    createFunction(Mod, "group1.1", Type::getVoidTy(Ctx));
    createFunction(Mod, "group1.4", Type::getVoidTy(Ctx));
    createFunction(Mod, "group1.6", Type::getInt8Ty(Ctx));
    createFunction(Mod, "group2", Type::getVoidTy(Ctx));
    createFunction(Mod, "group2.2", Type::getVoidTy(Ctx));
    createFunction(Mod, "group2.3", Type::getInt8Ty(Ctx));
    createFunction(Mod, "group2.9", Type::getVoidTy(Ctx));

    // Run the pass and check the results,
    ModulePassManager mpm;
    ModuleAnalysisManager mam;
    PassBuilder pb;
    pb.registerModuleAnalyses(mam);
    mpm.addPass(MergeNumberedFunctionsPass{});
    mpm.run(*Mod, mam);

    ASSERT_EQ(Mod->getFunctionList().size(), 4);
    ASSERT_TRUE(Mod->getFunction("group1"));
    ASSERT_FALSE(Mod->getFunction("group1.1"));
    ASSERT_FALSE(Mod->getFunction("group1.4"));
    ASSERT_TRUE(Mod->getFunction("group2"));
    ASSERT_FALSE(Mod->getFunction("group2.2"));
    ASSERT_TRUE(Mod->getFunction("group2.3"));
    ASSERT_FALSE(Mod->getFunction("group2.9"));
}
