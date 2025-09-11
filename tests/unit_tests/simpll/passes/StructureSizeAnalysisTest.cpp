//===------------ StructureSizeAnalysisTest.cpp - Unit tests ---------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the StructureSizeAnalysis pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/PassManagerImpl.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/StructureSizeAnalysis.h>

// Creates two structure types of the same size and one of a different one.
// Then the analysis is run and the generated map is checked.
TEST(StructureSizeAnalysisTest, Base) {
    LLVMContext Ctx;
    Module *Mod = new Module("test", Ctx);

    // Create structure types.
    auto STy1 = StructType::create(
            Ctx, {Type::getInt8Ty(Ctx), Type::getInt16Ty(Ctx)}, "struct.1");
    auto STy2 = StructType::create(
            Ctx, {Type::getInt16Ty(Ctx), Type::getInt8Ty(Ctx)}, "struct.2");
    auto STy3 = StructType::create(
            Ctx, {Type::getInt32Ty(Ctx), Type::getInt16Ty(Ctx)}, "struct.3");

    // Create a global variable for each one of them.
    for (auto STy : {STy1, STy2, STy3}) {
        new GlobalVariable(
                *Mod, STy, false, GlobalVariable::ExternalLinkage, nullptr);
    }

    // Run the analysis and check its result.
    AnalysisManager<Module, Function *> mam;
    mam.registerPass([] { return StructureSizeAnalysis(); });
    mam.registerPass([] { return PassInstrumentationAnalysis(); });
    auto Result = mam.getResult<StructureSizeAnalysis>(*Mod, nullptr);
    // Note: structure sizes are aligned to 32 bits by default.
    StructureSizeAnalysis::Result ExpectedResult{{4, {"struct.1", "struct.2"}},
                                                 {8, {"struct.3"}}};
    ASSERT_EQ(Result, ExpectedResult);
}
