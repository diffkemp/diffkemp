//=== StructHashGeneratorPass.cpp - Renaming struct types based on content ===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the StructHashGeneratorPass pass.
///
//===----------------------------------------------------------------------===//

#include "StructHashGeneratorPass.h"
#include <llvm/ADT/Hashing.h>
#include <llvm/IR/TypeFinder.h>
#include <llvm/Support/raw_ostream.h>

PreservedAnalyses StructHashGeneratorPass::run (
        Module &Mod,
        llvm::AnalysisManager<llvm::Module> &Main) {
    TypeFinder Types;
    Types.run(Mod, true);

    for (auto *Ty : Types) {
        if (auto STy = dyn_cast<StructType>(Ty)) {
            if (!STy->getStructName().startswith("union.anon") &&
                !STy->getStructName().startswith("struct.anon"))
                continue;
            std::string TypeName = STy->getStructName().str();
            std::string TypeDump; llvm::raw_string_ostream DumpStrm(TypeDump);
            DumpStrm << *STy; TypeDump = DumpStrm.str();

            // Extract the type declaration without type name
            int pos = TypeDump.find("{");
            if (pos == std::string::npos)
                continue;
            std::string TypeDecl = TypeDump.substr(pos);
            std::string NewTypeName =
                    (STy->getStructName().startswith("union.anon") ?
                            "union.anon." : "struct.anon.") +
                    std::to_string(hash_value(TypeDecl));

            // Rename the type
            STy->setName(NewTypeName);
        }
    }

    return PreservedAnalyses();
}
