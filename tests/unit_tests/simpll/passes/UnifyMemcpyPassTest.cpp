//===--------------- UnifyMemcpyPassTest.cpp - Unit tests ------------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the UnifyMemcpyPassTest pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/UnifyMemcpyPass.h>

/// Creates a function with two memcpy intrinsics - one with alignment set to
/// 0, the second with alignment set to 2. The first one should be changed to
/// 1 by the pass.
TEST(UnifyMemcpyPassTest, AlignmentUnification) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create a function with calls to memcpy intrinsics.
    Function *Main =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "main",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Main);
    IRBuilder<> Builder(BB);
    auto SrcAlloca = Builder.CreateAlloca(
            Type::getInt8Ty(Ctx), ConstantInt::get(Type::getInt32Ty(Ctx), 10));
    auto DestAlloca = Builder.CreateAlloca(
            Type::getInt8Ty(Ctx), ConstantInt::get(Type::getInt32Ty(Ctx), 10));
    auto SizeConst = ConstantInt::get(Type::getInt32Ty(Ctx), 5);
#if LLVM_VERSION_MAJOR < 7
    Builder.CreateMemCpy(DestAlloca, SrcAlloca, SizeConst, 0);
    Builder.CreateMemCpy(DestAlloca, SrcAlloca, SizeConst, 2);
#elif LLVM_VERSION_MAJOR < 10
    Builder.CreateMemCpy(DestAlloca, 0, SrcAlloca, 0, SizeConst);
    Builder.CreateMemCpy(DestAlloca, 2, SrcAlloca, 2, SizeConst);
#else
    Builder.CreateMemCpy(
            DestAlloca, MaybeAlign(0), SrcAlloca, MaybeAlign(0), SizeConst);
    Builder.CreateMemCpy(
            DestAlloca, MaybeAlign(2), SrcAlloca, MaybeAlign(2), SizeConst);
#endif
    Builder.CreateRetVoid();

    // Run the pass and check the results,
    FunctionPassManager fpm(false);
    FunctionAnalysisManager fam(false);
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(UnifyMemcpyPass{});
    fpm.run(*Main, fam);

    // %1 = alloca i8, i32 10
    // %2 = alloca i8, i32 10
    auto Iter = BB->begin();
    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<AllocaInst>(&*Iter));
    ++Iter;
    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<AllocaInst>(&*Iter));
    ++Iter;

    // call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 1 %2, i8* align 1 %1, i32
    // 5, i1 false)
    // Note: the exact form varies depending on LLVM version.
    ASSERT_NE(Iter, BB->end());
    auto TestCall1 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(TestCall1);
    auto TestMemcpy1 = TestCall1->getCalledFunction();
    ASSERT_TRUE(TestMemcpy1);
    ASSERT_EQ(TestMemcpy1->getIntrinsicID(), Intrinsic::memcpy);
#if LLVM_VERSION_MAJOR < 7
    ASSERT_EQ(TestCall1->getNumArgOperands(), 5);
#else
    ASSERT_EQ(TestCall1->getNumArgOperands(), 4);
#endif
    ASSERT_EQ(TestCall1->getOperand(0), DestAlloca);
    ASSERT_EQ(TestCall1->getOperand(1), SrcAlloca);
    ASSERT_EQ(TestCall1->getOperand(2), SizeConst);
#if LLVM_VERSION_MAJOR < 7
    // The alignment is the fourth operand.
    ASSERT_TRUE(isa<ConstantInt>(TestCall1->getOperand(3)));
    ASSERT_EQ(dyn_cast<ConstantInt>(TestCall1->getOperand(3))->getZExtValue(),
              1);
#elif LLVM_VERSION_MAJOR < 10
    ASSERT_EQ(TestCall1->getParamAlignment(0), 1);
    ASSERT_EQ(TestCall1->getParamAlignment(1), 1);
#else
    ASSERT_EQ(TestCall1->getParamAlign(0), 1);
    ASSERT_EQ(TestCall1->getParamAlign(1), 1);
#endif
    ++Iter;

    // call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 2 %2, i8* align 2 %1, i32
    // 5, i1 false)
    // Note: the exact form varies depending on LLVM version, the important
    // thing is the alignment should stay 2.
    ASSERT_NE(Iter, BB->end());
    auto TestCall2 = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(TestCall2);
    auto TestMemcpy2 = TestCall2->getCalledFunction();
    ASSERT_TRUE(TestMemcpy2);
    ASSERT_EQ(TestMemcpy2->getIntrinsicID(), Intrinsic::memcpy);
#if LLVM_VERSION_MAJOR < 7
    ASSERT_EQ(TestCall2->getNumArgOperands(), 5);
#else
    ASSERT_EQ(TestCall2->getNumArgOperands(), 4);
#endif
    ASSERT_EQ(TestCall2->getOperand(0), DestAlloca);
    ASSERT_EQ(TestCall2->getOperand(1), SrcAlloca);
    ASSERT_EQ(TestCall2->getOperand(2), SizeConst);
#if LLVM_VERSION_MAJOR < 7
    // The alignment is the fourth operand.
    ASSERT_TRUE(isa<ConstantInt>(TestCall2->getOperand(3)));
    ASSERT_EQ(dyn_cast<ConstantInt>(TestCall2->getOperand(3))->getZExtValue(),
              2);
#elif LLVM_VERSION_MAJOR < 10
    ASSERT_EQ(TestCall2->getParamAlignment(0), 2);
    ASSERT_EQ(TestCall2->getParamAlignment(1), 2);
#else
    ASSERT_EQ(TestCall2->getParamAlign(0), 2);
    ASSERT_EQ(TestCall2->getParamAlign(1), 2);
#endif
    ++Iter;

    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
    ++Iter;
    ASSERT_EQ(Iter, BB->end());
}

// Creates a declaration of the __memcpy function used in the kernel and a test
// function that calls it.
TEST(UnifyMemcpyPassTest, KernelMemcpyToIntrinsic) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create the memcpy function.
    Function *Memcpy =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx),
                                               {Type::getInt8PtrTy(Ctx),
                                                Type::getInt8PtrTy(Ctx),
                                                Type::getInt32Ty(Ctx)},
                                               false),
                             GlobalValue::ExternalLinkage,
                             "__memcpy",
                             Mod);

    // Create the main function with a call to __memcpy.
    Function *Main =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "main",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Main);
    IRBuilder<> Builder(BB);
    auto SrcAlloca = Builder.CreateAlloca(
            Type::getInt8Ty(Ctx), ConstantInt::get(Type::getInt32Ty(Ctx), 10));
    auto DestAlloca = Builder.CreateAlloca(
            Type::getInt8Ty(Ctx), ConstantInt::get(Type::getInt32Ty(Ctx), 10));
    auto SizeConst = ConstantInt::get(Type::getInt32Ty(Ctx), 5);
    Builder.CreateCall(Memcpy, {DestAlloca, SrcAlloca, SizeConst});
    Builder.CreateRetVoid();

    // Run the pass and check the results,
    FunctionPassManager fpm(false);
    FunctionAnalysisManager fam(false);
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(UnifyMemcpyPass{});
    fpm.run(*Main, fam);

    // %1 = alloca i8, i32 10
    // %2 = alloca i8, i32 10
    auto Iter = BB->begin();
    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<AllocaInst>(&*Iter));
    ++Iter;
    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<AllocaInst>(&*Iter));
    ++Iter;

    // call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 0 %2, i8* align 0 %1, i32
    // 5, i1 false)
    // Note: the exact form varies depending on LLVM version.
    ASSERT_NE(Iter, BB->end());
    auto TestCall = dyn_cast<CallInst>(&*Iter);
    ASSERT_TRUE(TestCall);
    auto TestMemcpy = TestCall->getCalledFunction();
    ASSERT_TRUE(TestMemcpy);
    ASSERT_EQ(TestMemcpy->getIntrinsicID(), Intrinsic::memcpy);
#if LLVM_VERSION_MAJOR < 7
    ASSERT_EQ(TestCall->getNumArgOperands(), 5);
#else
    ASSERT_EQ(TestCall->getNumArgOperands(), 4);
#endif
    ASSERT_EQ(TestCall->getOperand(0), DestAlloca);
    ASSERT_EQ(TestCall->getOperand(1), SrcAlloca);
    ASSERT_EQ(TestCall->getOperand(2), SizeConst);
    ++Iter;

    ASSERT_NE(Iter, BB->end());
    ASSERT_TRUE(isa<ReturnInst>(&*Iter));
    ++Iter;
    ASSERT_EQ(Iter, BB->end());
}
