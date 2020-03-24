//===------------------ FFI.h - C interface for SimpLL --------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains declarations of C functions and structure types used for
/// interacting with DiffKemp.
///
//===----------------------------------------------------------------------===//

#ifdef __cplusplus
extern "C" {
#endif

struct config {
    const char *CacheDir;
    const char *Variable;
    int ControlFlowOnly;
    int PrintAsmDiffs;
    int PrintCallStacks;
    int Verbose;
    int VerboseMacros;
};

void runSimpLL(const char *ModL,
               const char *ModR,
               const char *ModLOut,
               const char *ModROut,
               const char *FunL,
               const char *FunR,
               struct config Conf,
               char *Output);

#ifdef __cplusplus
}
#endif
