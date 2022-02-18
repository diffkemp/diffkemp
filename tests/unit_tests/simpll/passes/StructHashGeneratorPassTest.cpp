//===----------- StructHashGeneratorPassTest.cpp - Unit tests --------------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains unit tests for the StructHashGeneratorPass pass.
///
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <passes/StructHashGeneratorPass.h>

/// Creates two modules and two structure and union types in each module.
/// The first two are different, the second two are the same (but all of their
/// names are different). The pass is then run on the modules and the
/// equivalence of the equality of names and types is checked.
TEST(StructHashGeneratorPass, Base) {
    LLVMContext Ctx1;
    Module *Mod1 = new Module("1", Ctx1);
    LLVMContext Ctx2;
    Module *Mod2 = new Module("2", Ctx2);

    // Create the structure types - each one with a different number suffix.
    struct {
        Module *Mod;
        StructType *Str[4];
    } StrMod1{.Mod = Mod1}, StrMod2{.Mod = Mod2};
    int StructNum = 0;
    for (auto Cur : {&StrMod1, &StrMod2}) {
        auto &Ctx = Cur->Mod->getContext();
        // All these structure types should get different names by the pass.
        // Note: 0 and 2 are equal, but one of them is an union.
        Cur->Str[0] = StructType::create(
                Ctx, {Type::getInt8Ty(Ctx), Type::getInt16Ty(Ctx)});
        Cur->Str[1] = StructType::create(
                Ctx, {Type::getInt16Ty(Ctx), Type::getInt16Ty(Ctx)});
        Cur->Str[2] = StructType::create(
                Ctx, {Type::getInt8Ty(Ctx), Type::getInt16Ty(Ctx)});
        Cur->Str[3] = StructType::create(
                Ctx, {Type::getInt16Ty(Ctx), Type::getInt8Ty(Ctx)});
        for (int i = 0; i <= 3; i++) {
            Cur->Str[i]->setName(((i < 2) ? "struct.anon." : "union.anon.")
                                 + std::to_string(StructNum++));
            new GlobalVariable(*Cur->Mod,
                               Cur->Str[i],
                               false,
                               GlobalVariable::ExternalLinkage,
                               nullptr);
        }
    }

    // Run the pass and check the results.
    for (auto Mod : {Mod1, Mod2}) {
        ModulePassManager mpm;
        ModuleAnalysisManager mam;
        PassBuilder pb;
        pb.registerModuleAnalyses(mam);
        mpm.addPass(StructHashGeneratorPass{});
        mpm.run(*Mod, mam);
    }

    // Structures with the same index should have the same name, structures
    // with a different index should have a different name.
    for (int i = 0; i <= 3; i++) {
        for (int j = 0; j <= 3; j++) {
            if (i == j) {
                ASSERT_EQ(StrMod1.Str[i]->getName().str(),
                          StrMod2.Str[j]->getName().str());
            } else {
                ASSERT_NE(StrMod1.Str[i]->getName().str(),
                          StrMod2.Str[j]->getName().str());
            }
        }
    }
}
