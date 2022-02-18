//===------------- VarDependencySlicerTest.cpp - Unit tests ----------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the VarDependencySlicerTest pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/VarDependencySlicer.h>

/// Creates a function that takes an argument, performs some arithmetic on it,
/// then changes the value of a global variable independently on the argument
/// and returns the result of the computation with the argument.
/// The slicer should remove the arithmetic and change the function to void,
/// leaving only the change in the global variable.
TEST(VarDependencySlicerTest, Base) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create the function and global variable for the slicing test.
    Function *Fun = Function::Create(FunctionType::get(Type::getVoidTy(Ctx),
                                                       {Type::getInt8Ty(Ctx)},
                                                       false),
                                     GlobalValue::ExternalLinkage,
                                     "fun",
                                     Mod);
    GlobalVariable *GVar =
            new GlobalVariable(*Mod,
                               Type::getInt8Ty(Ctx),
                               false,
                               GlobalVariable::ExternalLinkage,
                               ConstantInt::get(Type::getInt8Ty(Ctx), 0),
                               "glob");
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Fun);
    IRBuilder<> Builder(BB);

    // Add 5 to the argument and increment the value of the global variable.
    auto ParamAdd =
            Builder.CreateBinOp(Instruction::BinaryOps::Add,
                                &*Fun->arg_begin(),
                                ConstantInt::get(Type::getInt8Ty(Ctx), 5));
    auto GVarLoad = Builder.CreateLoad(Type::getInt8Ty(Ctx), GVar);
    auto GVarNeg = Builder.CreateNeg(GVarLoad);
    Builder.CreateStore(GVarNeg, GVar);
    Builder.CreateRet(ParamAdd);

    // Run the pass and check the result (only the instructions operating on the
    // global variable should be left).
    PassManager<Function, FunctionAnalysisManager, GlobalVariable *> fpm;
    FunctionAnalysisManager fam;
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(VarDependencySlicer{});
    fpm.run(*Fun, fam, GVar);

    auto Iter = BB->begin();
    ASSERT_NE(Iter, BB->end());
    auto TestGVarLoad = dyn_cast<LoadInst>(&*Iter);
    ASSERT_TRUE(TestGVarLoad);
    ASSERT_EQ(TestGVarLoad->getPointerOperand(), GVar);
    ++Iter;

    ASSERT_NE(Iter, BB->end());
    auto TestGVarNeg = dyn_cast<BinaryOperator>(&*Iter);
    ASSERT_TRUE(TestGVarNeg);
    ASSERT_EQ(TestGVarNeg->getOpcode(), Instruction::BinaryOps::Sub);
    ASSERT_TRUE(isa<ConstantInt>(TestGVarNeg->getOperand(0)));
    ASSERT_EQ(dyn_cast<ConstantInt>(TestGVarNeg->getOperand(0))->getZExtValue(),
              0);
    ASSERT_EQ(TestGVarNeg->getOperand(1), TestGVarLoad);
    ++Iter;

    ASSERT_NE(Iter, BB->end());
    auto TestGVarStore = dyn_cast<StoreInst>(&*Iter);
    ASSERT_TRUE(TestGVarStore);
    ASSERT_EQ(TestGVarStore->getPointerOperand(), GVar);
    ASSERT_EQ(TestGVarStore->getValueOperand(), TestGVarNeg);
    ++Iter;

    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
    ++Iter;
    ASSERT_EQ(Iter, BB->end());
}
