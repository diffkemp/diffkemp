// File for testing of building of snapshots,
// this file should NOT be compiled to .ll and added to db file.
// Note: File was compiled from a C file, trimmed and edited.
    .text
    .p2align 4
    .globl mod
    .type mod, @function
mod:
    endbr64
    movl %edi, %eax
#ifdef MOD_SIGNED
    cltd
    idivl %esi
#else
    xorl %edx, %edx
    divl %esi
#endif
    movl %edx, %eax
    ret

