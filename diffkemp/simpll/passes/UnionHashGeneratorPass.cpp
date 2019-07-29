//===-- UnionHashGeneratorPass.h - Renaming union types based on content --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the UnionHashGeneratorPass pass.
///
//===----------------------------------------------------------------------===//

#include "UnionHashGeneratorPass.h"
#include <llvm/ADT/Hashing.h>
#include <llvm/IR/TypeFinder.h>
#include <llvm/Support/raw_ostream.h>

PreservedAnalyses UnionHashGeneratorPass::run (
        Module &Mod,
        llvm::AnalysisManager<llvm::Module> &Main) {
    TypeFinder Types;
    Types.run(Mod, true);

    for (auto *Ty : Types) {
        if (auto STy = dyn_cast<StructType>(Ty)) {
            if (!STy->getStructName().startswith("union.anon"))
                continue;
            std::string TypeName = STy->getStructName().str();
            std::string TypeDump; llvm::raw_string_ostream DumpStrm(TypeDump);
            DumpStrm << *STy; TypeDump = DumpStrm.str();

            // Extract the type declaration without type name
            std::string TypeDecl = TypeDump.substr(TypeDump.find("{"));
            std::string NewTypeName = "union.anon." +
                    std::to_string(hash_value(TypeDecl));

            // Rename the type
            STy->setName(NewTypeName);
        }
    }

    return PreservedAnalyses();
}
