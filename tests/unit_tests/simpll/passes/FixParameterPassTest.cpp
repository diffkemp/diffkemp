//===------------- FixParameterPassTest.cpp - Unit tests ------------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the FixParameterPass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <memory>
#include <passes/FixParameterPass.h>

#if LLVM_VERSION_MAJOR >= 18
#define GET_INT8_PTR_TYPE llvm::PointerType::getUnqual
#else
#define GET_INT8_PTR_TYPE llvm::Type::getInt8PtrTy
#endif

/// Test that a parameter is replaced with a fixed value.
/// Creates a function like: int add(int a, int b) { return a + b; }
/// After pass: int add(int a, int b) { return 0 + b; } (if a is fixed to 0)
TEST(FixParameterPassTest, FixParameterToZero) {
    LLVMContext Ctx;
    auto Mod = std::make_unique<Module>("test", Ctx);

    Function *Add = Function::Create(
            FunctionType::get(Type::getInt32Ty(Ctx),
                              {Type::getInt32Ty(Ctx), Type::getInt32Ty(Ctx)},
                              false),
            GlobalValue::ExternalLinkage,
            "add",
            Mod.get());

    BasicBlock *BB = BasicBlock::Create(Ctx, "", Add);
    IRBuilder<> Builder(BB);

    auto ParamA = Add->getArg(0);
    auto ParamB = Add->getArg(1);

    auto Sum = Builder.CreateAdd(ParamA, ParamB);
    Builder.CreateRet(Sum);

    FunctionPassManager fpm;
    FunctionAnalysisManager fam;
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(FixParameterPass{0, 0});
    fpm.run(*Add, fam);

    auto Iter = BB->begin();

    ASSERT_NE(Iter, BB->end());
    auto AddInst = dyn_cast<BinaryOperator>(&*Iter);
    ASSERT_TRUE(AddInst);
    ASSERT_TRUE(AddInst->getOpcode() == Instruction::Add);

    auto Op0 = dyn_cast<ConstantInt>(AddInst->getOperand(0));
    auto Op1 = AddInst->getOperand(1);

    ASSERT_TRUE(Op0);
    ASSERT_EQ(Op0->getZExtValue(), 0);
    ASSERT_EQ(Op1, ParamB);

    ++Iter;
    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
}

/// Test that parameter fixation works with different bit widths.
/// Creates a function like: long add(long a, long b) { return a + b; }
TEST(FixParameterPassTest, FixParameterDifferentBitWidth) {
    LLVMContext Ctx;
    auto Mod = std::make_unique<Module>("test", Ctx);

    Function *Add = Function::Create(
            FunctionType::get(Type::getInt64Ty(Ctx),
                              {Type::getInt64Ty(Ctx), Type::getInt64Ty(Ctx)},
                              false),
            GlobalValue::ExternalLinkage,
            "add64",
            Mod.get());

    BasicBlock *BB = BasicBlock::Create(Ctx, "", Add);
    IRBuilder<> Builder(BB);

    auto ParamA = Add->getArg(0);
    auto ParamB = Add->getArg(1);

    auto Sum = Builder.CreateAdd(ParamA, ParamB);
    Builder.CreateRet(Sum);

    FunctionPassManager fpm;
    FunctionAnalysisManager fam;
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(FixParameterPass{0, 42});
    fpm.run(*Add, fam);

    auto Iter = BB->begin();
    ASSERT_NE(Iter, BB->end());
    auto AddInst = dyn_cast<BinaryOperator>(&*Iter);
    ASSERT_TRUE(AddInst);

    auto Op0 = dyn_cast<ConstantInt>(AddInst->getOperand(0));
    ASSERT_TRUE(Op0);
    ASSERT_EQ(Op0->getZExtValue(), 42);
    ASSERT_EQ(Op0->getType(), Type::getInt64Ty(Ctx));
}

/// Test that the pass doesn't affect non-integer parameters.
TEST(FixParameterPassTest, IgnoreNonIntegerParameter) {
    LLVMContext Ctx;
    auto Mod = std::make_unique<Module>("test", Ctx);

    std::vector<Type *> Params = {GET_INT8_PTR_TYPE(Ctx)};
    Function *Test = Function::Create(
            FunctionType::get(Type::getVoidTy(Ctx), Params, false),
            GlobalValue::ExternalLinkage,
            "test",
            Mod.get());

    BasicBlock *BB = BasicBlock::Create(Ctx, "", Test);
    IRBuilder<> Builder(BB);
    Builder.CreateRetVoid();

    auto ParamPtr = Test->getArg(0);
    unsigned UseCountBefore = 0;
    for (auto &Use : ParamPtr->uses()) {
        UseCountBefore++;
    }

    FunctionPassManager fpm;
    FunctionAnalysisManager fam;
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(FixParameterPass{0, 0});
    fpm.run(*Test, fam);

    unsigned UseCountAfter = 0;
    for (auto &Use : ParamPtr->uses()) {
        UseCountAfter++;
    }

    ASSERT_EQ(UseCountBefore, UseCountAfter);
}

/// Test that invalid parameter index is handled gracefully.
TEST(FixParameterPassTest, InvalidParameterIndex) {
    LLVMContext Ctx;
    auto Mod = std::make_unique<Module>("test", Ctx);

    std::vector<Type *> Params = {Type::getInt32Ty(Ctx)};
    Function *Test = Function::Create(
            FunctionType::get(Type::getVoidTy(Ctx), Params, false),
            GlobalValue::ExternalLinkage,
            "test",
            Mod.get());

    BasicBlock *BB = BasicBlock::Create(Ctx, "", Test);
    IRBuilder<> Builder(BB);
    Builder.CreateRetVoid();

    FunctionPassManager fpm;
    FunctionAnalysisManager fam;
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(FixParameterPass{5, 0});
    fpm.run(*Test, fam);

    ASSERT_TRUE(Test->isDeclaration() == false);
}
