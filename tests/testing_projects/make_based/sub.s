# File for testing of building of snapshots,
# this file should NOT be compiled to .ll and added to db file.
# Note: File was compiled from a C file and trimmed.
    .text
    .p2align 4
    .globl sub
    .type sub, @function
sub:
    endbr64
    movl %edi, %eax
    subl %esi, %eax
    ret

