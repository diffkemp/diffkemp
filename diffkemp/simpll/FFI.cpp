//===----------------- FFI.cpp - C interface for SimpLL -------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definitions of C functions used for interacting with
/// DiffKemp.
///
//===----------------------------------------------------------------------===//

#include "FFI.h"
#include "Config.h"
#include "ModuleAnalysis.h"
#include "Output.h"
#include <cstring>
#include <llvm/Transforms/Utils/Cloning.h>
#include <unordered_map>

/// Map to store LLVMContext objects for modules with the module names as keys.
std::unordered_map<std::string, std::unique_ptr<LLVMContext>> ContextMap;

/// Map that manages unique pointers to modules.
std::unordered_map<Module *, std::unique_ptr<Module>> ModuleMap;

/// Simplifies modules and compares the specified functions.
/// Note: this variant takes ownership of the Module objects.
void runSimpLL(std::unique_ptr<Module> ModL,
               std::unique_ptr<Module> ModR,
               const char *ModLOut,
               const char *ModROut,
               const char *FunL,
               const char *FunR,
               struct config Conf,
               char *Output) {
    Config config(FunL,
                  FunR,
                  std::move(ModL),
                  std::move(ModR),
                  ModLOut,
                  ModROut,
                  Conf.CacheDir,
                  Conf.Variable,
                  Conf.OutputLlvmIR,
                  Conf.ControlFlowOnly,
                  Conf.PrintAsmDiffs,
                  Conf.PrintCallStacks,
                  Conf.Verbose,
                  Conf.VerboseMacros);

    OverallResult Result;
    processAndCompare(config, Result);

    std::string outputString = reportOutputToString(config, Result);
    strcpy(Output, outputString.c_str());
}

extern "C" {
void *loadModule(const char *Path) {
    SMDiagnostic err;
    ContextMap[Path] = std::make_unique<LLVMContext>();
    std::unique_ptr<Module> Mod = parseIRFile(Path, err, *ContextMap[Path]);
    Module *ModPtr = Mod.get();
    ModuleMap[ModPtr] = std::move(Mod);
    return (void *) ModPtr;
}

void freeModule(void *ModRaw) {
    Module *Mod = (Module *) ModRaw;
    std::string name = Mod->getName();
    ModuleMap.erase(Mod);
    ContextMap.erase(name);
}

/// Clones modules to get separate copies of them and runs the simplification
/// and comparison on the copies.
void cloneAndRunSimpLL(void *ModL,
                       void *ModR,
                       const char *ModLOut,
                       const char *ModROut,
                       const char *FunL,
                       const char *FunR,
                       struct config Conf,
                       char *Output) {
    runSimpLL(CloneModule(*((Module *) ModL)),
              CloneModule(*((Module *) ModR)),
              ModLOut,
              ModROut,
              FunL,
              FunR,
              Conf,
              Output);
}

/// Loads modules from the specified filers and runs the simplification and
/// comparison on the loaded objects, which are discarded after the comparison.
void parseAndRunSimpLL(const char *ModL,
                       const char *ModR,
                       const char *ModLOut,
                       const char *ModROut,
                       const char *FunL,
                       const char *FunR,
                       struct config Conf,
                       char *Output) {
    LLVMContext CtxL, CtxR;
    SMDiagnostic err;

    runSimpLL(parseIRFile(ModL, err, CtxL),
              parseIRFile(ModR, err, CtxR),
              ModLOut,
              ModROut,
              FunL,
              FunR,
              Conf,
              Output);
}

void shutdownSimpLL() {
    llvm_shutdown();
}
}
