#!/usr/bin/env python3
from cffi import FFI
from subprocess import check_output

ffibuilder = FFI()

ffibuilder.cdef("""
    struct config {
        const char *CacheDir;
        const char *Variable;
        int OutputLlvmIR;
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
""")

llvm_libs = ["irreader", "passes", "support"]
llvm_cflags = check_output(["llvm-config", "--cflags"])
llvm_ldflags = check_output(["llvm-config", "--libs"] + llvm_libs)

llvm_cflags = list(filter(lambda x: x != "",
                          llvm_cflags.decode("ascii").strip().split(" ")))
llvm_ldflags = list(filter(lambda x: x != "",
                           llvm_ldflags.decode("ascii").strip().split(" ")))

ffibuilder.set_source(
    "diffkemp.simpll._simpll", '#include <FFI.h>',
    libraries=['simpll-lib'],
    extra_compile_args=["-Idiffkemp/simpll"] + llvm_cflags,
    extra_link_args=["-Lbuild/diffkemp/simpll", "-lstdc++"] + llvm_ldflags)

if __name__ == "__main__":
    ffibuilder.compile()
