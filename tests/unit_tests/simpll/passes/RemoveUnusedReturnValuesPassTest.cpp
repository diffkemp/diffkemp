//===--------- RemoveUnusedReturnValuesPassTest.cpp - Unit tests -----------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the RemoveUnusedReturnValuesPass pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#if LLVM_VERSION_MAJOR >= 11
#include <llvm/IR/PassManagerImpl.h>
#endif
#include <llvm/Passes/PassBuilder.h>
#include <passes/CalledFunctionsAnalysis.h>
#include <passes/RemoveUnusedReturnValuesPass.h>

/// Creates two functions with a return type different than void (one with a
/// body, one without), then calls each one of them of them twice in the main
/// function - the first time leaving the value unused, the second time using
/// it. A non-call use is then added for each function to the main function.
/// Finally the pass is run on the module and the replacement is checked.
TEST(RemoveUnusedReturnValuesPass, Base) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // The first function is a declaration, the second one is a full function.
    Function *Fun1 =
            Function::Create(FunctionType::get(Type::getInt8Ty(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "fun1",
                             Mod);
    Function *Fun2 =
            Function::Create(FunctionType::get(Type::getInt8Ty(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "fun2",
                             Mod);
    ReturnInst::Create(Ctx,
                       ConstantInt::get(Type::getInt8Ty(Ctx), 0),
                       BasicBlock::Create(Ctx, "", Fun2));

    // Main function from which the other two functions are called.
    Function *Main =
            Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                             GlobalValue::ExternalLinkage,
                             "main",
                             Mod);
    BasicBlock *BB = BasicBlock::Create(Ctx, "", Main);

    // Create one used call, one unused call and one non-call use for each
    // function.
    for (Function *Fun : {Fun1, Fun2}) {
        CallInst::Create(Fun, {}, "", BB);
        auto CI = CallInst::Create(Fun, {}, "", BB);
        CastInst::Create(CastInst::SExt, CI, Type::getInt16Ty(Ctx), "", BB);
        CastInst::Create(CastInst::BitCast,
                         Fun,
                         PointerType::get(Type::getVoidTy(Ctx), 0),
                         "",
                         BB);
    }

    ReturnInst::Create(Ctx, BB);

    // Create an auxiliary second module to contain void-returning variants of
    // the functions.
    Module *Mod2 = new Module("aux", Ctx);
    for (std::string Name : {"fun1", "fun2"})
        Function::Create(FunctionType::get(Type::getVoidTy(Ctx), {}, false),
                         GlobalValue::ExternalLinkage,
                         Name,
                         Mod2);

    // Run the pass and check the results.
    AnalysisManager<Module, Function *> mam(false);
    PassManager<Module,
                AnalysisManager<Module, Function *>,
                Function *,
                Module *>
            mpm;
    mam.registerPass([] { return CalledFunctionsAnalysis(); });
#if LLVM_VERSION_MAJOR >= 8
    mam.registerPass([] { return PassInstrumentationAnalysis(); });
#endif
    mpm.addPass(RemoveUnusedReturnValuesPass{});
    mpm.run(*Mod, mam, Main, Mod2);

    // First check the main function (especially if the calls were replaced
    // correctly).
    auto MainIt = BB->begin();
    for (std::string Name : {"fun1", "fun2"}) {
        // call void @fun1.void()
        // call void @fun2.void()
        ASSERT_NE(MainIt, BB->end());
        auto FunCall1 = dyn_cast<CallInst>(&*MainIt);
        ASSERT_TRUE(FunCall1);
        ASSERT_TRUE(FunCall1->getCalledFunction());
        ASSERT_EQ(FunCall1->getCalledFunction()->getName(), Name + ".void");
        ASSERT_TRUE(FunCall1->getCalledFunction()->getReturnType()->isVoidTy());
        ++MainIt;

        // %1 = call i8 @fun1()
        // %4 = call i8 @fun2()
        ASSERT_NE(MainIt, BB->end());
        auto FunCall2 = dyn_cast<CallInst>(&*MainIt);
        ASSERT_TRUE(FunCall2);
        ASSERT_TRUE(FunCall1->getCalledFunction());
        ASSERT_EQ(FunCall2->getCalledFunction()->getName(), Name);
        ASSERT_EQ(FunCall2->getCalledFunction()->getReturnType(),
                  Type::getInt8Ty(Ctx));
        ++MainIt;

        // %2 = sext i8 %1 to i16
        // %5 = sext i8 %4 to i16
        ASSERT_NE(MainIt, BB->end());
        auto SExt = dyn_cast<CastInst>(&*MainIt);
        ASSERT_TRUE(SExt);
        ASSERT_EQ(SExt->getOpcode(), CastInst::SExt);
        ASSERT_EQ(SExt->getOperand(0), FunCall2);
        ASSERT_EQ(SExt->getDestTy(), Type::getInt16Ty(Ctx));
        ++MainIt;

        // %3 = bitcast i8 ()* @fun1 to void*
        // %6 = bitcast i8 ()* @fun2 to void*
        ASSERT_NE(MainIt, BB->end());
        auto BitCast = dyn_cast<CastInst>(&*MainIt);
        ASSERT_TRUE(BitCast);
        ASSERT_EQ(BitCast->getOpcode(), CastInst::BitCast);
        auto Fun = dyn_cast<Function>(BitCast->getOperand(0));
        ASSERT_TRUE(Fun);
        ASSERT_EQ(Fun->getName(), Name);
        auto PtrTy = PointerType::get(Type::getVoidTy(Ctx), 0);
        ASSERT_EQ(BitCast->getDestTy(), PtrTy);
        ++MainIt;
    }
    ASSERT_TRUE(isa<ReturnInst>(&*MainIt));

    // Now check whether the cloned function and the void-returning variant
    // are correct.
    // Note: the function pointers have to be refreshed, because fun1 and fun2
    // are clones, not the original functions.
    Fun1 = Mod->getFunction("fun1");
    Fun2 = Mod->getFunction("fun2");
    auto Fun1Void = Mod->getFunction("fun1.void");
    auto Fun2Void = Mod->getFunction("fun2.void");

    ASSERT_TRUE(Fun1->isDeclaration());
    ASSERT_TRUE(Fun1Void->isDeclaration());
    ASSERT_FALSE(Fun2->isDeclaration());
    ASSERT_EQ(Fun2->getBasicBlockList().size(), 1);
    ASSERT_EQ(Fun2->begin()->getInstList().size(), 1);
    auto Fun2Ret = dyn_cast<ReturnInst>(&*Fun2->begin()->begin());
    ASSERT_TRUE(Fun2Ret);
}
